#include "net/rpc_types.hpp"

#include "eth/hex.hpp"

namespace hft::net {
namespace {

std::string_view field(simdjson::dom::element json, const char* key) {
    std::string_view value;
    if (json[key].get(value) != simdjson::SUCCESS) {
        throw RpcError(-1, std::string("rpc: missing string field '") + key + "'");
    }
    return value;
}

std::uint64_t quantity_field(simdjson::dom::element json, const char* key) {
    const auto value = eth::parse_quantity_u64(field(json, key));
    if (!value) {
        throw RpcError(-1, std::string("rpc: bad quantity in '") + key + "'");
    }
    return *value;
}

std::optional<std::string_view> optional_field(simdjson::dom::element json, const char* key) {
    std::string_view value;
    if (json[key].get(value) != simdjson::SUCCESS) {
        return std::nullopt;  // missing or null (e.g. blockNumber of a pending log)
    }
    return value;
}

eth::Hash hash_from(std::string_view hex) {
    eth::Hash h{};
    if (!eth::from_hex(hex, h)) {
        throw RpcError(-1, "rpc: bad 32-byte hash");
    }
    return h;
}

}  // namespace

RpcError::RpcError(int c, const std::string& message, std::string d)
    : std::runtime_error(message), code(c), data(std::move(d)) {}

std::string LogFilter::to_json() const {
    std::string json = R"({"address":[)";
    for (std::size_t i = 0; i < addresses.size(); ++i) {
        json += (i ? ",\"" : "\"") + addresses[i].to_hex() + '"';
    }
    json += "]";
    if (!topic0.empty()) {
        json += R"(,"topics":[[)";
        for (std::size_t i = 0; i < topic0.size(); ++i) {
            json += (i ? ",\"" : "\"") + eth::to_hex(topic0[i]) + '"';
        }
        json += "]]";
    }
    if (from_block) {
        json += R"(,"fromBlock":")" + eth::to_quantity(*from_block) + '"';
    }
    if (to_block) {
        json += R"(,"toBlock":")" + eth::to_quantity(*to_block) + '"';
    }
    json += '}';
    return json;
}

Log parse_log(simdjson::dom::element json) {
    Log log;
    const auto address = eth::Address::from_hex(field(json, "address"));
    if (!address) {
        throw RpcError(-1, "rpc: bad log address");
    }
    log.address = *address;
    for (simdjson::dom::element topic : json["topics"].get_array()) {
        log.topics.push_back(hash_from(std::string_view(topic)));
    }
    auto data = eth::from_hex(field(json, "data"));
    if (!data) {
        throw RpcError(-1, "rpc: bad log data");
    }
    log.data = std::move(*data);
    if (const auto block = optional_field(json, "blockNumber")) {
        log.block_number = eth::parse_quantity_u64(*block).value_or(0);
    }
    if (const auto index = optional_field(json, "logIndex")) {
        log.log_index = eth::parse_quantity_u64(*index).value_or(0);
    }
    if (const auto tx = optional_field(json, "transactionHash")) {
        log.tx_hash = hash_from(*tx);
    }
    bool removed = false;
    if (json["removed"].get(removed) == simdjson::SUCCESS) {
        log.removed = removed;
    }
    return log;
}

BlockHeader parse_block_header(simdjson::dom::element json) {
    BlockHeader header;
    header.number = quantity_field(json, "number");
    header.timestamp = quantity_field(json, "timestamp");
    if (const auto base_fee = optional_field(json, "baseFeePerGas")) {
        header.base_fee = eth::parse_quantity(*base_fee).value_or(0);
    }
    return header;
}

Receipt parse_receipt(simdjson::dom::element json) {
    Receipt receipt;
    receipt.tx_hash = hash_from(field(json, "transactionHash"));
    receipt.success = quantity_field(json, "status") == 1;
    receipt.block_number = quantity_field(json, "blockNumber");
    receipt.gas_used = quantity_field(json, "gasUsed");
    if (const auto price = optional_field(json, "effectiveGasPrice")) {
        receipt.effective_gas_price = eth::parse_quantity(*price).value_or(0);
    }
    for (simdjson::dom::element log : json["logs"].get_array()) {
        receipt.logs.push_back(parse_log(log));
    }
    return receipt;
}

void throw_if_rpc_error(simdjson::dom::element response) {
    simdjson::dom::element error;
    if (response["error"].get(error) != simdjson::SUCCESS) {
        return;
    }
    std::int64_t code = -1;
    if (error["code"].get(code) != simdjson::SUCCESS) {
        code = -1;
    }
    std::string_view message;
    if (error["message"].get(message) != simdjson::SUCCESS) {
        message = "unknown error";
    }
    std::string_view data;
    if (error["data"].get(data) != simdjson::SUCCESS) {
        data = {};  // revert data is optional
    }
    throw RpcError(static_cast<int>(code), std::string(message), std::string(data));
}

}  // namespace hft::net
