#pragma once

#include <cstdint>
#include <vector>

#include "eth/address.hpp"
#include "eth/keccak.hpp"
#include "eth/signer.hpp"
#include "eth/u256.hpp"

namespace hft::eth {

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

Hash signing_hash(const Eip1559Tx& tx);

std::vector<std::uint8_t> encode_signed(const Eip1559Tx& tx, const Signature& signature);

std::vector<std::uint8_t> sign_transaction(const Eip1559Tx& tx, const Signer& signer);

inline Hash transaction_hash(const std::vector<std::uint8_t>& raw) { return keccak256(raw); }

}
