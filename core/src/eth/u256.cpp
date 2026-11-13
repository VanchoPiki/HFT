#include "eth/u256.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "eth/hex.hpp"

namespace hft::eth {
namespace {

constexpr std::uint64_t kLimbMask = ~std::uint64_t{0};

std::uint64_t load_be64(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v = (v << 8) | p[i];
    }
    return v;
}

void store_be64(std::uint8_t* p, std::uint64_t v) {
    for (int i = 7; i >= 0; --i) {
        p[i] = static_cast<std::uint8_t>(v);
        v >>= 8;
    }
}

}  // namespace

u256 u256_from_be(std::span<const std::uint8_t> bytes) {
    if (bytes.size() > 32) {
        throw std::invalid_argument("u256_from_be: more than 32 bytes");
    }
    Word w{};
    std::copy(bytes.begin(), bytes.end(), w.end() - static_cast<std::ptrdiff_t>(bytes.size()));
    u256 v = load_be64(w.data());
    for (int i = 1; i < 4; ++i) {
        v <<= 64;
        v |= load_be64(w.data() + 8 * i);
    }
    return v;
}

Word to_word(const u256& value) {
    Word w{};
    for (int i = 0; i < 4; ++i) {
        const auto limb = static_cast<std::uint64_t>((value >> (64 * (3 - i))) & kLimbMask);
        store_be64(w.data() + 8 * i, limb);
    }
    return w;
}

std::vector<std::uint8_t> to_be_minimal(const u256& value) {
    const Word w = to_word(value);
    const auto first = std::find_if(w.begin(), w.end(), [](std::uint8_t b) { return b != 0; });
    return {first, w.end()};
}

std::optional<u256> parse_quantity(std::string_view hex) {
    if (hex.size() < 3 || hex[0] != '0' || (hex[1] != 'x' && hex[1] != 'X')) {
        return std::nullopt;
    }
    hex.remove_prefix(2);
    if (hex.size() > 64) {
        return std::nullopt;
    }
    char padded[64];
    const std::size_t pad = 64 - hex.size();
    std::fill_n(padded, pad, '0');
    std::copy(hex.begin(), hex.end(), padded + pad);
    Word w{};
    if (!from_hex(std::string_view(padded, 64), w)) {
        return std::nullopt;
    }
    return u256_from_be(w);
}

std::optional<std::uint64_t> parse_quantity_u64(std::string_view hex) {
    const auto v = parse_quantity(hex);
    if (!v || *v > std::numeric_limits<std::uint64_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(*v);
}

std::string to_quantity(const u256& value) {
    const std::string hex = to_hex(to_word(value));
    const auto first = hex.find_first_not_of('0', 2);
    if (first == std::string::npos) {
        return "0x0";
    }
    return "0x" + hex.substr(first);
}

i256 to_signed(const u256& bits) {
    if (boost::multiprecision::bit_test(bits, 255)) {
        const u256 magnitude = ~bits + 1;  // 2^255 for the minimum value, which still fits
        return -static_cast<i256>(magnitude);
    }
    return static_cast<i256>(bits);
}

u256 from_signed(const i256& value) {
    if (value < 0) {
        const auto magnitude = static_cast<u256>(-value);
        return ~magnitude + 1;
    }
    return static_cast<u256>(value);
}

}  // namespace hft::eth
