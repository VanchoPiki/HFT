#include "dex/v3_pool.hpp"

#include <gtest/gtest.h>

namespace hft::dex {
namespace {

using eth::i256;
using eth::u256;

const u256 kQ96 = u256(1) << 96;

V3Pool make_pool() {
    V3Pool pool(PoolInfo{.fee = 500, .tick_spacing = 10});
    pool.reset(kQ96, 0, 1'000'000, /*block=*/100);
    return pool;
}

SwapEvent swap_to(const u256& sqrt_price, int tick, const u256& liquidity) {
    return SwapEvent{.sqrt_price_x96 = sqrt_price, .liquidity = liquidity, .tick = tick};
}

TEST(V3PoolTest, PriceOfSqrtOneIsOne) { EXPECT_DOUBLE_EQ(make_pool().raw_price(), 1.0); }

TEST(V3PoolTest, IgnoresEventsAlreadyInTheLoadedState) {
    V3Pool pool = make_pool();
    EXPECT_FALSE(pool.apply(swap_to(2 * kQ96, 100, 5), 100, 3));  // same block as the RPC read
    EXPECT_EQ(pool.sqrt_price_x96(), kQ96);
}

TEST(V3PoolTest, AppliesNewerSwapsOnceAndInOrder) {
    V3Pool pool = make_pool();
    EXPECT_TRUE(pool.apply(swap_to(2 * kQ96, 13863, 7), 101, 0));
    EXPECT_EQ(pool.sqrt_price_x96(), 2 * kQ96);
    EXPECT_EQ(pool.tick(), 13863);
    EXPECT_EQ(pool.liquidity(), 7);
    EXPECT_DOUBLE_EQ(pool.raw_price(), 4.0);

    EXPECT_FALSE(pool.apply(swap_to(kQ96, 0, 1), 101, 0));  // duplicate
    EXPECT_TRUE(pool.apply(swap_to(kQ96, 0, 1), 101, 1));
    EXPECT_EQ(pool.block(), 101u);
}

TEST(V3PoolTest, MintAndBurnInRangeChangeActiveLiquidityAndTicks) {
    V3Pool pool = make_pool();
    pool.set_known_tick_range(-2560, 2559);

    ASSERT_TRUE(pool.apply(LiquidityEvent{-60, 60, i256(1000)}, 101, 0));
    EXPECT_EQ(pool.liquidity(), 1'001'000);
    ASSERT_EQ(pool.ticks().size(), 2u);
    EXPECT_EQ(pool.ticks().at(-60).liquidity_net, 1000);
    EXPECT_EQ(pool.ticks().at(60).liquidity_net, -1000);
    EXPECT_EQ(pool.ticks().at(60).liquidity_gross, 1000);

    ASSERT_TRUE(pool.apply(LiquidityEvent{-60, 60, i256(-1000)}, 101, 1));
    EXPECT_EQ(pool.liquidity(), 1'000'000);
    EXPECT_TRUE(pool.ticks().empty());  // gross went to zero: ticks uninitialized
}

TEST(V3PoolTest, MintOutOfRangeOnlyTouchesTicks) {
    V3Pool pool = make_pool();
    pool.set_known_tick_range(-2560, 2559);
    ASSERT_TRUE(pool.apply(LiquidityEvent{100, 200, i256(50)}, 101, 0));
    EXPECT_EQ(pool.liquidity(), 1'000'000);
    EXPECT_EQ(pool.ticks().size(), 2u);
}

TEST(V3PoolTest, WithoutTickDataOnlyActiveLiquidityIsTracked) {
    V3Pool pool = make_pool();
    ASSERT_TRUE(pool.apply(LiquidityEvent{-60, 60, i256(1000)}, 101, 0));
    EXPECT_EQ(pool.liquidity(), 1'001'000);
    EXPECT_TRUE(pool.ticks().empty());
}

}  // namespace
}  // namespace hft::dex
