#!/usr/bin/env bash
# Stage 9: the whole pipeline on a local fork of Arbitrum One.
#
#   anvil fork -> deploy ArbExecutor -> start the bot (WebSocket feed, execution on)
#   -> a "whale" dumps WETH into the 0.3% WETH/USDC pool
#   -> the bot sees the Swap event, sizes the arbitrage, simulates, signs, sends
#   -> the transaction is mined and the executor contract holds the profit.
#
# Needs: Foundry (anvil, forge, cast) and the release build of the bot:
#   cd core && cmake --preset release && cmake --build --preset release
# Optional, to watch it on the dashboard (http://localhost:8080):
#   docker compose up -d postgres && (cd analytics && ./gradlew bootRun)
#
# Everything uses anvil's public test keys on a throwaway local fork: no real funds.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "$ROOT/.env" ]]; then
    set -a # RPC key from .env (simple KEY=VALUE lines)
    source "$ROOT/.env"
    set +a
fi
FORK_URL="${ARBITRUM_RPC_URL:-https://arb1.arbitrum.io/rpc}"
# The public endpoint throttles on its own; with a provider key keep anvil's built-in
# budget of upstream calls (it matches free plans) to avoid 429s.
ANVIL_LIMIT=(--no-rate-limit)
[[ -n "${ARBITRUM_RPC_URL:-}" ]] && ANVIL_LIMIT=()
RPC=http://127.0.0.1:8545
BOT="${HFT_BOT:-$HOME/.cache/hft-build/release/hft_bot}"
WHALE_WETH="${WHALE_WETH:-300}"
LOGS="${TMPDIR:-/tmp}/hft-demo"
mkdir -p "$LOGS"

# anvil's pre-funded accounts #0 (the bot) and #1 (the whale)
BOT_KEY=0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80
WHALE_KEY=0x59c6995e998f97a5a0044966f0945389dc9e86dae88c7a8412f4603b6b78690d
WHALE=0x70997970C51812dc3A010C7d01b50e0d17dc79C8

WETH=0x82aF49447D8a07e3bd95BD0d56f35241523fBab1
USDC=0xaf88d065e77c8cC2239327C5EDb3A432268e5831
ROUTER=0x68b3465833fb72A70ecDF485E0e4C7bD8665Fc45 # Uniswap SwapRouter02

cleanup() {
    [[ -n "${BOT_PID:-}" ]] && kill "$BOT_PID" 2>/dev/null || true
    [[ -n "${ANVIL_PID:-}" ]] && kill "$ANVIL_PID" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT TERM # turn signals into a normal exit so the EXIT trap cleans up

wait_for() { # wait_for <seconds> <description> <command...>
    local seconds=$1 what=$2
    shift 2
    for ((i = 0; i < seconds * 2; i++)); do
        if "$@" >/dev/null 2>&1; then return 0; fi
        sleep 0.5
    done
    echo "timed out waiting for $what" >&2
    return 1
}

[[ -x "$BOT" ]] || { echo "bot binary not found at $BOT (build the release preset first)" >&2; exit 1; }
for port in 8545 5555; do
    if lsof -nP -iTCP:$port -sTCP:LISTEN >/dev/null 2>&1; then
        echo "port $port is busy (another anvil or bot still running?):" >&2
        lsof -nP -iTCP:$port -sTCP:LISTEN >&2
        exit 1
    fi
done

echo "== 1/5 anvil: forking Arbitrum (${FORK_URL%%/v2/*}...)"  # never print the key
anvil --fork-url "$FORK_URL" --port 8545 ${ANVIL_LIMIT[@]+"${ANVIL_LIMIT[@]}"} > "$LOGS/anvil.log" 2>&1 &
ANVIL_PID=$!
wait_for 60 "anvil" cast chain-id --rpc-url "$RPC"
echo "   fork block $(cast block-number --rpc-url "$RPC"), chain id $(cast chain-id --rpc-url "$RPC")"

echo "== 2/5 deploying ArbExecutor"
if ! forge create --root "$ROOT/contracts" src/ArbExecutor.sol:ArbExecutor \
    --rpc-url "$RPC" --private-key "$BOT_KEY" --broadcast > "$LOGS/deploy.log" 2>&1; then
    echo "deployment failed:" >&2
    tail -20 "$LOGS/deploy.log" >&2
    exit 1
fi
EXECUTOR=$(sed -n 's/^Deployed to: //p' "$LOGS/deploy.log")
echo "   executor at $EXECUTOR"

echo "== 3/5 starting the bot (logs: $LOGS/bot.log)"
# exec: $! must be the bot itself, not a wrapper shell, or cleanup would leave it running.
(cd "$ROOT/core" && ANVIL_PRIVATE_KEY="$BOT_KEY" HFT_EXECUTOR="$EXECUTOR" \
    exec "$BOT" --config config/anvil.json > "$LOGS/bot.log" 2>&1) &
BOT_PID=$!
# The first start is slow: anvil fetches every pool slot and tick from upstream on demand.
wait_for 600 "the bot to subscribe" grep -q "ws feed: connected" "$LOGS/bot.log"
grep -E "tracking|synced" "$LOGS/bot.log" | sed 's/^/   /'

echo "== 4/5 whale dumps $WHALE_WETH WETH into the 0.3% WETH/USDC pool"
AMOUNT=$(cast to-wei "$WHALE_WETH")
cast send "$WETH" "deposit()" --value "$AMOUNT" --private-key "$WHALE_KEY" --rpc-url "$RPC" > /dev/null
cast send "$WETH" "approve(address,uint256)" "$ROUTER" "$AMOUNT" --private-key "$WHALE_KEY" --rpc-url "$RPC" > /dev/null
cast send "$ROUTER" "exactInputSingle((address,address,uint24,address,uint256,uint256,uint160))" \
    "($WETH,$USDC,3000,$WHALE,$AMOUNT,0,0)" --private-key "$WHALE_KEY" --rpc-url "$RPC" > /dev/null
echo "   done: WETH is now cheap in the 0.3% pool"

echo "== 5/5 waiting for the bot"
wait_for 60 "the arbitrage to be mined" grep -q "mined in block" "$LOGS/bot.log"
grep -E "OPP #|executor:" "$LOGS/bot.log" | sed 's/^[0-9:.]* //' | sed 's/^/   /'

PROFIT=$(cast call "$WETH" "balanceOf(address)(uint256)" "$EXECUTOR" --rpc-url "$RPC" | awk '{print $1}')
echo
echo "executor contract now holds $(cast from-wei "$PROFIT") WETH of profit"
echo "dashboard (if analytics is running): http://localhost:8080"
