#pragma once

#include <cstdint>
#include <string>

#include "dex/market.hpp"
#include "strategy/two_pool_arb.hpp"

namespace hft::telemetry {

std::int64_t now_ms();

void set_run_id(std::int64_t run_id);
std::int64_t run_id();

std::string pool_snapshot_event(const dex::Market& market, const dex::V3Pool& pool,
                                std::uint64_t block);
std::string opportunity_event(const dex::Market& market, const strategy::Opportunity& opp);
std::string heartbeat_event(std::uint64_t chain_id, std::uint64_t block, std::size_t pools,
                            const std::string& mode, std::uint64_t dropped_events);

}
