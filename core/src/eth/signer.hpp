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

struct Signature {
    u256 r;
    u256 s;
    std::uint8_t y_parity = 0;
};

class Signer {
public:
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

}
