#include "telemetry/events.hpp"

#include <chrono>
#include <format>

namespace hft::telemetry {
namespace {

std::int64_t g_run_id = 0;  // set once at startup, before any event is built

}  // namespace

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void set_run_id(std::int64_t run_id) { g_run_id = run_id; }

std::int64_t run_id() { return g_run_id; }

std::string pool_snapshot_event(const dex::Market& market, const dex::V3Pool& pool,
                                std::uint64_t block) {
    return std::format(
        R"({{"type":"pool_snapshot","ts_ms":{},"block":{},"pool":"{}","pair":"{}","fee":{},)"
        R"("price":{:.10g},"tick":{},"sqrt_price_x96":"{}","liquidity":"{}"}})",
        now_ms(), block, pool.info().address.to_hex(), market.pair_name(pool), pool.info().fee,
        market.human_price(pool), pool.tick(), pool.sqrt_price_x96().str(),
        pool.liquidity().str());
}

std::string opportunity_event(const dex::Market& market, const strategy::Opportunity& opp) {
    const auto& in = opp.token_in;
    auto human = [&](const auto& raw) { return market.human_amount(in, static_cast<double>(raw)); };
    return std::format(
        R"({{"type":"opportunity","ts_ms":{},"run":{},"id":{},"block":{},"pair":"{}",)"
        R"("pool_a":"{}","fee_a":{},"pool_b":"{}","fee_b":{},"token_in":"{}","symbol_in":"{}",)"
        R"("amount_in":"{}","amount_out":"{}","gross_profit":"{}","gas_cost":"{}","net_profit":"{}",)"
        R"("amount_in_human":{:.10g},"net_profit_human":{:.10g},"net_profit_weth":{:.10g},)"
        R"("compute_us":{}}})",
        now_ms(), g_run_id, opp.id, opp.block, market.pair_name(*opp.pool_a),
        opp.pool_a->info().address.to_hex(), opp.pool_a->info().fee,
        opp.pool_b->info().address.to_hex(), opp.pool_b->info().fee, in.to_hex(),
        market.token(in).symbol, opp.amount_in.str(), opp.amount_out.str(), opp.gross_profit.str(),
        opp.gas_cost.str(), opp.net_profit.str(), human(opp.amount_in), human(opp.net_profit),
        opp.net_profit_weth,
        std::chrono::duration_cast<std::chrono::microseconds>(opp.compute_time).count());
}

std::string heartbeat_event(std::uint64_t chain_id, std::uint64_t block, std::size_t pools,
                            const std::string& mode, std::uint64_t dropped_events) {
    return std::format(
        R"({{"type":"heartbeat","ts_ms":{},"run":{},"chain_id":{},"block":{},"pools":{},)"
        R"("mode":"{}","dropped_events":{}}})",
        now_ms(), g_run_id, chain_id, block, pools, mode, dropped_events);
}

}  // namespace hft::telemetry
