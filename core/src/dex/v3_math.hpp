#pragma once

// Uniswap V3 core math, ported line by line from v3-core (FullMath, TickMath,
// SqrtPriceMath, SwapMath, LiquidityMath) so local quotes match the chain to the wei.
// Only the exact-input direction is implemented: that is all the bot needs.

#include <cstdint>
#include <stdexcept>

#include "eth/u256.hpp"

namespace hft::dex::v3 {

using eth::i256;
using eth::u256;

constexpr int kMinTick = -887272;
constexpr int kMaxTick = 887272;
extern const u256 kMinSqrtRatio;  // sqrt_ratio_at_tick(kMinTick)
extern const u256 kMaxSqrtRatio;  // sqrt_ratio_at_tick(kMaxTick)
extern const u256 kQ96;

// A condition under which the Solidity code would revert.
struct MathError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// FullMath / UnsafeMath: a * b / denominator with a 512-bit intermediate product.
u256 mul_div(const u256& a, const u256& b, const u256& denominator);
u256 mul_div_rounding_up(const u256& a, const u256& b, const u256& denominator);
u256 div_rounding_up(const u256& x, const u256& y);

// TickMath
u256 sqrt_ratio_at_tick(int tick);
int tick_at_sqrt_ratio(const u256& sqrt_price_x96);  // greatest tick with ratio(tick) <= price

// LiquidityMath
u256 add_delta(const u256& liquidity, const i256& delta);

// SqrtPriceMath
u256 next_sqrt_price_from_input(const u256& sqrt_price_x96, const u256& liquidity,
                                const u256& amount_in, bool zero_for_one);
u256 amount0_delta(u256 sqrt_a, u256 sqrt_b, const u256& liquidity, bool round_up);
u256 amount1_delta(u256 sqrt_a, u256 sqrt_b, const u256& liquidity, bool round_up);

// SwapMath.computeSwapStep for exact input. fee_pips: 500 = 0.05%.
struct SwapStep {
    u256 sqrt_price_next;
    u256 amount_in;
    u256 amount_out;
    u256 fee_amount;
};
SwapStep compute_swap_step(const u256& sqrt_current, const u256& sqrt_target,
                           const u256& liquidity, const u256& amount_remaining,
                           std::uint32_t fee_pips);

}  // namespace hft::dex::v3
