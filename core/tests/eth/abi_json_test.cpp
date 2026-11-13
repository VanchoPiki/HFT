#include "eth/abi_json.hpp"

#include <gtest/gtest.h>

#include "eth/hex.hpp"

namespace hft::eth::abi {
namespace {

constexpr std::string_view kAbi = R"([
  {"type":"function","name":"slot0","stateMutability":"view","inputs":[],
   "outputs":[{"name":"sqrtPriceX96","type":"uint160"},{"name":"tick","type":"int24"}]},
  {"type":"function","name":"quoteExactInputSingle","stateMutability":"nonpayable",
   "inputs":[{"name":"params","type":"tuple","internalType":"struct IQuoterV2.QuoteExactInputSingleParams",
              "components":[{"name":"tokenIn","type":"address"},{"name":"tokenOut","type":"address"},
                            {"name":"amountIn","type":"uint256"},{"name":"fee","type":"uint24"},
                            {"name":"sqrtPriceLimitX96","type":"uint160"}]}],
   "outputs":[{"name":"amountOut","type":"uint256"}]},
  {"type":"event","name":"Swap","anonymous":false,
   "inputs":[{"name":"sender","type":"address","indexed":true},
             {"name":"recipient","type":"address","indexed":true},
             {"name":"amount0","type":"int256","indexed":false},
             {"name":"amount1","type":"int256","indexed":false},
             {"name":"sqrtPriceX96","type":"uint160","indexed":false},
             {"name":"liquidity","type":"uint128","indexed":false},
             {"name":"tick","type":"int24","indexed":false}]},
  {"type":"function","name":"over","inputs":[{"name":"a","type":"uint256"}],"outputs":[]},
  {"type":"function","name":"over","inputs":[],"outputs":[]},
  {"type":"constructor","inputs":[]}
])";

TEST(AbiJsonTest, FunctionSignatureAndSelector) {
    const auto contract = Contract::parse(kAbi);
    const auto& slot0 = contract.function("slot0");
    EXPECT_EQ(slot0.signature, "slot0()");
    EXPECT_EQ(to_hex(slot0.selector), "0x3850c7bd");
    EXPECT_EQ(slot0.state_mutability, "view");
    ASSERT_EQ(slot0.outputs.size(), 2u);
    EXPECT_EQ(slot0.outputs[1].type, "int24");
}

TEST(AbiJsonTest, TupleParametersAreExpanded) {
    const auto contract = Contract::parse(kAbi);
    const auto& quote = contract.function("quoteExactInputSingle");
    EXPECT_EQ(quote.signature, "quoteExactInputSingle((address,address,uint256,uint24,uint160))");
    EXPECT_EQ(to_hex(quote.selector), "0xc6a5026a");
}

TEST(AbiJsonTest, EventTopicAndIndexedFlags) {
    const auto contract = Contract::parse(kAbi);
    const auto& swap = contract.event("Swap");
    EXPECT_EQ(to_hex(swap.topic),
              "0xc42079f94a6350d7e6235f29174924f928cc2ac818eb64fed8004e115fbcca67");
    ASSERT_EQ(swap.inputs.size(), 7u);
    EXPECT_TRUE(swap.inputs[0].indexed);
    EXPECT_FALSE(swap.inputs[2].indexed);
}

TEST(AbiJsonTest, AcceptsFoundryArtifact) {
    const std::string artifact = R"({"abi":)" + std::string(kAbi) + R"(,"bytecode":{"object":"0x"}})";
    EXPECT_EQ(Contract::parse(artifact).functions().size(), 4u);
}

TEST(AbiJsonTest, CustomErrorsFromTheExecutorAbi) {
    const auto executor = Contract::load_file(std::string(HFT_ABI_DIR) + "/ArbExecutor.json");
    const auto& execute = executor.function("execute");
    EXPECT_EQ(execute.signature, "execute(address,address,address,address,uint256,uint256)");
    EXPECT_EQ(to_hex(execute.selector), "0x7264e8bf");  // `cast sig`
    const auto revert = *from_hex("0x653406a4" + std::string(128, '0'));
    EXPECT_EQ(executor.error_name(revert), "InsufficientProfit");
    EXPECT_EQ(executor.error_name(*from_hex("0xdeadbeef")), "");
}

TEST(AbiJsonTest, MissingAndOverloadedNamesThrow) {
    const auto contract = Contract::parse(kAbi);
    EXPECT_THROW(contract.function("nope"), Error);
    EXPECT_THROW(contract.function("over"), Error);
    EXPECT_THROW(contract.event("Mint"), Error);
}

}  // namespace
}  // namespace hft::eth::abi
