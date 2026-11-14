#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace hft::eth {

using Hash = std::array<std::uint8_t, 32>;

Hash keccak256(std::span<const std::uint8_t> data);
Hash keccak256(std::string_view text);

}
