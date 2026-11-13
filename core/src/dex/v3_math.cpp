#include "dex/v3_math.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace hft::dex::v3 {
namespace {

const u256 kMaxU256 = ~u256(0);
const u256 kMaxU160 = (u256(1) << 160) - 1;

// TickMath.getSqrtRatioAtTick: 1/sqrt(1.0001)^(2^i) in Q128.128 for each bit i of |tick|.
const u256 kTickFactors[20] = {
    u256("0xfffcb933bd6fad37aa2d162d1a594001"), u256("0xfff97272373d413259a46990580e213a"),
    u256("0xfff2e50f5f656932ef12357cf3c7fdcc"), u256("0xffe5caca7e10e4e61c3624eaa0941cd0"),
    u256("0xffcb9843d60f6159c9db58835c926644"), u256("0xff973b41fa98c081472e6896dfb254c0"),
    u256("0xff2ea16466c96a3843ec78b326b52861"), u256("0xfe5dee046a99a2a811c461f1969c3053"),
    u256("0xfcbe86c7900a88aedcffc83b479aa3a4"), u256("0xf987a7253ac413176f2b074cf7815e54"),
    u256("0xf3392b0822b70005940c7a398e4b70f3"), u256("0xe7159475a2c29b7443b29c7fa6e889d9"),
    u256("0xd097f3bdfd2022b8845ad8f792aa5825"), u256("0xa9f746462d870fdf8a65dc1f90e061e5"),
    u256("0x70d869a156d2a1b890bb3df62baf32f7"), u256("0x31be135f97d08fd981231505542fcfa6"),
    u256("0x9aa508b5b7a84e1c677de54f3e99bc9"),  u256("0x5d6af8dedb81196699c329225ee604"),
    u256("0x2216e584f5fa1ea926041bedfe98"),     u256("0x48a170391f7dc42444e8fa2"),
};

u256 checked_u256(const eth::u512& v) {
    if (v > eth::u512(kMaxU256)) {
        throw MathError("v3: mulDiv overflow");
    }
    return static_cast<u256>(v);
}

u256 to_u160(const u256& v) {
    if (v > kMaxU160) {
        throw MathError("v3: uint160 overflow");
    }
    return v;
}

// SqrtPriceMath.getNextSqrtPriceFromAmount0RoundingUp with add = true.
u256 next_from_amount0_up(const u256& sqrt_p, const u256& liquidity, const u256& amount) {
    if (amount == 0) {
        return sqrt_p;
    }
    const u256 numerator1 = liquidity << 96;
    const u256 product = amount * sqrt_p;  // wraps like unchecked EVM math
    if (product / amount == sqrt_p) {
        const u256 denominator = numerator1 + product;
        if (denominator >= numerator1) {
            return mul_div_rounding_up(numerator1, sqrt_p, denominator);
        }
    }
    const u256 sum = numerator1 / sqrt_p + amount;
    if (sum < amount) {
        throw MathError("v3: amount0 add overflow");
    }
    return div_rounding_up(numerator1, sum);
}

// SqrtPriceMath.getNextSqrtPriceFromAmount1RoundingDown with add = true.
u256 next_from_amount1_down(const u256& sqrt_p, const u256& liquidity, const u256& amount) {
    const u256 quotient =
        amount <= kMaxU160 ? (amount << 96) / liquidity : mul_div(amount, kQ96, liquidity);
    return to_u160(sqrt_p + quotient);
}

}  // namespace

const u256 kMinSqrtRatio("4295128739");
const u256 kMaxSqrtRatio("1461446703485210103287273052203988822378723970342");
const u256 kQ96 = u256(1) << 96;

u256 mul_div(const u256& a, const u256& b, const u256& denominator) {
    if (denominator == 0) {
        throw MathError("v3: mulDiv by zero");
    }
    return checked_u256(eth::u512(a) * b / denominator);
}

u256 mul_div_rounding_up(const u256& a, const u256& b, const u256& denominator) {
    if (denominator == 0) {
        throw MathError("v3: mulDiv by zero");
    }
    const eth::u512 product = eth::u512(a) * b;
    u256 result = checked_u256(product / denominator);
    if (product % denominator != 0) {
        if (result == kMaxU256) {
            throw MathError("v3: mulDivRoundingUp overflow");
        }
        ++result;
    }
    return result;
}

u256 div_rounding_up(const u256& x, const u256& y) {
    return x / y + (x % y != 0 ? 1 : 0);
}

u256 sqrt_ratio_at_tick(int tick) {
    const unsigned abs_tick = static_cast<unsigned>(tick < 0 ? -tick : tick);
    if (abs_tick > static_cast<unsigned>(kMaxTick)) {
        throw MathError("v3: tick out of range");
    }
    u256 ratio = (abs_tick & 1u) != 0 ? kTickFactors[0] : u256(1) << 128;
    for (unsigned bit = 1; bit < 20; ++bit) {
        if ((abs_tick & (1u << bit)) != 0) {
            ratio = (ratio * kTickFactors[bit]) >> 128;
        }
    }
    if (tick > 0) {
        ratio = kMaxU256 / ratio;
    }
    // Q128.128 -> Q64.96, rounding up so that tick_at_sqrt_ratio stays consistent.
    return (ratio >> 32) + ((ratio & 0xffffffff) == 0 ? 0 : 1);
}

