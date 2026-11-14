#include "net/json_rpc.hpp"

#include <algorithm>
#include <format>
#include <thread>

#include "eth/hex.hpp"

namespace hft::net {
namespace {

constexpr std::size_t kBatchChunk = 10;
constexpr int kRateLimitRounds = 5;

bool rate_limited(const RpcError& e) {
    const std::string_view m = e.what();
    return e.code == 429 || e.code == -32005 || m.find("capacity") != m.npos ||
           m.find("rate limit") != m.npos || m.find("Too Many") != m.npos;
}

std::string tx_json(const TxCall& tx) {
    std::string json = "{";
    if (tx.from) {
        json += R"("from":")" + tx.from->to_hex() + R"(",)";
    }
    json += R"("to":")" + tx.to.to_hex() + R"(","data":")" + eth::to_hex(tx.data) + '"';
    if (tx.value != 0) {
        json += R"(,"value":")" + eth::to_quantity(tx.value) + '"';
    }
    json += '}';
    return json;
}

std::string request_json(std::uint64_t id, std::string_view method, std::string_view params) {
    return std::format(R"({{"jsonrpc":"2.0","id":{},"method":"{}","params":{}}})", id, method,
                       params);
}

std::string_view as_string(simdjson::dom::element result) {
    std::string_view s;
    if (result.get(s) != simdjson::SUCCESS) {
        throw RpcError(-1, "rpc: expected a string result");
    }
    return s;
}

eth::u256 as_quantity(simdjson::dom::element result) {
    const auto v = eth::parse_quantity(as_string(result));
    if (!v) {
        throw RpcError(-1, "rpc: bad quantity result");
    }
    return *v;
}

std::uint64_t as_u64(simdjson::dom::element result) {
    const auto v = eth::parse_quantity_u64(as_string(result));
    if (!v) {
        throw RpcError(-1, "rpc: bad quantity result");
    }
    return *v;
}

}

std::string block_tag(std::uint64_t number) { return eth::to_quantity(number); }

JsonRpcClient::JsonRpcClient(const std::string& url, int max_attempts,
                             std::chrono::milliseconds timeout)
    : http_(url, timeout), max_attempts_(std::max(1, max_attempts)) {}

std::string JsonRpcClient::post(const std::string& body) {
    constexpr int kRateLimitRetries = 5;
    int rate_limited = 0;
    for (int attempt = 1;; ++attempt) {
        try {
            return http_.post_json(body);
        } catch (const HttpStatusError& e) {
            if (e.status == 429 && max_attempts_ > 1 && rate_limited < kRateLimitRetries) {
                std::this_thread::sleep_for(std::chrono::seconds(1) * (1 << rate_limited++));
                --attempt;
                continue;
            }
            if (e.status < 500 || attempt >= max_attempts_) {
                throw;
            }
        } catch (const std::exception&) {
            if (attempt >= max_attempts_) {
                throw;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250 * attempt));
    }
}

template <typename F>
auto JsonRpcClient::rpc(std::string_view method, const std::string& params, F&& on_result) {
    const std::string response = post(request_json(next_id_++, method, params));
    simdjson::dom::element doc;
    if (const auto err = parser_.parse(response).get(doc)) {
        throw RpcError(-1, std::string("rpc: invalid JSON response: ") + simdjson::error_message(err));
    }
    throw_if_rpc_error(doc);
    simdjson::dom::element result;
    if (doc["result"].get(result) != simdjson::SUCCESS) {
        throw RpcError(-1, "rpc: response without result");
    }
    return on_result(result);
}

std::string JsonRpcClient::request(std::string_view method, std::string_view params_json) {
    return rpc(method, std::string(params_json),
               [](simdjson::dom::element r) { return simdjson::minify(r); });
}

std::uint64_t JsonRpcClient::chain_id() { return rpc("eth_chainId", "[]", as_u64); }

std::uint64_t JsonRpcClient::block_number() { return rpc("eth_blockNumber", "[]", as_u64); }

eth::u256 JsonRpcClient::gas_price() { return rpc("eth_gasPrice", "[]", as_quantity); }

eth::u256 JsonRpcClient::get_balance(const eth::Address& address, std::string_view block) {
    return rpc("eth_getBalance", std::format(R"(["{}","{}"])", address.to_hex(), block),
               as_quantity);
}

std::uint64_t JsonRpcClient::get_transaction_count(const eth::Address& address,
                                                   std::string_view block) {
    return rpc("eth_getTransactionCount", std::format(R"(["{}","{}"])", address.to_hex(), block),
               as_u64);
}

