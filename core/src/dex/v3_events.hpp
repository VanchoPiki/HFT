#pragma once

#include <variant>

#include "dex/uniswap_abi.hpp"
#include "net/rpc_types.hpp"

namespace hft::dex {

// Swap carries the pool's complete new price state, so applying it is just an assignment.
struct SwapEvent {
    eth::i256 amount0;
    eth::i256 amount1;
    eth::u256 sqrt_price_x96;
    eth::u256 liquidity;
    int tick = 0;
};

// Mint (positive delta) or Burn (negative delta) of liquidity in [tick_lower, tick_upper).
struct LiquidityEvent {
    int tick_lower = 0;
    int tick_upper = 0;
    eth::i256 liquidity_delta;
};

using PoolEvent = std::variant<std::monostate, SwapEvent, LiquidityEvent>;

class V3EventDecoder {
public:
    explicit V3EventDecoder(const UniswapV3Abi& abi);

    // monostate for logs that do not change price or liquidity (Collect, Flash, ...).
    // Throws eth::abi::Error on malformed logs.
    PoolEvent decode(const net::Log& log) const;

private:
    eth::Hash swap_;
    eth::Hash mint_;
    eth::Hash burn_;
};

}  // namespace hft::dex
