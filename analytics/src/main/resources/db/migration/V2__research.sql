-- Stage 10: pool discovery, indexed swap history and backtests (all computed on the Java side).

create table discovered_pools (
    address        varchar(42) primary key,
    token0         varchar(42)      not null,
    token1         varchar(42)      not null,
    symbol0        varchar(16)      not null,
    symbol1        varchar(16)      not null,
    decimals0      integer          not null,
    decimals1      integer          not null,
    fee            integer          not null,
    tick_spacing   integer          not null,
    liquidity      numeric(78, 0)   not null,
    sqrt_price_x96 numeric(78, 0)   not null,
    price          double precision not null,
    discovered_at  timestamptz      not null
);

create table swap_events (
    id             bigserial primary key,
    pool           varchar(42)    not null,
    block          bigint         not null,
    log_index      integer        not null,
    tx_hash        varchar(66)    not null,
    amount0        numeric(78, 0) not null,
    amount1        numeric(78, 0) not null,
    sqrt_price_x96 numeric(78, 0) not null,
    liquidity      numeric(78, 0) not null,
    tick           integer        not null,
    unique (pool, block, log_index)
);
create index swap_events_block on swap_events (block, log_index);

create table backtest_runs (
    id                bigserial primary key,
    created_at        timestamptz      not null,
    from_block        bigint           not null,
    to_block          bigint           not null,
    swap_events       integer          not null,
    min_profit_weth   double precision not null,
    gas_units         bigint           not null,
    gas_price_gwei    double precision not null,
    opportunities     integer          not null,
    profitable_blocks integer          not null,
    total_profit_weth double precision not null,
    best_profit_weth  double precision not null
);

create table backtest_opportunities (
    id             bigserial primary key,
    run_id         bigint           not null references backtest_runs (id) on delete cascade,
    block          bigint           not null,
    pair           varchar(32)      not null,
    pool_a         varchar(42)      not null,
    fee_a          integer          not null,
    pool_b         varchar(42)      not null,
    fee_b          integer          not null,
    token_in       varchar(16)      not null,
    amount_in      double precision not null,
    profit_weth    double precision not null
);
create index backtest_opportunities_run on backtest_opportunities (run_id);
