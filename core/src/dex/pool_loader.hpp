#pragma once

#include <span>
#include <utility>
#include <vector>

#include "dex/market.hpp"
#include "dex/uniswap_abi.hpp"
#include "net/json_rpc.hpp"

namespace hft::dex {

class PoolLoader {
public:
    PoolLoader(net::JsonRpcClient& rpc, const UniswapV3Abi& abi) : rpc_(rpc), abi_(abi) {}

    std::vector<eth::Address> discover(const eth::Address& factory,
                                       std::span<const std::pair<eth::Address, eth::Address>> pairs,
                                       std::span<const std::uint32_t> fee_tiers);

    std::vector<PoolInfo> load_info(std::span<const eth::Address> pools);
    void load_decimals(std::span<Token> tokens);

    void load_state(std::span<V3Pool* const> pools, std::uint64_t block);

    void load_ticks(const eth::Address& tick_lens, std::span<V3Pool* const> pools,
                    int words_each_side, std::uint64_t block);

private:
    std::vector<net::CallResult> run(std::span<const net::CallRequest> calls, std::uint64_t block);

    net::JsonRpcClient& rpc_;
    const UniswapV3Abi& abi_;
};

}
