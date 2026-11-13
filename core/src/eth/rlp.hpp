#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "eth/u256.hpp"

namespace hft::eth::rlp {

// RLP encoding of a single byte string.
std::vector<std::uint8_t> encode_bytes(std::span<const std::uint8_t> bytes);

// Builds an RLP list item by item. Integers are encoded as minimal big-endian byte
// strings (0 -> empty string), as Ethereum transactions require.
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

}  // namespace hft::eth::rlp
