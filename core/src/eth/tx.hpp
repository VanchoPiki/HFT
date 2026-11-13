#pragma once

#include <cstdint>
#include <vector>

#include "eth/address.hpp"
#include "eth/keccak.hpp"
#include "eth/signer.hpp"
#include "eth/u256.hpp"

namespace hft::eth {

// EIP-1559 (type 2) transaction with an empty access list.
struct Eip1559Tx {
    std::uint64_t chain_id = 0;
    std::uint64_t nonce = 0;
    u256 max_priority_fee_per_gas;
    u256 max_fee_per_gas;
    std::uint64_t gas_limit = 0;
    Address to;
    u256 value;
    std::vector<std::uint8_t> data;
};

// keccak256(0x02 || rlp([chainId, nonce, maxPriorityFee, maxFee, gasLimit, to, value,
// data, accessList])): what the sender signs.
Hash signing_hash(const Eip1559Tx& tx);

// 0x02 || rlp([... the 9 fields ..., yParity, r, s]): what eth_sendRawTransaction takes.
std::vector<std::uint8_t> encode_signed(const Eip1559Tx& tx, const Signature& signature);

// Signs and encodes in one go.
std::vector<std::uint8_t> sign_transaction(const Eip1559Tx& tx, const Signer& signer);

// The transaction hash is keccak256 of the raw signed bytes.
inline Hash transaction_hash(const std::vector<std::uint8_t>& raw) { return keccak256(raw); }

}  // namespace hft::eth
