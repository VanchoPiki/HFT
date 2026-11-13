# HFT: арбитражный бот для Uniswap V3 (Arbitrum)

Бот следит за пулами Uniswap V3, держит их точную копию у себя в памяти, находит
расхождения цен между пулами одной пары, считает оптимальный объём сделки, симулирует,
подписывает и отправляет транзакцию в собственный контракт-исполнитель, который делает
оба свопа атомарно через флеш-своп, без своего капитала.

```
core/        C++20: бот (hot path) — RPC/WebSocket, математика V3, стратегия, подпись, отправка
contracts/   Solidity + Foundry: ArbExecutor (флеш-своп в двух пулах) и интерфейсы
analytics/   Java 25 / Spring Boot 4: приём событий, PostgreSQL, дашборд, поиск пулов, бэктест
schema/      формат событий C++ → Java (ZeroMQ)
scripts/     demo_fork.sh — весь конвейер на локальном форке Arbitrum
docs/        PLAN.md — план и статус этапов
```

## Требования

```bash
brew install cmake ninja pkg-config googletest google-benchmark boost simdjson \
             secp256k1 zeromq cppzmq foundry
```

JDK 25 и Docker (для Postgres и тестов Java).

> ⚠️ Проект лежит в `~/Desktop`, а Рабочий стол синхронизируется с iCloud. iCloud
> портит объектные файлы прямо во время компиляции, поэтому C++ собирается в
> `~/.cache/hft-build`. Надёжнее всего перенести проект из синхронизируемой папки
> (например, в `~/dev/HFT`).

## Сборка и тесты

```bash
# C++: 92 теста (debug с ASan/UBSan) + бенчмарки (release)
cd core
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
cmake --preset release && cmake --build --preset release
~/.cache/hft-build/release/bench/hft_bench

# Solidity: 7 тестов на форке Arbitrum
cd contracts && forge test --fork-url https://arb1.arbitrum.io/rpc

# Java: 23 теста (поднимают свой Postgres через Testcontainers)
cd analytics && ./gradlew build
```

## Запуск

```bash
# 1. База и аналитика (дашборд: http://localhost:8080)
docker compose up -d postgres
cd analytics && ./gradlew bootRun

# 2. Бот на mainnet в режиме наблюдения (транзакции не отправляет)
cd core && ~/.cache/hft-build/release/hft_bot
#    --snapshot  только загрузить пулы и показать цены

# 3. Проверка точности: локальные котировки против Uniswap QuoterV2
~/.cache/hft-build/release/hft_verify --samples 8

# 4. Весь конвейер на локальном форке: кит двигает цену -> бот зарабатывает
./scripts/demo_fork.sh
```

RPC по умолчанию — публичный `https://arb1.arbitrum.io/rpc` (только HTTP, бот опрашивает
блоки раз в секунду). Для WebSocket-подписок и меньшей задержки впиши свой ключ в `.env`
(см. `.env.example`): `ARBITRUM_RPC_URL`, `ARBITRUM_WS_URL`.

## Сервер 24/7

Весь стек (бот в режиме наблюдения, Java, Postgres, Caddy с HTTPS) ставится на чистый
Ubuntu 24.04 одной командой, она же обновляет сервер после изменений:

```bash
./deploy/deploy.sh root@<IP>
```

Скрипт ставит Docker, включает файрвол (только SSH, HTTP, HTTPS), создаёт секреты на
сервере и выдаёт пароль администратора один раз. Дашборд: `https://<ip-через-дефисы>.sslip.io`.
Просмотр открыт всем, кнопки исследования (тратят лимит RPC) — по паролю.

## Исследование рынка (Java)

На дашборде в разделе «Исследование рынка» или через API:

```bash
curl -X POST localhost:8080/api/discovery/run            # все пулы между токенами из конфига
curl -X POST "localhost:8080/api/history/index?minutes=60" # история свопов
curl -X POST localhost:8080/api/backtest                 # прогон стратегии по истории
curl localhost:8080/api/bot-config > core/config/generated.json  # конфиг для бота
```

## Безопасность

- Исполнение выключено в `core/config/arbitrum.json` (`execution.enabled: false`).
- Конфиг сам называет переменные окружения, из которых читает RPC и ключ: форк-конфиг
  (`config/anvil.json`) не видит ключи mainnet из `.env`.
- Приватный ключ только в переменной окружения, в лог не пишется, в памяти затирается.
- Контракт откатывает сделку, если прибыль меньше `minProfit`: неудачный арбитраж
  стоит только газа.
