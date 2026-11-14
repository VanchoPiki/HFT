#include "eth/rlp.hpp"

namespace hft::eth::rlp {
namespace {

void append_header(std::vector<std::uint8_t>& out, std::size_t length, std::uint8_t base) {
    if (length <= 55) {
        out.push_back(static_cast<std::uint8_t>(base + length));
        return;
    }
    std::uint8_t be[8];
    int n = 0;
    for (std::size_t v = length; v != 0; v >>= 8) {
        be[n++] = static_cast<std::uint8_t>(v);
    }
    out.push_back(static_cast<std::uint8_t>(base + 55 + n));
    while (n > 0) {
        out.push_back(be[--n]);
    }
}

void append_bytes(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> bytes) {
    if (bytes.size() == 1 && bytes[0] < 0x80) {
        out.push_back(bytes[0]);
        return;
    }
    append_header(out, bytes.size(), 0x80);
    out.insert(out.end(), bytes.begin(), bytes.end());
}

}

std::vector<std::uint8_t> encode_bytes(std::span<const std::uint8_t> bytes) {
    std::vector<std::uint8_t> out;
    append_bytes(out, bytes);
    return out;
}

List& List::add_bytes(std::span<const std::uint8_t> bytes) {
    append_bytes(payload_, bytes);
    return *this;
}

List& List::add_uint(const u256& value) {
    append_bytes(payload_, to_be_minimal(value));
    return *this;
}

List& List::add_uint(std::uint64_t value) {
    std::uint8_t be[8];
    int n = 0;
    for (std::uint64_t v = value; v != 0; v >>= 8) {
        be[7 - n++] = static_cast<std::uint8_t>(v);
    }
    append_bytes(payload_, std::span(be + 8 - n, static_cast<std::size_t>(n)));
    return *this;
}

List& List::add_list(const List& list) {
    append_header(payload_, list.payload_.size(), 0xc0);
    payload_.insert(payload_.end(), list.payload_.begin(), list.payload_.end());
    return *this;
}

std::vector<std::uint8_t> List::encode() const {
    std::vector<std::uint8_t> out;
    out.reserve(payload_.size() + 9);
    append_header(out, payload_.size(), 0xc0);
    out.insert(out.end(), payload_.begin(), payload_.end());
    return out;
}

}
