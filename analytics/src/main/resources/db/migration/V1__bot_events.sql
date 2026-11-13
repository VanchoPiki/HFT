-- Events published by the C++ bot (schema/events.md). Token amounts are raw integers
-- (wei-like units) and can exceed 64 bits, hence numeric(78, 0) = any uint256.

create table pool_snapshots (
    id             bigserial primary key,
    ts             timestamptz      not null,
    block          bigint           not null,
    pool           varchar(42)      not null,
    pair           varchar(32)      not null,
    fee            integer          not null,
    price          double precision not null,
    tick           integer          not null,
    sqrt_price_x96 numeric(78, 0)   not null,
    liquidity      numeric(78, 0)   not null
);
create index pool_snapshots_pair_ts on pool_snapshots (pair, ts);
create index pool_snapshots_pool_ts on pool_snapshots (pool, ts);

create table opportunities (
    id               bigserial primary key,
    run_id           bigint           not null,
    opportunity_id   bigint           not null,
    ts               timestamptz      not null,
    block            bigint           not null,
    pair             varchar(32)      not null,
    pool_a           varchar(42)      not null,
    fee_a            integer          not null,
    pool_b           varchar(42)      not null,
    fee_b            integer          not null,
    token_in         varchar(42)      not null,
    symbol_in        varchar(16)      not null,
    amount_in        numeric(78, 0)   not null,
    amount_out       numeric(78, 0)   not null,
    gross_profit     numeric(78, 0)   not null,
    gas_cost         numeric(78, 0)   not null,
    net_profit       numeric(78, 0)   not null,
    amount_in_human  double precision not null,
    net_profit_human double precision not null,
    net_profit_weth  double precision not null,
    compute_us       bigint           not null,
    unique (run_id, opportunity_id)
);
create index opportunities_ts on opportunities (ts);

-- One row per execution attempt; tx_result events fill in the mined_* columns.
create table executions (
    id                   bigserial primary key,
    run_id               bigint         not null,
    opportunity_id       bigint         not null,
    ts                   timestamptz    not null,
    status               varchar(32)    not null,
    tx_hash              varchar(66) unique,
    nonce                bigint,
    gas_limit            bigint,
    max_fee_per_gas      numeric(78, 0),
    error                text,
    simulate_us          bigint,
    sign_us              bigint,
    send_us              bigint,
    success              boolean,
    mined_block          bigint,
    gas_used             bigint,
    effective_gas_price  numeric(78, 0),
    realized_profit      numeric(78, 0),
    realized_profit_weth double precision,
    mined_at             timestamptz
);
create index executions_ts on executions (ts);
