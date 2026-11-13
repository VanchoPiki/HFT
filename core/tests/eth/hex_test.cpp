#include "eth/hex.hpp"

#include <gtest/gtest.h>

#include <array>

namespace hft::eth {
namespace {

using Bytes = std::vector<std::uint8_t>;

TEST(HexTest, EncodesLowercaseWithPrefix) {
    const Bytes bytes{0x00, 0x0f, 0xa4, 0xb1, 0xff};
    EXPECT_EQ(to_hex(bytes), "0x000fa4b1ff");
}

TEST(HexTest, EncodesEmptyAsBarePrefix) { EXPECT_EQ(to_hex(Bytes{}), "0x"); }

TEST(HexTest, DecodesWithAndWithoutPrefixInAnyCase) {
    const Bytes expected{0xde, 0xad, 0xbe, 0xef};
    EXPECT_EQ(from_hex("0xdeadbeef"), expected);
    EXPECT_EQ(from_hex("0XDEADBEEF"), expected);
    EXPECT_EQ(from_hex("DeAdBeEf"), expected);
}

TEST(HexTest, DecodesEmpty) {
    EXPECT_EQ(from_hex("0x"), Bytes{});
    EXPECT_EQ(from_hex(""), Bytes{});
}

TEST(HexTest, RejectsOddLength) { EXPECT_EQ(from_hex("0x123"), std::nullopt); }

TEST(HexTest, RejectsNonHexCharacters) {
    EXPECT_EQ(from_hex("0xzz"), std::nullopt);
    EXPECT_EQ(from_hex("0x0x12"), std::nullopt);
    EXPECT_EQ(from_hex("0x12 4"), std::nullopt);
}

TEST(HexTest, RoundTripsAllByteValues) {
    Bytes all(256);
    for (std::size_t i = 0; i < all.size(); ++i) {
        all[i] = static_cast<std::uint8_t>(i);
    }
    EXPECT_EQ(from_hex(to_hex(all)), all);
}

// WETH on Arbitrum One, as checksummed on block explorers.
TEST(HexTest, DecodesAddressIntoFixedBuffer) {
    std::array<std::uint8_t, 20> address{};
    ASSERT_TRUE(from_hex("0x82aF49447D8a07e3bd95BD0d56f35241523fBab1", address));
    EXPECT_EQ(to_hex(address), "0x82af49447d8a07e3bd95bd0d56f35241523fbab1");
}

TEST(HexTest, FixedBufferRejectsLengthMismatch) {
    std::array<std::uint8_t, 20> address{};
    EXPECT_FALSE(from_hex("0x82af49447d8a07e3bd95bd0d56f35241523fba", address));    // 19 bytes
    EXPECT_FALSE(from_hex("0x82af49447d8a07e3bd95bd0d56f35241523fbab100", address));  // 21 bytes
}

}  // namespace
}  // namespace hft::eth
