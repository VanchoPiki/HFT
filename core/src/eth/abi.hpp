#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "eth/address.hpp"
#include "eth/keccak.hpp"
#include "eth/u256.hpp"

namespace hft::eth::abi {

using Selector = std::array<std::uint8_t, 4>;

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

Selector selector(std::string_view signature);
Hash event_topic(std::string_view signature);

class Encoder {
public:
    Encoder() = default;
    explicit Encoder(const Selector& sel);

    Encoder& add_uint(const u256& value);
    Encoder& add_int(const i256& value);
    Encoder& add_address(const Address& address);
    Encoder& add_bool(bool value);
    Encoder& add_bytes32(const Word& word);
    Encoder& add_bytes(std::span<const std::uint8_t> data);

    std::vector<std::uint8_t> finish() const;

private:
    struct DynamicSlot {
        std::size_t head_index;
        std::size_t tail_offset;
    };

    std::vector<std::uint8_t> prefix_;
    std::vector<Word> head_;
    std::vector<std::uint8_t> tail_;
    std::vector<DynamicSlot> dynamic_;
};

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data) : data_(data) {}

    u256 read_uint();
    std::uint64_t read_u64();
    i256 read_int();
    Address read_address();
    bool read_bool();
    Word read_word();
    std::vector<std::uint8_t> read_bytes();

    Reader read_dynamic();

    void skip(std::size_t words = 1);

private:
    const std::uint8_t* next_word();

    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
};

}
