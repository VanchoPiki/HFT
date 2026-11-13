-- Every history indexing run covers one contiguous block range. A backtest must stay inside
-- one of them: across a gap, a pool's last known swap is stale and fakes dislocations.
create table indexed_ranges (
    id         bigserial primary key,
    from_block bigint      not null,
    to_block   bigint      not null,
    events     integer     not null,
    indexed_at timestamptz not null
);
