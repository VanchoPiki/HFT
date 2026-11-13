#!/usr/bin/env bash
# Deploys (or updates) the whole stack on an Ubuntu 24.04 server over SSH:
#
#   ./deploy/deploy.sh root@203.0.113.7
#
# First run: installs Docker from Ubuntu's repositories, enables the firewall (SSH, HTTP,
# HTTPS only), writes deploy/.env on the server with fresh secrets and prints the admin
# password once. Later runs just copy the code and rebuild.
set -euo pipefail

TARGET=${1:?usage: deploy/deploy.sh user@host}
HOST=${TARGET#*@}
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REMOTE=/opt/hft
SSH=(ssh -o StrictHostKeyChecking=accept-new "$TARGET")

[[ "$HOST" =~ ^[0-9]+(\.[0-9]+){3}$ ]] || { echo "expected user@IPv4, got $TARGET" >&2; exit 1; }

echo "== 1/4 server setup: Docker, firewall, swap"
"${SSH[@]}" bash -s <<'EOF'
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
if ! command -v docker >/dev/null; then
    apt-get update -qq
    apt-get install -y -qq docker.io docker-compose-v2 rsync ufw > /dev/null
    systemctl enable --now docker
fi
command -v rsync >/dev/null || apt-get install -y -qq rsync > /dev/null
# Compiling the bot needs ~2 GB per job: give small servers some swap.
if [[ $(free -m | awk '/Mem:/ {print $2}') -lt 3500 && ! -f /swapfile ]]; then
    fallocate -l 4G /swapfile && chmod 600 /swapfile && mkswap /swapfile > /dev/null && swapon /swapfile
    echo '/swapfile none swap sw 0 0' >> /etc/fstab
fi
ufw allow OpenSSH > /dev/null && ufw allow 80/tcp > /dev/null && ufw allow 443/tcp > /dev/null
ufw --force enable > /dev/null
mkdir -p /opt/hft
EOF

echo "== 2/4 copying the project"
rsync -az --delete \
    --exclude .git --exclude .env --exclude deploy/.env --exclude .DS_Store \
    --exclude 'core/build*' --exclude analytics/build --exclude analytics/.gradle \
    --exclude contracts/out --exclude contracts/cache \
    -e "ssh -o StrictHostKeyChecking=accept-new" "$ROOT/" "$TARGET:$REMOTE/"

echo "== 3/4 secrets (only created once, kept on the server)"
if "${SSH[@]}" test -f "$REMOTE/deploy/.env"; then
    echo "   deploy/.env already exists on the server, keeping it"
else
    # The RPC key goes from the local .env straight into the server's file, never to stdout.
    RPC_URL=$(grep -E '^ARBITRUM_RPC_URL=' "$ROOT/.env" 2>/dev/null | cut -d= -f2- || true)
    WS_URL=$(grep -E '^ARBITRUM_WS_URL=' "$ROOT/.env" 2>/dev/null | cut -d= -f2- || true)
    ADMIN_PASSWORD=$(LC_ALL=C tr -dc 'A-Za-z0-9' < /dev/urandom | head -c 20)
    # Values travel on stdin, not on a command line where `ps` would show them.
    {
        printf 'RPC_URL=%q\nWS_URL=%q\nADMIN_PASSWORD=%q\nSITE=%q\nREMOTE=%q\n' \
            "$RPC_URL" "$WS_URL" "$ADMIN_PASSWORD" "${HOST//./-}.sslip.io" "$REMOTE"
        cat <<'EOF'
set -euo pipefail
HASH=$(docker run --rm caddy:2.10 caddy hash-password --plaintext "$ADMIN_PASSWORD")
DB_PASSWORD=$(tr -dc 'A-Za-z0-9' < /dev/urandom | head -c 32)
umask 077
cat > "$REMOTE/deploy/.env" <<ENV
ARBITRUM_RPC_URL=$RPC_URL
ARBITRUM_WS_URL=$WS_URL
DB_PASSWORD=$DB_PASSWORD
SITE_ADDRESS=$SITE
ADMIN_USER=admin
ADMIN_PASSWORD_HASH='$HASH'
ENV
EOF
    } | "${SSH[@]}" bash -s
    echo "   admin login for the research buttons: admin / $ADMIN_PASSWORD  (shown only now)"
fi

echo "== 4/4 building and starting (first build of the bot takes several minutes)"
"${SSH[@]}" "cd $REMOTE && docker compose -f deploy/docker-compose.yml up -d --build --remove-orphans"
"${SSH[@]}" "cd $REMOTE && docker compose -f deploy/docker-compose.yml ps --format '{{.Service}}: {{.Status}}'"
echo
echo "dashboard: https://${HOST//./-}.sslip.io"
