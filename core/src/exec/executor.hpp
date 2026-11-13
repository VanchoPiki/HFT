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
    eth::Address contract;                   // deployed ArbExecutor (owner = our key)
    std::string private_key_env = "PRIVATE_KEY";
    std::uint64_t gas_limit = 1'500'000;     // Arbitrum charges gas used, not the limit
    double max_fee_multiplier = 2.0;         // maxFeePerGas = current gas price * this
    double min_profit_share = 0.5;           // contract reverts below this share of the expected gross profit
    bool simulate = true;                    // eth_call the exact transaction before sending it
    std::chrono::milliseconds receipt_timeout{30'000};
};

// Everything the executor thread needs, copied out of the strategy's Opportunity so that
// pool state (owned by the feed thread) is never touched from another thread.
struct Plan {
    std::uint64_t opportunity_id = 0;
    eth::Address pool_a;
    eth::Address pool_b;
    eth::Address token_in;
    eth::Address token_out;
    eth::u256 amount_in;
    eth::u256 min_profit;  // token_in units, enforced on-chain
    double wei_per_unit = 0;  // WETH wei per raw token_in unit, to report PnL in WETH
};

// Runs simulate -> sign -> send -> wait for receipt on its own thread, one transaction at a
// time, and reports every step to telemetry (`execution`, `tx_result` events).
class Executor {
public:
    Executor(ExecutionConfig config, const std::string& rpc_url,
             std::chrono::milliseconds rpc_timeout, std::uint64_t chain_id, eth::Signer signer,
             eth::abi::Contract executor_abi, telemetry::Publisher& publisher);
    ~Executor();
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

    // Hot path. Never blocks: returns false and drops the plan while a transaction is in flight.
    bool submit(const Plan& plan);

    const eth::Address& address() const { return signer_.address(); }
    std::uint64_t sent() const { return sent_; }

private:
    void run();
    void execute(const Plan& plan);
    eth::u256 current_gas_price();

    ExecutionConfig config_;
    std::uint64_t chain_id_;
    // Used only from the executor thread; kept open so no request pays for a new TLS handshake.
    net::JsonRpcClient rpc_;       // reads: retried
    net::JsonRpcClient send_rpc_;  // eth_sendRawTransaction: never blindly retried
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

}  // namespace hft::exec
