#include "dex/v3_events.hpp"

#include <gtest/gtest.h>

#include "eth/abi.hpp"
#include "eth/hex.hpp"

namespace hft::dex {
namespace {

using eth::i256;
using eth::u256;

eth::Hash tick_topic(int tick) { return eth::to_word(eth::from_signed(i256(tick))); }

class V3EventsTest : public ::testing::Test {
protected:
    UniswapV3Abi abi = UniswapV3Abi::load(HFT_ABI_DIR);
    V3EventDecoder decoder{abi};
};

TEST_F(V3EventsTest, DecodesRealSwap) {
    net::Log log;
    log.topics = {abi.swap_topic, eth::Hash{}, eth::Hash{}};
    log.data = *eth::from_hex(
        "0x0000000000000000000000000000000000000000000000001147a2bb08dfa66b"
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffff52538de2"
        "000000000000000000000000000000000000000000032bc9a2321a8e40000000"
        "00000000000000000000000000000000000000000000000035e4ee3ff449d831"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffcf7b0");
    const auto swap = std::get<SwapEvent>(decoder.decode(log));
    EXPECT_EQ(swap.amount0, i256("1245142746182690411"));
    EXPECT_EQ(swap.amount1, i256(-2913759774LL));
    EXPECT_EQ(swap.sqrt_price_x96, u256("3833558700599279129460736"));
    EXPECT_EQ(swap.liquidity, u256("3883490737149040689"));
    EXPECT_EQ(swap.tick, -198736);
}

TEST_F(V3EventsTest, DecodesMintAsPositiveDelta) {
    net::Log log;
    log.topics = {abi.mint_topic, eth::Hash{}, tick_topic(-887220), tick_topic(-60)};
    log.data = eth::abi::Encoder().add_address({}).add_uint(5000).add_uint(1).add_uint(2).finish();
    const auto e = std::get<LiquidityEvent>(decoder.decode(log));
    EXPECT_EQ(e.tick_lower, -887220);
    EXPECT_EQ(e.tick_upper, -60);
    EXPECT_EQ(e.liquidity_delta, 5000);
}

TEST_F(V3EventsTest, DecodesBurnAsNegativeDelta) {
    net::Log log;
    log.topics = {abi.burn_topic, eth::Hash{}, tick_topic(-60), tick_topic(60)};
    log.data = eth::abi::Encoder().add_uint(700).add_uint(1).add_uint(2).finish();
    const auto e = std::get<LiquidityEvent>(decoder.decode(log));
    EXPECT_EQ(e.tick_lower, -60);
    EXPECT_EQ(e.tick_upper, 60);
    EXPECT_EQ(e.liquidity_delta, -700);
}

TEST_F(V3EventsTest, IgnoresOtherEvents) {
    net::Log log;
    log.topics = {eth::keccak256("Collect(address,address,int24,int24,uint128,uint128)")};
    EXPECT_TRUE(std::holds_alternative<std::monostate>(decoder.decode(log)));
}

TEST_F(V3EventsTest, MalformedSwapThrows) {
    net::Log log;
    log.topics = {abi.swap_topic};
    log.data = std::vector<std::uint8_t>(64, 0);
    EXPECT_THROW(decoder.decode(log), eth::abi::Error);
}

}
}
