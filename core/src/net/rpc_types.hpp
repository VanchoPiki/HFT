#pragma once

#include <simdjson.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "eth/address.hpp"
#include "eth/keccak.hpp"
#include "eth/u256.hpp"

namespace hft::net {

struct RpcError : std::runtime_error {
    RpcError(int code, const std::string& message, std::string data = {});
    int code;
    std::string data;
};

struct Log {
    eth::Address address;
    std::vector<eth::Hash> topics;
    std::vector<std::uint8_t> data;
    std::uint64_t block_number = 0;
    std::uint64_t log_index = 0;
    eth::Hash tx_hash{};
    bool removed = false;
};

struct BlockHeader {
    std::uint64_t number = 0;
    std::uint64_t timestamp = 0;
    eth::u256 base_fee;
};

struct Receipt {
    eth::Hash tx_hash{};
    bool success = false;
    std::uint64_t block_number = 0;
    std::uint64_t gas_used = 0;
    eth::u256 effective_gas_price;
    std::vector<Log> logs;
};

struct LogFilter {
    std::vector<eth::Address> addresses;
    std::vector<eth::Hash> topic0;
    std::optional<std::uint64_t> from_block;
    std::optional<std::uint64_t> to_block;

    std::string to_json() const;
};

Log parse_log(simdjson::dom::element json);
BlockHeader parse_block_header(simdjson::dom::element json);
Receipt parse_receipt(simdjson::dom::element json);

void throw_if_rpc_error(simdjson::dom::element response);

}
