#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "eth/abi_json.hpp"
#include "eth/signer.hpp"
#include "net/json_rpc.hpp"
#include "telemetry/bounded_queue.hpp"
#include "telemetry/publisher.hpp"

namespace hft::exec {

struct ExecutionConfig {
    bool enabled = false;
    eth::Address contract;
    std::string private_key_env = "PRIVATE_KEY";
    std::uint64_t gas_limit = 1'500'000;
    double max_fee_multiplier = 2.0;
    double min_profit_share = 0.5;
    bool simulate = true;
    std::chrono::milliseconds receipt_timeout{30'000};
};

struct Plan {
    std::uint64_t opportunity_id = 0;
    eth::Address pool_a;
    eth::Address pool_b;
    eth::Address token_in;
    eth::Address token_out;
    eth::u256 amount_in;
    eth::u256 min_profit;
    double wei_per_unit = 0;
};

class Executor {
public:
    Executor(ExecutionConfig config, const std::string& rpc_url,
             std::chrono::milliseconds rpc_timeout, std::uint64_t chain_id, eth::Signer signer,
             eth::abi::Contract executor_abi, telemetry::Publisher& publisher);
    ~Executor();
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

    bool submit(const Plan& plan);

    const eth::Address& address() const { return signer_.address(); }
    std::uint64_t sent() const { return sent_; }

private:
    void run();
    void execute(const Plan& plan);
    eth::u256 current_gas_price();

    ExecutionConfig config_;
    std::uint64_t chain_id_;
    net::JsonRpcClient rpc_;
    net::JsonRpcClient send_rpc_;
    eth::Signer signer_;
    eth::abi::Contract abi_;
    eth::Hash arbitrage_topic_{};
    telemetry::Publisher& publisher_;

    telemetry::BoundedQueue<Plan> queue_{1};
    std::atomic<bool> busy_{false};
    std::atomic<std::uint64_t> sent_{0};
    std::uint64_t nonce_ = 0;
    bool nonce_known_ = false;
    eth::u256 gas_price_;
    std::chrono::steady_clock::time_point gas_price_at_{};
    std::thread thread_;
};

}
