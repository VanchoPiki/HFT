#include "dex/market.hpp"

#include <gtest/gtest.h>

namespace hft::dex {
namespace {

const eth::Address kWeth = *eth::Address::from_hex("0x82aF49447D8a07e3bd95BD0d56f35241523fBab1");
const eth::Address kUsdc = *eth::Address::from_hex("0xaf88d065e77c8cC2239327C5EDb3A432268e5831");
const eth::Address kArb = *eth::Address::from_hex("0x912CE59144191C1204E64559FE8253a0e49E6548");

eth::Address pool_address(std::uint8_t n) {
    eth::Address a;
    a.bytes[19] = n;
    return a;
}

TEST(MarketTest, GroupsPoolsByPair) {
    Market m;
    m.add_token({kWeth, "WETH", 18});
    m.add_token({kUsdc, "USDC", 6});
    m.add_token({kArb, "ARB", 18});
    V3Pool& a = m.add_pool({pool_address(1), kWeth, kUsdc, 500, 10});
    V3Pool& b = m.add_pool({pool_address(2), kWeth, kArb, 500, 10});
    V3Pool& c = m.add_pool({pool_address(3), kWeth, kUsdc, 3000, 60});

    ASSERT_EQ(m.pair_groups().size(), 2u);
    EXPECT_EQ(m.group_of(a), (std::vector<V3Pool*>{&a, &c}));
    EXPECT_EQ(m.group_of(b).size(), 1u);
    EXPECT_EQ(m.find_pool(pool_address(3)), &c);
    EXPECT_EQ(m.find_pool(pool_address(9)), nullptr);
    EXPECT_EQ(m.pair_name(a), "WETH/USDC");
}

TEST(MarketTest, HumanPriceAppliesDecimals) {
    Market m;
    m.add_token({kWeth, "WETH", 18});
    m.add_token({kUsdc, "USDC", 6});
    V3Pool& pool = m.add_pool({pool_address(1), kWeth, kUsdc, 500, 10});
    pool.reset(eth::u256("3833558700599279129460736"), -198736, 1, 1);
    EXPECT_NEAR(m.human_price(pool), 2341.2, 0.1);
}

TEST(MarketTest, FeeLabels) {
    EXPECT_EQ(fee_label(100), "0.01%");
    EXPECT_EQ(fee_label(500), "0.05%");
    EXPECT_EQ(fee_label(3000), "0.3%");
    EXPECT_EQ(fee_label(10000), "1%");
}

}
}
