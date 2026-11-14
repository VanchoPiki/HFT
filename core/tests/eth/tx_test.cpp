#include "eth/tx.hpp"

#include <gtest/gtest.h>

#include "eth/abi.hpp"
#include "eth/hex.hpp"

namespace hft::eth {
namespace {

constexpr std::string_view kTestKey =
    "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80";

Address addr(std::string_view hex) { return *Address::from_hex(hex); }

TEST(SignerTest, DerivesTheAddress) {
    EXPECT_EQ(Signer::from_hex(kTestKey).address().to_checksum(),
              "0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266");
}

TEST(SignerTest, RejectsInvalidKeys) {
    EXPECT_THROW(Signer::from_hex("0x00"), std::invalid_argument);
    EXPECT_THROW(Signer::from_hex("0x" + std::string(64, '0')), std::invalid_argument);
    EXPECT_THROW(Signer::from_hex("0x" + std::string(64, 'f')), std::invalid_argument);
}

TEST(SignerTest, SignaturesAreLowS) {
    const Signer signer = Signer::from_hex(kTestKey);
    const u256 half_order("0x7fffffffffffffffffffffffffffffff5d576e7357a4501ddfe92f46681b20a0");
    for (int i = 0; i < 50; ++i) {
        const Signature sig = signer.sign(keccak256(std::to_string(i)));
        EXPECT_LE(sig.s, half_order);
        EXPECT_LE(sig.y_parity, 1);
    }
}

TEST(TxTest, ValueTransferMatchesCast) {
    Eip1559Tx tx;
    tx.chain_id = 31337;
    tx.nonce = 0;
    tx.max_priority_fee_per_gas = 1'000'000'000;
    tx.max_fee_per_gas = 2'000'000'000;
    tx.gas_limit = 21000;
    tx.to = addr("0x70997970C51812dc3A010C7d01b50e0d17dc79C8");
    tx.value = u256("1000000000000000000");
    EXPECT_EQ(to_hex(sign_transaction(tx, Signer::from_hex(kTestKey))),
              "0x02f874827a6980843b9aca0084773594008252089470997970c51812dc3a010c7d01b50e0d17dc79c8"
              "880de0b6b3a764000080c080a074128fd7d42631897474d93692787a2e56b813754468caaca4993c641c98"
              "6a37a06d5a3b027177cc2841199b70400fece0a29ac5eabee2fa9cbf9b6aa213716c48");
}

TEST(TxTest, ExecutorCallMatchesCast) {
    Eip1559Tx tx;
    tx.chain_id = 42161;
    tx.nonce = 7;
    tx.max_priority_fee_per_gas = 1000;
    tx.max_fee_per_gas = 20'000'000;
    tx.gas_limit = 420'000;
    tx.to = addr("0xc6962004f452be9203591991d15f6b388e09e8d0");
    tx.data = abi::Encoder(abi::selector("execute(address,address,address,address,uint256,uint256)"))
                  .add_address(addr("0xC6962004f452bE9203591991D15f6b388e09E8D0"))
                  .add_address(addr("0xc473e2aEE3441BF9240Be85eb122aBB059A3B57c"))
                  .add_address(addr("0x82aF49447D8a07e3bd95BD0d56f35241523fBab1"))
                  .add_address(addr("0xaf88d065e77c8cC2239327C5EDb3A432268e5831"))
                  .add_uint(u256("20000000000000000000"))
                  .add_uint(1)
                  .finish();
    const auto raw = sign_transaction(tx, Signer::from_hex(kTestKey));
    EXPECT_EQ(to_hex(raw),
              "0x02f9013082a4b1078203e88401312d00830668a094c6962004f452be9203591991d15f6b388e09e8d0"
              "80b8c47264e8bf000000000000000000000000c6962004f452be9203591991d15f6b388e09e8d0000000"
              "000000000000000000c473e2aee3441bf9240be85eb122abb059a3b57c00000000000000000000000082"
              "af49447d8a07e3bd95bd0d56f35241523fbab1000000000000000000000000af88d065e77c8cc2239327"
              "c5edb3a432268e5831000000000000000000000000000000000000000000000001158e460913d0000000"
              "00000000000000000000000000000000000000000000000000000000000001c080a0e00e61c5ce27e3fd"
              "21d0a2b083c36e5318bfdd7026437525866295527e35fe15a031c0cf3826ad9fe6eb7567cf0ccaa059ad"
              "dbe09ea2601429f6afd7b213db0c16");
}

}
}
