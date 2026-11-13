#include "net/rpc_types.hpp"

#include <gtest/gtest.h>

#include "eth/hex.hpp"

namespace hft::net {
namespace {

// A real Swap log of the WETH/USDC 0.05% pool on Arbitrum (eth_getLogs).
constexpr std::string_view kSwapLog = R"({
  "address": "0xc6962004f452be9203591991d15f6b388e09e8d0",
  "topics": ["0xc42079f94a6350d7e6235f29174924f928cc2ac818eb64fed8004e115fbcca67",
             "0x00000000000000000000000096daa0b8a5499ea9323421ed0cda06b345caab73",
             "0x00000000000000000000000096daa0b8a5499ea9323421ed0cda06b345caab73"],
  "data": "0x0000000000000000000000000000000000000000000000001147a2bb08dfa66bffffffffffffffffffffffffffffffffffffffffffffffffffffffff52538de2000000000000000000000000000000000000000000032bc9a2321a8e4000000000000000000000000000000000000000000000000000000035e4ee3ff449d831fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffcf7b0",
  "blockNumber": "0x1ef5d33c", "logIndex": "0x2",
  "transactionHash": "0xf32a46e76f05cb2929822b0ac6bb818ae572f24c64ab0fb1674863ea7b449ed3",
  "removed": false})";

TEST(RpcTypesTest, ParsesLog) {
    simdjson::dom::parser parser;
    const Log log = parse_log(parser.parse(simdjson::padded_string(kSwapLog)));
    EXPECT_EQ(log.address.to_checksum(), "0xC6962004f452bE9203591991D15f6b388e09E8D0");
    ASSERT_EQ(log.topics.size(), 3u);
    EXPECT_EQ(eth::to_hex(log.topics[0]),
              "0xc42079f94a6350d7e6235f29174924f928cc2ac818eb64fed8004e115fbcca67");
    EXPECT_EQ(log.data.size(), 5u * 32);
    EXPECT_EQ(log.block_number, 0x1ef5d33cu);
    EXPECT_EQ(log.log_index, 2u);
    EXPECT_FALSE(log.removed);
}

TEST(RpcTypesTest, PendingLogWithoutBlockNumber) {
    simdjson::dom::parser parser;
    const Log log = parse_log(parser.parse(simdjson::padded_string(std::string_view(
        R"({"address":"0xc6962004f452be9203591991d15f6b388e09e8d0","topics":[],"data":"0x",)"
        R"("blockNumber":null,"logIndex":null,"transactionHash":null})"))));
    EXPECT_EQ(log.block_number, 0u);
    EXPECT_TRUE(log.data.empty());
}

TEST(RpcTypesTest, LogFilterJson) {
    LogFilter f;
    f.addresses.push_back(*eth::Address::from_hex("0xc6962004f452be9203591991d15f6b388e09e8d0"));
    f.topic0.push_back(eth::Hash{});
    f.from_block = 16;
    f.to_block = 255;
    EXPECT_EQ(f.to_json(),
              R"({"address":["0xc6962004f452be9203591991d15f6b388e09e8d0"],"topics":[["0x)" +
                  std::string(64, '0') + R"("]],"fromBlock":"0x10","toBlock":"0xff"})");
}

TEST(RpcTypesTest, ErrorObjectThrows) {
    simdjson::dom::parser parser;
    const simdjson::dom::element doc = parser.parse(simdjson::padded_string(std::string_view(
        R"({"jsonrpc":"2.0","id":1,"error":{"code":3,"message":"execution reverted","data":"0x08c379a0"}})")));
    try {
        throw_if_rpc_error(doc);
        FAIL() << "expected RpcError";
    } catch (const RpcError& e) {
        EXPECT_EQ(e.code, 3);
        EXPECT_EQ(e.data, "0x08c379a0");
        EXPECT_STREQ(e.what(), "execution reverted");
    }
}

}  // namespace
}  // namespace hft::net
