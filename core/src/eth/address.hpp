#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

namespace hft::eth {

struct Address {
    std::array<std::uint8_t, 20> bytes{};

    static std::optional<Address> from_hex(std::string_view hex);

    std::string to_hex() const;
    std::string to_checksum() const;
    bool is_zero() const;

    auto operator<=>(const Address&) const = default;
};

struct AddressHash {
    std::size_t operator()(const Address& a) const noexcept {
        std::size_t h;
        std::memcpy(&h, a.bytes.data() + a.bytes.size() - sizeof(h), sizeof(h));
        return h;
    }
};

}
