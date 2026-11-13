#include "eth/u256.hpp"

#include <gtest/gtest.h>

#include "eth/hex.hpp"

namespace hft::eth {
namespace {

const u256 kMax = ~u256{0};

TEST(U256Test, WordRoundTrip) {
    for (const u256& v : {u256{0}, u256{1}, u256{42161}, u256{1} << 255, kMax}) {
        EXPECT_EQ(u256_from_be(to_word(v)), v);
    }
}

TEST(U256Test, WordIsBigEndian) {
    const Word w = to_word(u256{0x0102});
    EXPECT_EQ(w[30], 0x01);
    EXPECT_EQ(w[31], 0x02);
    EXPECT_EQ(w[0], 0x00);
}

TEST(U256Test, FromShortBigEndian) {
    const std::vector<std::uint8_t> bytes{0x04, 0x00};
    EXPECT_EQ(u256_from_be(bytes), u256{1024});
    EXPECT_EQ(u256_from_be(std::span<const std::uint8_t>{}), u256{0});
}

TEST(U256Test, MinimalBigEndian) {
    EXPECT_TRUE(to_be_minimal(0).empty());
    EXPECT_EQ(to_be_minimal(1024), (std::vector<std::uint8_t>{0x04, 0x00}));
    EXPECT_EQ(to_be_minimal(kMax).size(), 32u);
}

TEST(U256Test, ParsesQuantities) {
    EXPECT_EQ(parse_quantity("0x0"), u256{0});
    EXPECT_EQ(parse_quantity("0xa4b1"), u256{42161});
    EXPECT_EQ(parse_quantity("0x00ff"), u256{255});
    EXPECT_EQ(parse_quantity("0x" + std::string(64, 'f')), kMax);
    EXPECT_EQ(parse_quantity_u64("0x1ef5c116"), 0x1ef5c116u);
}

TEST(U256Test, RejectsBadQuantities) {
    EXPECT_EQ(parse_quantity("0x"), std::nullopt);
    EXPECT_EQ(parse_quantity("a4b1"), std::nullopt);
    EXPECT_EQ(parse_quantity("0xzz"), std::nullopt);
    EXPECT_EQ(parse_quantity("0x1" + std::string(64, '0')), std::nullopt);
    EXPECT_EQ(parse_quantity_u64("0x10000000000000000"), std::nullopt);
}

TEST(U256Test, FormatsQuantities) {
    EXPECT_EQ(to_quantity(0), "0x0");
    EXPECT_EQ(to_quantity(42161), "0xa4b1");
    EXPECT_EQ(to_quantity(kMax), "0x" + std::string(64, 'f'));
}

TEST(U256Test, TwosComplement) {
    // -887272 (MIN_TICK) as int24 ABI word, from `cast abi-encode "f(int24)" -- -887272`.
    const auto bits =
        *parse_quantity("0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffffff27618");
    EXPECT_EQ(to_signed(bits), i256{-887272});
    EXPECT_EQ(from_signed(i256{-887272}), bits);

    EXPECT_EQ(from_signed(i256{-1}), kMax);
    EXPECT_EQ(to_signed(kMax), i256{-1});
    EXPECT_EQ(to_signed(u256{5}), i256{5});

    const u256 min_bits = u256{1} << 255;
    EXPECT_EQ(from_signed(to_signed(min_bits)), min_bits);
    EXPECT_LT(to_signed(min_bits), 0);
}

}  // namespace
}  // namespace hft::eth
