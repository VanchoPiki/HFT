#include "exec/executor.hpp"

#include <format>

#include "eth/abi.hpp"
#include "eth/hex.hpp"
#include "eth/tx.hpp"
#include "net/json_rpc.hpp"
#include "telemetry/events.hpp"
#include "util/log.hpp"

namespace hft::exec {
namespace {

using Clock = std::chrono::steady_clock;

std::int64_t micros_since(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
}

std::string json_escape(std::string_view s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            out += ' ';
        } else {
            out += c;
        }
    }
    return out;
}

struct Attempt {
    std::string status;
    std::string tx_hash;
    std::uint64_t nonce = 0;
    eth::u256 max_fee;
    std::string error;
    std::int64_t simulate_us = 0;
    std::int64_t sign_us = 0;
    std::int64_t send_us = 0;
};

std::string execution_event(const Plan& plan, std::uint64_t gas_limit, const Attempt& a) {
    return std::format(
        R"({{"type":"execution","ts_ms":{},"run":{},"opportunity_id":{},"status":"{}","tx_hash":"{}",)"
        R"("nonce":{},"gas_limit":{},"max_fee_per_gas":"{}","error":"{}","simulate_us":{},)"
        R"("sign_us":{},"send_us":{}}})",
        telemetry::now_ms(), telemetry::run_id(), plan.opportunity_id, a.status, a.tx_hash, a.nonce,
        gas_limit, a.max_fee.str(), json_escape(a.error), a.simulate_us, a.sign_us, a.send_us);
}

}

Executor::Executor(ExecutionConfig config, const std::string& rpc_url,
                   std::chrono::milliseconds rpc_timeout, std::uint64_t chain_id,
                   eth::Signer signer, eth::abi::Contract executor_abi,
                   telemetry::Publisher& publisher)
    : config_(std::move(config)),
      chain_id_(chain_id),
      rpc_(rpc_url, 3, rpc_timeout),
      send_rpc_(rpc_url, 1, rpc_timeout),
      signer_(std::move(signer)),
      abi_(std::move(executor_abi)),
      arbitrage_topic_(abi_.event("Arbitrage").topic),
      publisher_(publisher),
      thread_([this] { run(); }) {
    log::info("executor: sending from {} through contract {}", signer_.address().to_checksum(),
              config_.contract.to_checksum());
}

