#pragma once

#include <cstdint>
#include <stdexcept>

#include "eth/u256.hpp"

namespace hft::dex::v3 {

using eth::i256;
using eth::u256;

constexpr int kMinTick = -887272;
constexpr int kMaxTick = 887272;
extern const u256 kMinSqrtRatio;
extern const u256 kMaxSqrtRatio;
extern const u256 kQ96;

struct MathError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

u256 mul_div(const u256& a, const u256& b, const u256& denominator);
u256 mul_div_rounding_up(const u256& a, const u256& b, const u256& denominator);
u256 div_rounding_up(const u256& x, const u256& y);

u256 sqrt_ratio_at_tick(int tick);
int tick_at_sqrt_ratio(const u256& sqrt_price_x96);

u256 add_delta(const u256& liquidity, const i256& delta);

u256 next_sqrt_price_from_input(const u256& sqrt_price_x96, const u256& liquidity,
                                const u256& amount_in, bool zero_for_one);
u256 amount0_delta(u256 sqrt_a, u256 sqrt_b, const u256& liquidity, bool round_up);
u256 amount1_delta(u256 sqrt_a, u256 sqrt_b, const u256& liquidity, bool round_up);

struct SwapStep {
    u256 sqrt_price_next;
    u256 amount_in;
    u256 amount_out;
    u256 fee_amount;
};
SwapStep compute_swap_step(const u256& sqrt_current, const u256& sqrt_target,
                           const u256& liquidity, const u256& amount_remaining,
                           std::uint32_t fee_pips);

}
