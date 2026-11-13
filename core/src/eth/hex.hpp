#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hft::eth {

// Hex for byte data (calldata, hashes, addresses), not for JSON-RPC quantities:
// odd-length strings like "0x1" are rejected here.

// Encodes bytes as lowercase hex with a "0x" prefix.
std::string to_hex(std::span<const std::uint8_t> bytes);

// Decodes into a fixed-size buffer (address = 20 bytes, bytes32 = 32 bytes) without allocating.
// The "0x" prefix is optional, digits are case-insensitive. Returns false if the length does
// not match out.size() exactly or a character is not a hex digit.
bool from_hex(std::string_view hex, std::span<std::uint8_t> out);

// Decodes a variable-length string. Returns nullopt on odd length or a non-hex character.
std::optional<std::vector<std::uint8_t>> from_hex(std::string_view hex);

}  // namespace hft::eth
