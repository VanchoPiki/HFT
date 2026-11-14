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

using u256 = boost::multiprecision::uint256_t;
using u512 = boost::multiprecision::uint512_t;
using i256 = boost::multiprecision::int256_t;

using Word = std::array<std::uint8_t, 32>;

u256 u256_from_be(std::span<const std::uint8_t> bytes);

Word to_word(const u256& value);

std::vector<std::uint8_t> to_be_minimal(const u256& value);

std::optional<u256> parse_quantity(std::string_view hex);
std::optional<std::uint64_t> parse_quantity_u64(std::string_view hex);
std::string to_quantity(const u256& value);

i256 to_signed(const u256& bits);
u256 from_signed(const i256& value);

}
