#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hft::eth {

std::string to_hex(std::span<const std::uint8_t> bytes);

bool from_hex(std::string_view hex, std::span<std::uint8_t> out);

std::optional<std::vector<std::uint8_t>> from_hex(std::string_view hex);

}
