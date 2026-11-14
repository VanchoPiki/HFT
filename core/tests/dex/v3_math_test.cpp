#include "dex/v3_math.hpp"

#include <gtest/gtest.h>

#include <random>

namespace hft::dex::v3 {
namespace {

const u256 kMaxU256 = ~u256(0);

TEST(TickMathTest, KnownRatios) {
    EXPECT_EQ(sqrt_ratio_at_tick(0), kQ96);
    EXPECT_EQ(sqrt_ratio_at_tick(kMinTick), kMinSqrtRatio);
    EXPECT_EQ(sqrt_ratio_at_tick(kMaxTick), kMaxSqrtRatio);
    EXPECT_THROW(sqrt_ratio_at_tick(kMaxTick + 1), MathError);
    EXPECT_THROW(sqrt_ratio_at_tick(kMinTick - 1), MathError);
}

TEST(TickMathTest, TickAtRatioIsTheInverse) {
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> any_tick(kMinTick + 1, kMaxTick - 1);
    std::vector<int> ticks{kMinTick + 1, -198736, -1, 0, 1, 276324, kMaxTick - 1};
    for (int i = 0; i < 300; ++i) {
        ticks.push_back(any_tick(rng));
    }
    for (int t : ticks) {
        const u256 ratio = sqrt_ratio_at_tick(t);
        EXPECT_EQ(tick_at_sqrt_ratio(ratio), t);
        EXPECT_EQ(tick_at_sqrt_ratio(ratio - 1), t - 1);
        EXPECT_LT(ratio, sqrt_ratio_at_tick(t + 1));
    }
}

TEST(TickMathTest, RejectsPricesOutOfRange) {
    EXPECT_THROW(tick_at_sqrt_ratio(kMinSqrtRatio - 1), MathError);
    EXPECT_THROW(tick_at_sqrt_ratio(kMaxSqrtRatio), MathError);
    EXPECT_EQ(tick_at_sqrt_ratio(kMinSqrtRatio), kMinTick);
}

TEST(FullMathTest, MulDivUsesA512BitProduct) {
    EXPECT_EQ(mul_div(kMaxU256, kMaxU256, kMaxU256), kMaxU256);
    EXPECT_EQ(mul_div(kQ96, kQ96 * 3, kQ96), kQ96 * 3);
    EXPECT_THROW(mul_div(kMaxU256, kMaxU256, 1), MathError);
    EXPECT_THROW(mul_div(1, 1, 0), MathError);
}

TEST(FullMathTest, Rounding) {
    EXPECT_EQ(mul_div(7, 3, 2), 10);
    EXPECT_EQ(mul_div_rounding_up(7, 3, 2), 11);
    EXPECT_EQ(mul_div_rounding_up(6, 3, 2), 9);
    EXPECT_EQ(div_rounding_up(10, 3), 4);
    EXPECT_EQ(div_rounding_up(9, 3), 3);
}

TEST(SqrtPriceMathTest, RoundingUpAddsAtMostOneWei) {
    const u256 a = sqrt_ratio_at_tick(-200000);
    const u256 b = sqrt_ratio_at_tick(-199000);
    const u256 liquidity("3883490737149040689");
    for (auto delta : {amount0_delta, amount1_delta}) {
        const u256 down = delta(a, b, liquidity, false);
        const u256 up = delta(a, b, liquidity, true);
        EXPECT_TRUE(up == down || up == down + 1);
        EXPECT_EQ(delta(b, a, liquidity, false), down);
    }
}

TEST(SwapMathTest, PartialStepSpendsTheWholeInput) {
    const u256 amount("1000000000000000");
    const SwapStep step =
        compute_swap_step(kQ96, sqrt_ratio_at_tick(-1000), u256("1000000000000000000"), amount, 500);
    EXPECT_NE(step.sqrt_price_next, sqrt_ratio_at_tick(-1000));
    EXPECT_LT(step.sqrt_price_next, kQ96);
    EXPECT_EQ(step.amount_in + step.fee_amount, amount);
    EXPECT_GT(step.amount_out, 0);
    EXPECT_LT(step.amount_out, step.amount_in);
}

TEST(SwapMathTest, FullStepStopsAtTheTarget) {
    const u256 target = sqrt_ratio_at_tick(10);
    const SwapStep step = compute_swap_step(kQ96, target, u256("1000000000000000000"),
                                            u256("1000000000000000000000"), 3000);
    EXPECT_EQ(step.sqrt_price_next, target);
    EXPECT_EQ(step.amount_in, amount1_delta(kQ96, target, u256("1000000000000000000"), true));
    EXPECT_EQ(step.fee_amount, mul_div_rounding_up(step.amount_in, 3000, 1'000'000 - 3000));
}

}
}
