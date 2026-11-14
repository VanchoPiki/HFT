#pragma once

#include <variant>

#include "dex/uniswap_abi.hpp"
#include "net/rpc_types.hpp"

namespace hft::dex {

struct SwapEvent {
    eth::i256 amount0;
    eth::i256 amount1;
    eth::u256 sqrt_price_x96;
    eth::u256 liquidity;
    int tick = 0;
};

struct LiquidityEvent {
    int tick_lower = 0;
    int tick_upper = 0;
    eth::i256 liquidity_delta;
};

using PoolEvent = std::variant<std::monostate, SwapEvent, LiquidityEvent>;

class V3EventDecoder {
public:
    explicit V3EventDecoder(const UniswapV3Abi& abi);

    PoolEvent decode(const net::Log& log) const;

private:
    eth::Hash swap_;
    eth::Hash mint_;
    eth::Hash burn_;
};

}
