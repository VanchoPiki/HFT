#include "strategy/two_pool_arb.hpp"

#include <gtest/gtest.h>

#include <cmath>

#include "dex/v3_math.hpp"

namespace hft::strategy {
namespace {

using eth::u256;

const eth::Address kToken0 = *eth::Address::from_hex("0x1000000000000000000000000000000000000000");
const eth::Address kToken1 = *eth::Address::from_hex("0x2000000000000000000000000000000000000000");

u256 sqrt_price_x96(double price) {
    return u256(static_cast<std::uint64_t>(std::sqrt(price) * 0x1p62)) << 34;
}

eth::Address pool_address(std::uint8_t n) {
    eth::Address a;
    a.bytes[19] = n;
    return a;
}

// A pool with one full-range position: constant product over the whole price range.
dex::V3Pool& add_full_range_pool(dex::Market& m, std::uint8_t n, std::uint32_t fee, double price,
                                 const u256& liquidity) {
    dex::V3Pool& pool = m.add_pool({pool_address(n), kToken0, kToken1, fee, 10});
    const u256 sqrt_p = sqrt_price_x96(price);
    pool.reset(sqrt_p, dex::v3::tick_at_sqrt_ratio(sqrt_p), liquidity, 1);
    pool.set_known_tick_range(dex::v3::kMinTick, dex::v3::kMaxTick);
    pool.set_tick(-887270, eth::i256(liquidity), liquidity);
    pool.set_tick(887270, -eth::i256(liquidity), liquidity);
    return pool;
}

class TwoPoolArbTest : public ::testing::Test {
protected:
    TwoPoolArbTest() {
        market.add_token({kToken0, "T0", 18});
        market.add_token({kToken1, "T1", 18});
    }

    double exact_profit(const dex::V3Pool& a, const dex::V3Pool& b, bool start0, const u256& in) {
        const auto qa = a.quote_exact_input(start0, in);
        const auto qb = b.quote_exact_input(!start0, qa->amount_out);
        return static_cast<double>(qb->amount_out) - static_cast<double>(in);
    }

    dex::Market market;
    StrategyConfig config{.min_profit_weth = 0, .gas_units = 0, .max_input_weth = 1e30};
};

TEST_F(TwoPoolArbTest, NoOpportunityWhenSpreadIsBelowFees) {
    const u256 l("1000000000000000000000");
    auto& cheap = add_full_range_pool(market, 1, 3000, 1.000, l);
    auto& rich = add_full_range_pool(market, 2, 3000, 1.004, l);  // 0.4% < 0.3% + 0.3%
    TwoPoolArb arb(market, kToken0, config);
    for (bool start0 : {true, false}) {
        EXPECT_FALSE(arb.evaluate(cheap, rich, start0));
        EXPECT_FALSE(arb.evaluate(rich, cheap, start0));
    }
}

TEST_F(TwoPoolArbTest, FindsTheOptimalSize) {
    const u256 l("1000000000000000000000");
    auto& cheap = add_full_range_pool(market, 1, 500, 1.00, l);
    auto& rich = add_full_range_pool(market, 2, 500, 1.02, l);
    TwoPoolArb arb(market, kToken0, config);

    // token0 is expensive in `rich`: sell it there, buy it back in `cheap`.
    const auto opp = arb.evaluate(rich, cheap, /*start_with_token0=*/true);
    ASSERT_TRUE(opp);
    EXPECT_GT(opp->gross_profit, 0);
    EXPECT_EQ(opp->token_in, kToken0);
    EXPECT_EQ(opp->amount_out - opp->amount_in, static_cast<u256>(opp->gross_profit));

    // Brute force over a fine geometric grid must not beat the optimizer.
    double best_grid = 0;
    for (double x = 1e15; x < 1e23; x *= 1.01) {
        best_grid = std::max(best_grid, exact_profit(rich, cheap, true, u256(x)));
    }
    EXPECT_GE(static_cast<double>(opp->gross_profit), best_grid * (1 - 1e-6));

    // The wrong direction loses money at any size.
    EXPECT_FALSE(arb.evaluate(cheap, rich, true));
}

// For two constant-product pools the optimum has a closed form; the search must land on it.
TEST_F(TwoPoolArbTest, MatchesClosedFormForConstantProductPools) {
    const u256 l("1000000000000000000000");
    auto& cheap = add_full_range_pool(market, 1, 500, 1.00, l);
    auto& rich = add_full_range_pool(market, 2, 3000, 1.03, l);
    TwoPoolArb arb(market, kToken0, config);
    const auto opp = arb.evaluate(rich, cheap, true);
    ASSERT_TRUE(opp);

    const double l_d = 1e21;
    const double s_rich = std::sqrt(1.03);
    const double ga = 1 - 0.003;
    const double gb = 1 - 0.0005;
    // rich: token0 in -> token1 out; cheap: token1 in -> token0 out
    const double a = ga * gb * (l_d * s_rich) * (l_d / 1.0);
    const double b = (l_d / s_rich) * (l_d * 1.0);
    const double c = ga * (l_d * 1.0 + gb * l_d * s_rich);
    const double x_star = (std::sqrt(a * b) - b) / c;
    EXPECT_NEAR(static_cast<double>(opp->amount_in) / x_star, 1.0, 1e-4);
}

TEST_F(TwoPoolArbTest, OnPoolChangedAppliesMinProfitAndGas) {
    const u256 l("1000000000000000000000");
    add_full_range_pool(market, 1, 500, 1.00, l);
    auto& rich = add_full_range_pool(market, 2, 500, 1.02, l);

    TwoPoolArb arb(market, kToken0, config);
    const auto opp = arb.on_pool_changed(rich, 42);
    ASSERT_TRUE(opp);
    EXPECT_EQ(opp->block, 42u);
    EXPECT_GT(opp->net_profit_weth, 0);

    StrategyConfig strict = config;
    strict.min_profit_weth = opp->net_profit_weth * 2;
    TwoPoolArb picky(market, kToken0, strict);
    EXPECT_FALSE(picky.on_pool_changed(rich, 42));

    StrategyConfig costly = config;
    costly.gas_units = 1'000'000'000;
    TwoPoolArb gas_heavy(market, kToken0, costly);
    gas_heavy.set_gas_price(u256("1000000000000000"));  // gas eats the whole profit
    EXPECT_FALSE(gas_heavy.on_pool_changed(rich, 42));
}

TEST_F(TwoPoolArbTest, RespectsTheInputCap) {
    const u256 l("1000000000000000000000");
    add_full_range_pool(market, 1, 500, 1.00, l);
    auto& rich = add_full_range_pool(market, 2, 500, 1.02, l);
    StrategyConfig capped = config;
    capped.max_input_weth = 1.0;  // 1e18 raw units of token0 (= WETH here)
    TwoPoolArb arb(market, kToken0, capped);
    const auto opp = arb.on_pool_changed(rich, 1);
    ASSERT_TRUE(opp);
    EXPECT_LE(opp->amount_in, u256("1000000000000000000"));
}

}  // namespace
}  // namespace hft::strategy
