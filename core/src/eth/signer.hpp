#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "eth/address.hpp"
#include "eth/keccak.hpp"
#include "eth/u256.hpp"

struct secp256k1_context_struct;

namespace hft::eth {

// Recoverable ECDSA signature over secp256k1, as Ethereum transactions carry it.
struct Signature {
    u256 r;
    u256 s;  // always in the lower half of the curve order (EIP-2)
    std::uint8_t y_parity = 0;
};

// Holds a private key and signs 32-byte hashes. The key never leaves this object and is
// wiped on destruction. Signing is deterministic (RFC 6979), so equal inputs give equal
// signatures, which is what lets tests compare against `cast` byte for byte.
class Signer {
public:
    // Throws std::invalid_argument if the key is not a valid secp256k1 secret key.
    explicit Signer(std::span<const std::uint8_t, 32> private_key);
    static Signer from_hex(std::string_view private_key_hex);
    ~Signer();
    Signer(Signer&& other) noexcept;
    Signer& operator=(Signer&&) = delete;
    Signer(const Signer&) = delete;
    Signer& operator=(const Signer&) = delete;

    const Address& address() const { return address_; }
    Signature sign(const Hash& hash) const;

private:
    secp256k1_context_struct* ctx_;
    std::array<std::uint8_t, 32> key_;
    Address address_;
};

}  // namespace hft::eth
