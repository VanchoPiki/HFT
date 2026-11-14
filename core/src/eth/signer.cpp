#include "eth/signer.hpp"

#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include <algorithm>
#include <random>
#include <stdexcept>

#include "eth/hex.hpp"

namespace hft::eth {
namespace {

void wipe(std::span<std::uint8_t> bytes) {
    volatile std::uint8_t* p = bytes.data();
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        p[i] = 0;
    }
}

}

Signer::Signer(std::span<const std::uint8_t, 32> private_key)
    : ctx_(secp256k1_context_create(SECP256K1_CONTEXT_NONE)) {
    std::copy(private_key.begin(), private_key.end(), key_.begin());
    auto fail = [this](const char* message) {
        wipe(key_);
        secp256k1_context_destroy(ctx_);
        throw std::invalid_argument(message);
    };
    if (secp256k1_ec_seckey_verify(ctx_, key_.data()) != 1) {
        fail("signer: invalid private key");
    }
    std::array<std::uint8_t, 32> seed{};
    std::random_device rd;
    std::generate(seed.begin(), seed.end(), [&] { return static_cast<std::uint8_t>(rd()); });
    if (secp256k1_context_randomize(ctx_, seed.data()) != 1) {
        fail("signer: context randomization failed");
    }

    secp256k1_pubkey pubkey;
    if (secp256k1_ec_pubkey_create(ctx_, &pubkey, key_.data()) != 1) {
        fail("signer: public key derivation failed");
    }
    std::array<std::uint8_t, 65> serialized{};
    std::size_t length = serialized.size();
    secp256k1_ec_pubkey_serialize(ctx_, serialized.data(), &length, &pubkey,
                                  SECP256K1_EC_UNCOMPRESSED);
    const Hash hash = keccak256(std::span(serialized).subspan(1));
    std::copy(hash.begin() + 12, hash.end(), address_.bytes.begin());
}

Signer Signer::from_hex(std::string_view private_key_hex) {
    std::array<std::uint8_t, 32> key{};
    if (!eth::from_hex(private_key_hex, key)) {
        throw std::invalid_argument("signer: private key must be 32 bytes of hex");
    }
    Signer signer(key);
    wipe(key);
    return signer;
}

Signer::Signer(Signer&& other) noexcept
    : ctx_(other.ctx_), key_(other.key_), address_(other.address_) {
    other.ctx_ = nullptr;
    wipe(other.key_);
}

Signer::~Signer() {
    wipe(key_);
    if (ctx_ != nullptr) {
        secp256k1_context_destroy(ctx_);
    }
}

Signature Signer::sign(const Hash& hash) const {
    secp256k1_ecdsa_recoverable_signature sig;
    if (secp256k1_ecdsa_sign_recoverable(ctx_, &sig, hash.data(), key_.data(), nullptr,
                                         nullptr) != 1) {
        throw std::runtime_error("signer: signing failed");
    }
    std::array<std::uint8_t, 64> compact{};
    int recovery_id = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx_, compact.data(), &recovery_id,
                                                            &sig);
    Signature out;
    out.r = u256_from_be(std::span(compact).first(32));
    out.s = u256_from_be(std::span(compact).last(32));
    out.y_parity = static_cast<std::uint8_t>(recovery_id);
    return out;
}

}