Executor::~Executor() {
    queue_.close();
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool Executor::submit(const Plan& plan) {
    bool expected = false;
    if (!busy_.compare_exchange_strong(expected, true)) {
        return false;
    }
    if (!queue_.try_push(plan)) {
        busy_ = false;
        return false;
    }
    return true;
}

void Executor::run() {
    Plan plan;
    while (!queue_.closed_and_empty()) {
        if (!queue_.pop(plan, std::chrono::milliseconds(200))) {
            continue;
        }
        try {
            execute(plan);
        } catch (const std::exception& e) {
            log::error("executor: opportunity #{}: {}", plan.opportunity_id, e.what());
            nonce_known_ = false;
        }
        busy_ = false;
    }
}

eth::u256 Executor::current_gas_price() {
    if (gas_price_ == 0 || Clock::now() - gas_price_at_ > std::chrono::seconds(5)) {
        gas_price_ = rpc_.gas_price();
        gas_price_at_ = Clock::now();
    }
    return gas_price_;
}

void Executor::execute(const Plan& plan) {
    auto& rpc = rpc_;
    auto& send_rpc = send_rpc_;
    Attempt attempt;

    const auto calldata = eth::abi::Encoder(abi_.function("execute").selector)
                              .add_address(plan.pool_a)
                              .add_address(plan.pool_b)
                              .add_address(plan.token_in)
                              .add_address(plan.token_out)
                              .add_uint(plan.amount_in)
                              .add_uint(plan.min_profit)
                              .finish();

    if (config_.simulate) {
        const auto start = Clock::now();
        try {
            (void)rpc.call({.from = signer_.address(), .to = config_.contract, .data = calldata});
            attempt.simulate_us = micros_since(start);
        } catch (const net::RpcError& e) {
            attempt.simulate_us = micros_since(start);
            attempt.status = "simulation_failed";
            const auto data = eth::from_hex(e.data);
            const std::string reason = data ? abi_.error_name(*data) : "";
            attempt.error = reason.empty() ? e.what() : reason;
            publisher_.publish(execution_event(plan, config_.gas_limit, attempt));
            log::info("executor: #{} simulation reverted ({}) in {} us - not sending",
                      plan.opportunity_id, attempt.error, attempt.simulate_us);
            return;
        }
    }

    if (!nonce_known_) {
        nonce_ = rpc.get_transaction_count(signer_.address(), "pending");
        nonce_known_ = true;
    }
    eth::Eip1559Tx tx;
    tx.chain_id = chain_id_;
    tx.nonce = nonce_;
    tx.max_priority_fee_per_gas = 0;
    const auto multiplier_permille = static_cast<std::uint64_t>(config_.max_fee_multiplier * 1000);
    tx.max_fee_per_gas = current_gas_price() * multiplier_permille / 1000;
    tx.gas_limit = config_.gas_limit;
    tx.to = config_.contract;
    tx.data = calldata;
    attempt.nonce = tx.nonce;
    attempt.max_fee = tx.max_fee_per_gas;

    auto start = Clock::now();
    const auto raw = eth::sign_transaction(tx, signer_);
    attempt.sign_us = micros_since(start);

    start = Clock::now();
    eth::Hash tx_hash{};
    try {
        tx_hash = send_rpc.send_raw_transaction(raw);
    } catch (const std::exception& e) {
        attempt.send_us = micros_since(start);
        attempt.status = "send_failed";
        attempt.error = e.what();
        publisher_.publish(execution_event(plan, config_.gas_limit, attempt));
        log::warn("executor: #{} send failed: {}", plan.opportunity_id, e.what());
        nonce_known_ = false;
        return;
    }
    attempt.send_us = micros_since(start);
    attempt.status = "sent";
    attempt.tx_hash = eth::to_hex(tx_hash);
    ++nonce_;
    ++sent_;
    publisher_.publish(execution_event(plan, config_.gas_limit, attempt));
    log::info("executor: #{} sent {} (nonce {}) | simulate {} us, sign {} us, send {} us",
              plan.opportunity_id, attempt.tx_hash, tx.nonce, attempt.simulate_us, attempt.sign_us,
              attempt.send_us);

    const auto deadline = Clock::now() + config_.receipt_timeout;
    std::optional<net::Receipt> receipt;
    while (!receipt && Clock::now() < deadline) {
        receipt = rpc.get_transaction_receipt(tx_hash);
        if (!receipt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    if (!receipt) {
        log::warn("executor: #{} no receipt after {} ms", plan.opportunity_id,
                  config_.receipt_timeout.count());
        nonce_known_ = false;
        return;
    }
    eth::u256 profit = 0;
    for (const auto& l : receipt->logs) {
        if (l.address == config_.contract && !l.topics.empty() && l.topics[0] == arbitrage_topic_) {
            eth::abi::Reader data(l.data);
            data.skip();
            profit = data.read_uint();
        }
    }
    const double gas_eth = static_cast<double>(receipt->effective_gas_price) *
                           static_cast<double>(receipt->gas_used) / 1e18;
    const double profit_weth = static_cast<double>(profit) * plan.wei_per_unit / 1e18 - gas_eth;
    publisher_.publish(std::format(
        R"({{"type":"tx_result","ts_ms":{},"run":{},"opportunity_id":{},"tx_hash":"{}","success":{},)"
        R"("block":{},"gas_used":{},"effective_gas_price":"{}","realized_profit":"{}",)"
        R"("realized_profit_weth":{:.12g}}})",
        telemetry::now_ms(), telemetry::run_id(), plan.opportunity_id, attempt.tx_hash,
        receipt->success ? "true" : "false", receipt->block_number, receipt->gas_used,
        receipt->effective_gas_price.str(), profit.str(), profit_weth));
    log::info("executor: #{} mined in block {}: {} | gas {} | profit {} raw, {:+.8f} WETH after gas",
              plan.opportunity_id, receipt->block_number, receipt->success ? "SUCCESS" : "REVERTED",
              receipt->gas_used, profit.str(), profit_weth);
}

}
