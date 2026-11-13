#include "eth/abi.hpp"

#include <gtest/gtest.h>

#include "eth/hex.hpp"

namespace hft::eth::abi {
namespace {

// Expected encodings come from `cast abi-encode` / `cast calldata`.

const Address kWeth = *Address::from_hex("0x82aF49447D8a07e3bd95BD0d56f35241523fBab1");
const Address kUsdc = *Address::from_hex("0xaf88d065e77c8cC2239327C5EDb3A432268e5831");

std::vector<std::uint8_t> bytes(std::string_view hex) { return *from_hex(hex); }

TEST(AbiEncoderTest, StaticTypes) {
    Word b32{};
    b32[28] = 0xde;
    b32[29] = 0xad;
    b32[30] = 0xbe;
    b32[31] = 0xef;
    const auto encoded = Encoder()
                             .add_address(kWeth)
                             .add_uint(123456789)
                             .add_int(-887272)
                             .add_bool(true)
                             .add_bytes32(b32)
                             .finish();
    EXPECT_EQ(to_hex(encoded),
              "0x00000000000000000000000082af49447d8a07e3bd95bd0d56f35241523fbab1"
              "00000000000000000000000000000000000000000000000000000000075bcd15"
              "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff27618"
              "0000000000000000000000000000000000000000000000000000000000000001"
              "00000000000000000000000000000000000000000000000000000000deadbeef");
}

TEST(AbiEncoderTest, DynamicBytes) {
    const auto encoded =
        Encoder().add_uint(1).add_bytes(bytes("0xdeadbeef")).add_uint(2).finish();
    EXPECT_EQ(to_hex(encoded),
              "0x0000000000000000000000000000000000000000000000000000000000000001"
              "0000000000000000000000000000000000000000000000000000000000000060"
              "0000000000000000000000000000000000000000000000000000000000000002"
              "0000000000000000000000000000000000000000000000000000000000000004"
              "deadbeef00000000000000000000000000000000000000000000000000000000");
}

TEST(AbiEncoderTest, CalldataWithSelector) {
    const auto calldata =
        Encoder(selector("transfer(address,uint256)")).add_address(kUsdc).add_uint(1000000).finish();
    EXPECT_EQ(to_hex(calldata),
              "0xa9059cbb000000000000000000000000af88d065e77c8cc2239327c5edb3a432268e5831"
              "00000000000000000000000000000000000000000000000000000000000f4240");
}

TEST(AbiReaderTest, ReadsStaticTypes) {
    const auto data = Encoder().add_address(kWeth).add_int(-60).add_bool(true).add_uint(7).finish();
    Reader r(data);
    EXPECT_EQ(r.read_address(), kWeth);
    EXPECT_EQ(r.read_int(), -60);
    EXPECT_TRUE(r.read_bool());
    EXPECT_EQ(r.read_u64(), 7u);
    EXPECT_THROW(r.read_uint(), Error);
}

TEST(AbiReaderTest, ReadsDynamicBytes) {
    const auto data = Encoder().add_uint(1).add_bytes(bytes("0xdeadbeef")).add_uint(2).finish();
    Reader r(data);
    EXPECT_EQ(r.read_uint(), 1);
    EXPECT_EQ(r.read_bytes(), bytes("0xdeadbeef"));
    EXPECT_EQ(r.read_uint(), 2);
}

// The shape TickLens.getPopulatedTicksInWord returns: (int24 tick, int128 net, uint128 gross)[]
TEST(AbiReaderTest, ReadsArrayOfStaticTuples) {
    const auto data = bytes(
        "0x0000000000000000000000000000000000000000000000000000000000000020"
        "0000000000000000000000000000000000000000000000000000000000000002"
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc4"
        "00000000000000000000000000000000000000000000000000000000000003e8"
        "00000000000000000000000000000000000000000000000000000000000003e8"
        "000000000000000000000000000000000000000000000000000000000000003c"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc18"
        "00000000000000000000000000000000000000000000000000000000000003e8");
    Reader r(data);
    Reader array = r.read_dynamic();
    ASSERT_EQ(array.read_u64(), 2u);
    EXPECT_EQ(array.read_int(), -60);
    EXPECT_EQ(array.read_int(), 1000);
    EXPECT_EQ(array.read_uint(), 1000);
    EXPECT_EQ(array.read_int(), 60);
    EXPECT_EQ(array.read_int(), -1000);
    EXPECT_EQ(array.read_uint(), 1000);
}

TEST(AbiReaderTest, RejectsBadOffsets) {
    const auto data = Encoder().add_uint(1u << 20).finish();
    Reader r(data);
    EXPECT_THROW(r.read_dynamic(), Error);
}

}  // namespace
}  // namespace hft::eth::abi
