#include "eth/hex.hpp"

#include <array>

namespace hft::eth {
namespace {

constexpr char kDigits[] = "0123456789abcdef";

constexpr std::array<std::int8_t, 256> make_decode_table() {
    std::array<std::int8_t, 256> table{};
    table.fill(-1);
    for (std::size_t i = 0; i < 10; ++i) {
        table['0' + i] = static_cast<std::int8_t>(i);
    }
    for (std::size_t i = 0; i < 6; ++i) {
        table['a' + i] = static_cast<std::int8_t>(10 + i);
        table['A' + i] = static_cast<std::int8_t>(10 + i);
    }
    return table;
}

constexpr auto kDecode = make_decode_table();

std::string_view strip_prefix(std::string_view hex) {
    if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex.remove_prefix(2);
    }
    return hex;
}

}

std::string to_hex(std::span<const std::uint8_t> bytes) {
    std::string out(2 + bytes.size() * 2, '\0');
    out[0] = '0';
    out[1] = 'x';
    char* p = out.data() + 2;
    for (std::uint8_t b : bytes) {
        *p++ = kDigits[b >> 4];
        *p++ = kDigits[b & 0x0f];
    }
    return out;
}

bool from_hex(std::string_view hex, std::span<std::uint8_t> out) {
    hex = strip_prefix(hex);
    if (hex.size() != out.size() * 2) {
        return false;
    }
    for (std::size_t i = 0; i < out.size(); ++i) {
        const int hi = kDecode[static_cast<unsigned char>(hex[2 * i])];
        const int lo = kDecode[static_cast<unsigned char>(hex[2 * i + 1])];
        if ((hi | lo) < 0) {
            return false;
        }
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

std::optional<std::vector<std::uint8_t>> from_hex(std::string_view hex) {
    hex = strip_prefix(hex);
    if (hex.size() % 2 != 0) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> out(hex.size() / 2);
    if (!from_hex(hex, out)) {
        return std::nullopt;
    }
    return out;
}

}
