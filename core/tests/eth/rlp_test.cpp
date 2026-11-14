#include "eth/rlp.hpp"

#include <gtest/gtest.h>

#include <string_view>

#include "eth/hex.hpp"

namespace hft::eth::rlp {
namespace {

std::vector<std::uint8_t> str(std::string_view s) { return {s.begin(), s.end()}; }

TEST(RlpTest, ByteStrings) {
    EXPECT_EQ(to_hex(encode_bytes({})), "0x80");
    EXPECT_EQ(to_hex(encode_bytes(*from_hex("0x00"))), "0x00");
    EXPECT_EQ(to_hex(encode_bytes(*from_hex("0x0f"))), "0x0f");
    EXPECT_EQ(to_hex(encode_bytes(*from_hex("0x0400"))), "0x820400");
    EXPECT_EQ(to_hex(encode_bytes(str("dog"))), "0x83646f67");
}

TEST(RlpTest, LongString) {
    const auto lorem = str("Lorem ipsum dolor sit amet, consectetur adipisicing elit");
    EXPECT_EQ(to_hex(encode_bytes(lorem)),
              "0xb8384c6f72656d20697073756d20646f6c6f722073697420616d65742c20636f6e7365637465"
              "747572206164697069736963696e6720656c6974");
}

TEST(RlpTest, Lists) {
    EXPECT_EQ(to_hex(List().encode()), "0xc0");
    EXPECT_EQ(to_hex(List().add_bytes(str("cat")).add_bytes(str("dog")).encode()),
              "0xc88363617483646f67");
}

TEST(RlpTest, NestedLists) {
    const List empty;
    const List one = List().add_list(empty);
    const List two = List().add_list(empty).add_list(one);
    EXPECT_EQ(to_hex(List().add_list(empty).add_list(one).add_list(two).encode()),
              "0xc7c0c1c0c3c0c1c0");
}

TEST(RlpTest, IntegersAreMinimalBigEndian) {
    EXPECT_EQ(to_hex(List().add_uint(std::uint64_t{0}).encode()), "0xc180");
    EXPECT_EQ(to_hex(List().add_uint(std::uint64_t{15}).encode()), "0xc10f");
    EXPECT_EQ(to_hex(List().add_uint(std::uint64_t{1024}).encode()), "0xc3820400");
    EXPECT_EQ(to_hex(List().add_uint(u256{1024}).encode()), "0xc3820400");
    EXPECT_EQ(to_hex(List().add_uint(u256{0}).encode()), "0xc180");
}

}
}
