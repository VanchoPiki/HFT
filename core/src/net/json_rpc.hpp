#pragma once

#include <simdjson.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "net/http_client.hpp"
#include "net/rpc_types.hpp"

namespace hft::net {

struct TxCall {
    std::optional<eth::Address> from;
    eth::Address to;
    std::vector<std::uint8_t> data;
    eth::u256 value;
};

struct CallRequest {
    eth::Address to;
    std::vector<std::uint8_t> data;
};

struct CallResult {
    bool ok = false;
    std::vector<std::uint8_t> data;
    std::string error;
};

class JsonRpcClient {
public:
    explicit JsonRpcClient(const std::string& url, int max_attempts = 3,
                           std::chrono::milliseconds timeout = std::chrono::seconds(10));

    std::uint64_t chain_id();
    std::uint64_t block_number();
    eth::u256 gas_price();
    eth::u256 get_balance(const eth::Address& address, std::string_view block = "latest");
    std::uint64_t get_transaction_count(const eth::Address& address,
                                        std::string_view block = "pending");
    BlockHeader get_block_header(std::string_view block = "latest");

    std::vector<std::uint8_t> call(const TxCall& tx, std::string_view block = "latest");
    std::vector<CallResult> call_batch(std::span<const CallRequest> calls,
                                       std::string_view block = "latest");
    std::uint64_t estimate_gas(const TxCall& tx);

    std::vector<Log> get_logs(const LogFilter& filter);
    eth::Hash send_raw_transaction(std::span<const std::uint8_t> raw_tx);
    std::optional<Receipt> get_transaction_receipt(const eth::Hash& tx_hash);

    std::string request(std::string_view method, std::string_view params_json);

private:
    template <typename F>
    auto rpc(std::string_view method, const std::string& params, F&& on_result);
    std::string post(const std::string& body);

    HttpClient http_;
    int max_attempts_;
    std::uint64_t next_id_ = 1;
    simdjson::dom::parser parser_;
};

std::string block_tag(std::uint64_t number);

}
