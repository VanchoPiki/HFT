#pragma once

#include <cstdint>

#include "app/config.hpp"
#include "dex/market.hpp"
#include "dex/uniswap_abi.hpp"
#include "net/json_rpc.hpp"

namespace hft::app {

dex::Market discover_market(const Config& cfg, net::JsonRpcClient& rpc,
                            const dex::UniswapV3Abi& abi);

void sync_market(const Config& cfg, net::JsonRpcClient& rpc, const dex::UniswapV3Abi& abi,
                 dex::Market& market, std::uint64_t block);

}
