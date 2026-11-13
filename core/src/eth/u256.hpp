#pragma once

#include <boost/multiprecision/cpp_int.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hft::eth {

// Fixed-width EVM integers. Unsigned types wrap modulo 2^N exactly like the EVM.
// i256 is sign-magnitude (Boost), not two's complement: use to_signed/from_signed
// to convert from/to the EVM bit pattern.
using u256 = boost::multiprecision::uint256_t;
using u512 = boost::multiprecision::uint512_t;
using i256 = boost::multiprecision::int256_t;

using Word = std::array<std::uint8_t, 32>;

// Big-endian bytes (at most 32) -> u256.
u256 u256_from_be(std::span<const std::uint8_t> bytes);

// u256 -> 32-byte big-endian word.
Word to_word(const u256& value);

// u256 -> big-endian bytes without leading zeros (0 -> empty), as RLP wants.
std::vector<std::uint8_t> to_be_minimal(const u256& value);

// JSON-RPC quantity: "0x1a" ("0x0" for zero). Parsing tolerates leading zeros.
std::optional<u256> parse_quantity(std::string_view hex);
std::optional<std::uint64_t> parse_quantity_u64(std::string_view hex);
std::string to_quantity(const u256& value);

// EVM two's complement bits <-> signed value.
i256 to_signed(const u256& bits);
u256 from_signed(const i256& value);

}  // namespace hft::eth
