#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "eth/u256.hpp"

namespace hft::eth::rlp {

std::vector<std::uint8_t> encode_bytes(std::span<const std::uint8_t> bytes);

class List {
public:
    List& add_bytes(std::span<const std::uint8_t> bytes);
    List& add_uint(const u256& value);
    List& add_uint(std::uint64_t value);
    List& add_list(const List& list);

    std::vector<std::uint8_t> encode() const;

private:
    std::vector<std::uint8_t> payload_;
};

}
