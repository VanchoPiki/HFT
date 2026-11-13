#include "eth/abi.hpp"

#include <algorithm>
#include <limits>

namespace hft::eth::abi {

Selector selector(std::string_view signature) {
    const Hash h = keccak256(signature);
    return {h[0], h[1], h[2], h[3]};
}

Hash event_topic(std::string_view signature) { return keccak256(signature); }

Encoder::Encoder(const Selector& sel) : prefix_(sel.begin(), sel.end()) {}

Encoder& Encoder::add_uint(const u256& value) {
    head_.push_back(to_word(value));
    return *this;
}

Encoder& Encoder::add_int(const i256& value) {
    head_.push_back(to_word(from_signed(value)));
    return *this;
}

Encoder& Encoder::add_address(const Address& address) {
    Word w{};
    std::copy(address.bytes.begin(), address.bytes.end(), w.begin() + 12);
    head_.push_back(w);
    return *this;
}

Encoder& Encoder::add_bool(bool value) {
    Word w{};
    w[31] = value ? 1 : 0;
    head_.push_back(w);
    return *this;
}

Encoder& Encoder::add_bytes32(const Word& word) {
    head_.push_back(word);
    return *this;
}

Encoder& Encoder::add_bytes(std::span<const std::uint8_t> data) {
    dynamic_.push_back({head_.size(), tail_.size()});
    head_.push_back(Word{});  // offset, patched in finish()
    const Word length = to_word(data.size());
    tail_.insert(tail_.end(), length.begin(), length.end());
    tail_.insert(tail_.end(), data.begin(), data.end());
    tail_.resize(tail_.size() + (32 - data.size() % 32) % 32, 0);
    return *this;
}

std::vector<std::uint8_t> Encoder::finish() const {
    std::vector<Word> head = head_;
    const std::size_t head_bytes = head.size() * 32;
    for (const auto& slot : dynamic_) {
        head[slot.head_index] = to_word(head_bytes + slot.tail_offset);
    }
    std::vector<std::uint8_t> out;
    out.reserve(prefix_.size() + head_bytes + tail_.size());
    out.insert(out.end(), prefix_.begin(), prefix_.end());
    for (const auto& w : head) {
        out.insert(out.end(), w.begin(), w.end());
    }
    out.insert(out.end(), tail_.begin(), tail_.end());
    return out;
}

const std::uint8_t* Reader::next_word() {
    if (data_.size() < 32 || pos_ > data_.size() - 32) {
        throw Error("abi: read past the end of data");
    }
    const std::uint8_t* p = data_.data() + pos_;
    pos_ += 32;
    return p;
}

u256 Reader::read_uint() { return u256_from_be({next_word(), 32}); }

std::uint64_t Reader::read_u64() {
    const u256 v = read_uint();
    if (v > std::numeric_limits<std::uint64_t>::max()) {
        throw Error("abi: value does not fit into 64 bits");
    }
    return static_cast<std::uint64_t>(v);
}

i256 Reader::read_int() { return to_signed(read_uint()); }

Address Reader::read_address() {
    const std::uint8_t* p = next_word();
    Address a;
    std::copy(p + 12, p + 32, a.bytes.begin());
    return a;
}

bool Reader::read_bool() { return read_uint() != 0; }

Word Reader::read_word() {
    Word w;
    std::copy_n(next_word(), 32, w.begin());
    return w;
}

Reader Reader::read_dynamic() {
    const u256 offset = read_uint();
    if (offset > data_.size()) {
        throw Error("abi: dynamic offset out of range");
    }
    return Reader(data_.subspan(static_cast<std::size_t>(offset)));
}

std::vector<std::uint8_t> Reader::read_bytes() {
    Reader tail = read_dynamic();
    const u256 length = tail.read_uint();
    if (length > tail.data_.size() - tail.pos_) {
        throw Error("abi: bytes length out of range");
    }
    const std::uint8_t* p = tail.data_.data() + tail.pos_;
    return {p, p + static_cast<std::size_t>(length)};
}

void Reader::skip(std::size_t words) {
    if (words > (data_.size() - pos_) / 32) {
        throw Error("abi: skip past the end of data");
    }
    pos_ += 32 * words;
}

}  // namespace hft::eth::abi
