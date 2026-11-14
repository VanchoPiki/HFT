#include "eth/keccak.hpp"

#include <gtest/gtest.h>

#include <string>

#include "eth/abi.hpp"
#include "eth/hex.hpp"

namespace hft::eth {
namespace {

TEST(KeccakTest, EmptyInput) {
    EXPECT_EQ(to_hex(keccak256("")),
              "0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470");
}

TEST(KeccakTest, ShortInput) {
    EXPECT_EQ(to_hex(keccak256("abc")),
              "0x4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45");
}

TEST(KeccakTest, ExactlyOneBlock) {
    EXPECT_EQ(to_hex(keccak256(std::string(136, 'b'))),
              "0x121b76d0b19f3c2c7632310b92c54cddd59d16a6b5aafe84696426f10e5733bf");
}

TEST(KeccakTest, MultipleBlocks) {
    EXPECT_EQ(to_hex(keccak256(std::string(300, 'a'))),
              "0x5b7e0e47a96f32a88b4f14ca177982790807c40e1a105742ba0fc1babe1ef826");
}

TEST(KeccakTest, FunctionSelectors) {
    EXPECT_EQ(to_hex(abi::selector("slot0()")), "0x3850c7bd");
    EXPECT_EQ(to_hex(abi::selector("transfer(address,uint256)")), "0xa9059cbb");
    EXPECT_EQ(
        to_hex(abi::selector("quoteExactInputSingle((address,address,uint256,uint24,uint160))")),
        "0xc6a5026a");
}

TEST(KeccakTest, EventTopic) {
    EXPECT_EQ(to_hex(abi::event_topic("Swap(address,address,int256,int256,uint160,uint128,int24)")),
              "0xc42079f94a6350d7e6235f29174924f928cc2ac818eb64fed8004e115fbcca67");
}

}
}
