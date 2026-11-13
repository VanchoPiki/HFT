#pragma once

#include <cstdint>

#include "app/config.hpp"
#include "dex/market.hpp"
#include "dex/uniswap_abi.hpp"
#include "net/json_rpc.hpp"

namespace hft::app {

// Loads token decimals, discovers the configured pools via the factory and reads their
// static info. Pool state is not loaded yet.
dex::Market discover_market(const Config& cfg, net::JsonRpcClient& rpc,
                            const dex::UniswapV3Abi& abi);

// slot0, liquidity and initialized ticks of every pool, all as of one block.
void sync_market(const Config& cfg, net::JsonRpcClient& rpc, const dex::UniswapV3Abi& abi,
                 dex::Market& market, std::uint64_t block);

}  // namespace hft::app
