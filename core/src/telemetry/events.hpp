#pragma once

// JSON events for the analytics service. The format is documented in schema/events.md;
// big integers are decimal strings, human-readable values are plain numbers.

#include <cstdint>
#include <string>

#include "dex/market.hpp"
#include "strategy/two_pool_arb.hpp"

namespace hft::telemetry {

std::int64_t now_ms();

// Identifies one bot run (its start time): opportunity ids restart from 1 on every run.
void set_run_id(std::int64_t run_id);
std::int64_t run_id();

std::string pool_snapshot_event(const dex::Market& market, const dex::V3Pool& pool,
                                std::uint64_t block);
std::string opportunity_event(const dex::Market& market, const strategy::Opportunity& opp);
std::string heartbeat_event(std::uint64_t chain_id, std::uint64_t block, std::size_t pools,
                            const std::string& mode, std::uint64_t dropped_events);

}  // namespace hft::telemetry