BlockHeader JsonRpcClient::get_block_header(std::string_view block) {
    return rpc("eth_getBlockByNumber", std::format(R"(["{}",false])", block), parse_block_header);
}

std::vector<std::uint8_t> JsonRpcClient::call(const TxCall& tx, std::string_view block) {
    return rpc("eth_call", std::format(R"([{},"{}"])", tx_json(tx), block),
               [](simdjson::dom::element r) {
                   auto bytes = eth::from_hex(as_string(r));
                   if (!bytes) {
                       throw RpcError(-1, "rpc: bad eth_call hex");
                   }
                   return std::move(*bytes);
               });
}

std::vector<CallResult> JsonRpcClient::call_batch(std::span<const CallRequest> calls,
                                                  std::string_view block) {
    std::vector<CallResult> results(calls.size());
    std::vector<std::size_t> pending(calls.size());
    for (std::size_t i = 0; i < pending.size(); ++i) {
        pending[i] = i;
    }
    for (int round = 0; !pending.empty(); ++round) {
        std::vector<std::size_t> throttled;
        for (std::size_t start = 0; start < pending.size(); start += kBatchChunk) {
            const std::size_t end = std::min(pending.size(), start + kBatchChunk);
            std::string body = "[";
            for (std::size_t k = start; k < end; ++k) {
                const std::size_t i = pending[k];
                if (k > start) {
                    body += ',';
                }
                const TxCall tx{.to = calls[i].to, .data = calls[i].data};
                body += request_json(i, "eth_call", std::format(R"([{},"{}"])", tx_json(tx), block));
            }
            body += ']';

            const std::string response = post(body);
            simdjson::dom::element doc;
            if (const auto err = parser_.parse(response).get(doc)) {
                throw RpcError(-1, std::string("rpc: invalid batch response: ") +
                                       simdjson::error_message(err));
            }
            simdjson::dom::array items;
            if (doc.get(items) != simdjson::SUCCESS) {
                throw_if_rpc_error(doc);
                throw RpcError(-1, "rpc: batch response is not an array");
            }
            for (simdjson::dom::element item : items) {
                std::uint64_t id = 0;
                if (item["id"].get(id) != simdjson::SUCCESS || id >= calls.size()) {
                    continue;
                }
                CallResult& r = results[id];
                try {
                    throw_if_rpc_error(item);
                    auto bytes = eth::from_hex(as_string(item["result"].value()));
                    if (!bytes) {
                        throw RpcError(-1, "rpc: bad eth_call hex");
                    }
                    r.ok = true;
                    r.data = std::move(*bytes);
                } catch (const RpcError& e) {
                    r.error = e.what();
                    if (rate_limited(e)) {
                        throttled.push_back(id);
                    }
                } catch (const std::exception& e) {
                    r.error = e.what();
                }
            }
        }
        if (throttled.empty() || round + 1 >= kRateLimitRounds) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500) * (1 << round));
        pending = std::move(throttled);
    }
    return results;
}

std::uint64_t JsonRpcClient::estimate_gas(const TxCall& tx) {
    return rpc("eth_estimateGas", std::format("[{}]", tx_json(tx)), as_u64);
}

std::vector<Log> JsonRpcClient::get_logs(const LogFilter& filter) {
    return rpc("eth_getLogs", "[" + filter.to_json() + "]", [](simdjson::dom::element r) {
        std::vector<Log> logs;
        for (simdjson::dom::element log : r.get_array()) {
            logs.push_back(parse_log(log));
        }
        return logs;
    });
}

eth::Hash JsonRpcClient::send_raw_transaction(std::span<const std::uint8_t> raw_tx) {
    return rpc("eth_sendRawTransaction", std::format(R"(["{}"])", eth::to_hex(raw_tx)),
               [](simdjson::dom::element r) {
                   eth::Hash h{};
                   if (!eth::from_hex(as_string(r), h)) {
                       throw RpcError(-1, "rpc: bad transaction hash");
                   }
                   return h;
               });
}

std::optional<Receipt> JsonRpcClient::get_transaction_receipt(const eth::Hash& tx_hash) {
    return rpc("eth_getTransactionReceipt", std::format(R"(["{}"])", eth::to_hex(tx_hash)),
               [](simdjson::dom::element r) -> std::optional<Receipt> {
                   if (r.is_null()) {
                       return std::nullopt;
                   }
                   return parse_receipt(r);
               });
}

}
