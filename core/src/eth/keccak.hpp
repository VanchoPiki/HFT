#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace hft::eth {

using Hash = std::array<std::uint8_t, 32>;

// Ethereum's Keccak-256: original Keccak padding (0x01), not NIST SHA3-256 (0x06).
Hash keccak256(std::span<const std::uint8_t> data);
Hash keccak256(std::string_view text);

}  // namespace hft::eth
