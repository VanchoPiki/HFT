#include "eth/address.hpp"

#include <gtest/gtest.h>

#include <unordered_set>

namespace hft::eth {
namespace {

// Checksums from `cast to-check-sum-address`.
TEST(AddressTest, Eip55Checksum) {
    EXPECT_EQ(Address::from_hex("0x82af49447d8a07e3bd95bd0d56f35241523fbab1")->to_checksum(),
              "0x82aF49447D8a07e3bd95BD0d56f35241523fBab1");
    EXPECT_EQ(Address::from_hex("0xaf88d065e77c8cc2239327c5edb3a432268e5831")->to_checksum(),
              "0xaf88d065e77c8cC2239327C5EDb3A432268e5831");
}

TEST(AddressTest, ParsesAnyCaseAndPrintsLowercase) {
    const auto a = Address::from_hex("0x82aF49447D8a07e3bd95BD0d56f35241523fBab1");
    ASSERT_TRUE(a);
    EXPECT_EQ(a->to_hex(), "0x82af49447d8a07e3bd95bd0d56f35241523fbab1");
}

TEST(AddressTest, RejectsWrongLength) {
    EXPECT_FALSE(Address::from_hex("0x82af49447d8a07e3bd95bd0d56f35241523fba"));
    EXPECT_FALSE(Address::from_hex(""));
}

// Uniswap orders pool tokens by address: WETH (0x82..) is token0, USDC (0xaf..) is token1.
TEST(AddressTest, OrdersLikeSolidity) {
    const auto weth = *Address::from_hex("0x82af49447d8a07e3bd95bd0d56f35241523fbab1");
    const auto usdc = *Address::from_hex("0xaf88d065e77c8cc2239327c5edb3a432268e5831");
    EXPECT_LT(weth, usdc);
}

TEST(AddressTest, WorksAsHashKey) {
    std::unordered_set<Address, AddressHash> set;
    set.insert(*Address::from_hex("0x82af49447d8a07e3bd95bd0d56f35241523fbab1"));
    set.insert(*Address::from_hex("0x82AF49447D8A07E3BD95BD0D56F35241523FBAB1"));
    EXPECT_EQ(set.size(), 1u);
    EXPECT_TRUE(Address{}.is_zero());
}

}  // namespace
}  // namespace hft::eth
