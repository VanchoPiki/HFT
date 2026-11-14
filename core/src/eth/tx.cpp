#include "eth/tx.hpp"

#include "eth/rlp.hpp"

namespace hft::eth {
namespace {

constexpr std::uint8_t kEip1559Type = 0x02;

rlp::List unsigned_fields(const Eip1559Tx& tx) {
    rlp::List fields;
    fields.add_uint(tx.chain_id)
        .add_uint(tx.nonce)
        .add_uint(tx.max_priority_fee_per_gas)
        .add_uint(tx.max_fee_per_gas)
        .add_uint(tx.gas_limit)
        .add_bytes(tx.to.bytes)
        .add_uint(tx.value)
        .add_bytes(tx.data)
        .add_list(rlp::List{});
    return fields;
}

std::vector<std::uint8_t> typed(const rlp::List& fields) {
    std::vector<std::uint8_t> out{kEip1559Type};
    const auto body = fields.encode();
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

}

Hash signing_hash(const Eip1559Tx& tx) { return keccak256(typed(unsigned_fields(tx))); }

std::vector<std::uint8_t> encode_signed(const Eip1559Tx& tx, const Signature& signature) {
    rlp::List fields = unsigned_fields(tx);
    fields.add_uint(std::uint64_t{signature.y_parity}).add_uint(signature.r).add_uint(signature.s);
    return typed(fields);
}

std::vector<std::uint8_t> sign_transaction(const Eip1559Tx& tx, const Signer& signer) {
    return encode_signed(tx, signer.sign(signing_hash(tx)));
}

}
