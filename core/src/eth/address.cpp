#include "eth/address.hpp"

#include <algorithm>

#include "eth/hex.hpp"
#include "eth/keccak.hpp"

namespace hft::eth {

std::optional<Address> Address::from_hex(std::string_view hex) {
    Address a;
    if (!eth::from_hex(hex, a.bytes)) {
        return std::nullopt;
    }
    return a;
}

std::string Address::to_hex() const { return eth::to_hex(bytes); }

std::string Address::to_checksum() const {
    std::string hex = to_hex();
    const Hash hash = keccak256(std::string_view(hex).substr(2));
    for (std::size_t i = 0; i < 40; ++i) {
        char& c = hex[2 + i];
        const int nibble = (i % 2 == 0) ? (hash[i / 2] >> 4) : (hash[i / 2] & 0x0f);
        if (c >= 'a' && c <= 'f' && nibble >= 8) {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return hex;
}

bool Address::is_zero() const {
    return std::all_of(bytes.begin(), bytes.end(), [](std::uint8_t b) { return b == 0; });
}

}  // namespace hft::eth