int tick_at_sqrt_ratio(const u256& sqrt_price_x96) {
    if (sqrt_price_x96 < kMinSqrtRatio || sqrt_price_x96 >= kMaxSqrtRatio) {
        throw MathError("v3: sqrt price out of range");
    }
    // The floating-point estimate is within a tick of the answer; exact integer
    // comparisons then pin down the greatest tick with sqrt_ratio_at_tick(tick) <= price.
    const double sqrt_p = static_cast<double>(sqrt_price_x96) / 0x1p96;
    int tick = static_cast<int>(std::floor(2.0 * std::log(sqrt_p) / std::log(1.0001)));
    tick = std::clamp(tick, kMinTick, kMaxTick - 1);
    while (tick > kMinTick && sqrt_ratio_at_tick(tick) > sqrt_price_x96) {
        --tick;
    }
    while (tick < kMaxTick - 1 && sqrt_ratio_at_tick(tick + 1) <= sqrt_price_x96) {
        ++tick;
    }
    return tick;
}

u256 add_delta(const u256& liquidity, const i256& delta) {
    const i256 result = static_cast<i256>(liquidity) + delta;
    if (result < 0) {
        throw MathError("v3: liquidity underflow");
    }
    return static_cast<u256>(result);
}

u256 next_sqrt_price_from_input(const u256& sqrt_price_x96, const u256& liquidity,
                                const u256& amount_in, bool zero_for_one) {
    if (sqrt_price_x96 == 0 || liquidity == 0) {
        throw MathError("v3: zero price or liquidity");
    }
    return zero_for_one ? next_from_amount0_up(sqrt_price_x96, liquidity, amount_in)
                        : next_from_amount1_down(sqrt_price_x96, liquidity, amount_in);
}

u256 amount0_delta(u256 sqrt_a, u256 sqrt_b, const u256& liquidity, bool round_up) {
    if (sqrt_a > sqrt_b) {
        std::swap(sqrt_a, sqrt_b);
    }
    if (sqrt_a == 0) {
        throw MathError("v3: zero sqrt price");
    }
    const u256 numerator1 = liquidity << 96;
    const u256 numerator2 = sqrt_b - sqrt_a;
    return round_up ? div_rounding_up(mul_div_rounding_up(numerator1, numerator2, sqrt_b), sqrt_a)
                    : mul_div(numerator1, numerator2, sqrt_b) / sqrt_a;
}

u256 amount1_delta(u256 sqrt_a, u256 sqrt_b, const u256& liquidity, bool round_up) {
    if (sqrt_a > sqrt_b) {
        std::swap(sqrt_a, sqrt_b);
    }
    return round_up ? mul_div_rounding_up(liquidity, sqrt_b - sqrt_a, kQ96)
                    : mul_div(liquidity, sqrt_b - sqrt_a, kQ96);
}

SwapStep compute_swap_step(const u256& sqrt_current, const u256& sqrt_target,
                           const u256& liquidity, const u256& amount_remaining,
                           std::uint32_t fee_pips) {
    const bool zero_for_one = sqrt_current >= sqrt_target;
    SwapStep step;

    const u256 remaining_less_fee = mul_div(amount_remaining, 1'000'000 - fee_pips, 1'000'000);
    step.amount_in = zero_for_one ? amount0_delta(sqrt_target, sqrt_current, liquidity, true)
                                  : amount1_delta(sqrt_current, sqrt_target, liquidity, true);
    if (remaining_less_fee >= step.amount_in) {
        step.sqrt_price_next = sqrt_target;
    } else {
        step.sqrt_price_next =
            next_sqrt_price_from_input(sqrt_current, liquidity, remaining_less_fee, zero_for_one);
    }

    const bool reached_target = step.sqrt_price_next == sqrt_target;
    if (zero_for_one) {
        if (!reached_target) {
            step.amount_in = amount0_delta(step.sqrt_price_next, sqrt_current, liquidity, true);
        }
        step.amount_out = amount1_delta(step.sqrt_price_next, sqrt_current, liquidity, false);
    } else {
        if (!reached_target) {
            step.amount_in = amount1_delta(sqrt_current, step.sqrt_price_next, liquidity, true);
        }
        step.amount_out = amount0_delta(sqrt_current, step.sqrt_price_next, liquidity, false);
    }

    if (!reached_target) {
        step.fee_amount = amount_remaining - step.amount_in;  // the remainder is all fee
    } else {
        step.fee_amount = mul_div_rounding_up(step.amount_in, fee_pips, 1'000'000 - fee_pips);
    }
    return step;
}

}  // namespace hft::dex::v3
