#include "app/bot.hpp"

#include <algorithm>
#include <cstdio>
#include <format>
#include <limits>
#include <stdexcept>

#include "app/setup.hpp"
#include "telemetry/events.hpp"
#include "util/log.hpp"

namespace hft::app {

Bot::Bot(Config config)
    : cfg_(std::move(config)),
      publisher_(cfg_.telemetry_endpoint),
      rpc_(cfg_.rpc_http, 3, cfg_.rpc_timeout),
      abi_(dex::UniswapV3Abi::load(cfg_.abi_dir)),
      decoder_(abi_),
      strategy_(market_, cfg_.token_address("WETH"), cfg_.strategy) {
    telemetry::set_run_id(telemetry::now_ms());
    if (!cfg_.execution.enabled) {
        return;
    }
    if (cfg_.execution.contract.is_zero()) {
        throw std::runtime_error(
            "bot: execution is enabled but no executor contract is set "
            "(deploy contracts/src/ArbExecutor.sol and set HFT_EXECUTOR)");
    }
    const char* key = std::getenv(cfg_.execution.private_key_env.c_str());
    if (key == nullptr || *key == '\0') {
        throw std::runtime_error("bot: execution is enabled but $" + cfg_.execution.private_key_env +
                                 " is not set");
    }
    executor_ = std::make_unique<exec::Executor>(
        cfg_.execution, cfg_.rpc_http, cfg_.rpc_timeout, cfg_.chain_id, eth::Signer::from_hex(key),
        eth::abi::Contract::load_file(cfg_.abi_dir / "ArbExecutor.json"), publisher_);
}

void Bot::bootstrap() {
    market_ = discover_market(cfg_, rpc_, abi_);
    resync();
    strategy_.set_gas_price(rpc_.gas_price());
    bootstrapped_ = true;
    log::info("bot: tracking {} pools in {} pairs", market_.pools().size(),
              market_.pair_groups().size());
}

void Bot::resync() {
    const std::uint64_t block = rpc_.block_number();
    sync_market(cfg_, rpc_, abi_, market_, block);
    log::info("bot: pool state and ticks synced at block {}", block);
}

void Bot::print_snapshot() const {
    std::printf("%-12s %-7s %-44s %16s %12s\n", "pair", "fee", "pool", "price", "liquidity");
    for (const auto& group : market_.pair_groups()) {
        for (const dex::V3Pool* pool : group) {
            std::printf("%-12s %-7s %-44s %16.6f %12.3e\n", market_.pair_name(*pool).c_str(),
                        dex::fee_label(pool->info().fee).c_str(),
                        pool->info().address.to_checksum().c_str(), market_.human_price(*pool),
                        static_cast<double>(pool->liquidity()));
        }
    }
}

// One line per pair: price in each pool and the best two-pool edge after fees.
// edge = spread between two pools minus both fees (in bps); > 0 means a possible arbitrage.
void Bot::print_group(const std::vector<dex::V3Pool*>& group, std::uint64_t block) const {
    std::string line = std::format("#{} {:<12}", block, market_.pair_name(*group.front()));
    double best_edge = -std::numeric_limits<double>::infinity();
    for (const dex::V3Pool* buy : group) {
        if (buy->liquidity() == 0) {
            continue;
        }
        line += std::format(" {}={:.6g}", dex::fee_label(buy->info().fee), market_.human_price(*buy));
        for (const dex::V3Pool* sell : group) {
            if (sell == buy || sell->liquidity() == 0) {
                continue;
            }
            const double spread = sell->raw_price() / buy->raw_price() - 1.0;
            const double fees = (buy->info().fee + sell->info().fee) / 1e6;
            best_edge = std::max(best_edge, (spread - fees) * 1e4);
        }
    }
    if (best_edge > -std::numeric_limits<double>::infinity()) {
        line += std::format("  | best edge {:+.2f} bps", best_edge);
    }
    std::printf("%s\n", line.c_str());
    std::fflush(stdout);
}

net::LogFilter Bot::pool_filter() const {
    net::LogFilter filter;
    for (const auto& pool : market_.pools()) {
        filter.addresses.push_back(pool.info().address);
    }
    filter.topic0 = {abi_.swap_topic, abi_.mint_topic, abi_.burn_topic};
    return filter;
}

void Bot::run(const std::atomic<bool>& stop) {
    for (const dex::V3Pool& pool : market_.pools()) {
        publisher_.publish(telemetry::pool_snapshot_event(market_, pool, pool.block()));
    }
    std::unique_ptr<net::Feed> feed;
    if (!cfg_.rpc_ws.empty()) {
        feed = std::make_unique<net::WsFeed>(cfg_.rpc_ws, pool_filter());
    } else {
        feed = std::make_unique<net::PollingFeed>(cfg_.rpc_http, pool_filter(), cfg_.poll_interval);
    }
    feed->run(*this, stop);
}

void Bot::on_connected() {
    pending_.clear();
    if (first_connect_ && bootstrapped_) {
        first_connect_ = false;  // state was just loaded by bootstrap()
        return;
    }
    resync();
}

void Bot::on_log(const net::Log& log) {
    if (log.removed) {
        log::warn("bot: reorg removed a log in block {}, resyncing", log.block_number);
        pending_.clear();
        resync();
        return;
    }
    dex::V3Pool* pool = market_.find_pool(log.address);
    if (pool == nullptr) {
        return;
    }
    if (log.tx_hash != pending_tx_ || log.block_number != pending_block_) {
        evaluate_pending();  // the previous transaction is complete
        pending_tx_ = log.tx_hash;
        pending_block_ = log.block_number;
    }
    if (pool->apply(decoder_.decode(log), log.block_number, log.log_index)) {
        changed_.insert(pool);
        if (std::find(pending_.begin(), pending_.end(), pool) == pending_.end()) {
            pending_.push_back(pool);
        }
    }
}

void Bot::on_idle() { evaluate_pending(); }

void Bot::evaluate_pending() {
    // The hot path: best cycle through any pool the last transaction touched.
    std::optional<strategy::Opportunity> best;
    for (const dex::V3Pool* pool : pending_) {
        auto opp = strategy_.on_pool_changed(*pool, pending_block_);
        if (opp && (!best || opp->net_profit_weth > best->net_profit_weth)) {
            best = std::move(opp);
        }
    }
    pending_.clear();
    if (best) {
        handle(*best);
    }
}

void Bot::handle(const strategy::Opportunity& opp) {
    if (executor_) {
        // The contract must keep at least this much, or the whole transaction reverts.
        const auto share = static_cast<std::uint64_t>(cfg_.execution.min_profit_share * 1000);
        const eth::u256 gross = static_cast<eth::u256>(opp.gross_profit);
        const exec::Plan plan{.opportunity_id = opp.id,
                              .pool_a = opp.pool_a->info().address,
                              .pool_b = opp.pool_b->info().address,
                              .token_in = opp.token_in,
                              .token_out = opp.token_mid,
                              .amount_in = opp.amount_in,
                              .min_profit = std::max(opp.gas_cost, gross * share / 1000),
                              .wei_per_unit = strategy_.to_weth(opp.token_in, 1.0)};
        if (!executor_->submit(plan)) {
            log::debug("bot: executor busy, skipping opportunity #{}", opp.id);
        }
    }
    publisher_.publish(telemetry::opportunity_event(market_, opp));
    const auto& in = opp.token_in;
    log::info(
        "OPP #{} block {} {}: sell {} {} in {} pool -> buy back in {} pool | out {:.6f}, "
        "gross {:+.6f}, gas {:.6f}, net {:+.6f} {} ({:+.6f} WETH) | computed in {} us",
        opp.id, opp.block, market_.pair_name(*opp.pool_a),
        market_.human_amount(in, static_cast<double>(opp.amount_in)), market_.token(in).symbol,
        dex::fee_label(opp.pool_a->info().fee), dex::fee_label(opp.pool_b->info().fee),
        market_.human_amount(in, static_cast<double>(opp.amount_out)),
        market_.human_amount(in, static_cast<double>(opp.gross_profit)),
        market_.human_amount(in, static_cast<double>(opp.gas_cost)),
        market_.human_amount(in, static_cast<double>(opp.net_profit)), market_.token(in).symbol,
        opp.net_profit_weth,
        std::chrono::duration_cast<std::chrono::microseconds>(opp.compute_time).count());
}

void Bot::refresh_gas_price(const net::BlockHeader& header) {
    if (header.base_fee > 0) {
        strategy_.set_gas_price(header.base_fee);  // WebSocket heads carry it for free
        return;
    }
    constexpr std::uint64_t kEveryBlocks = 240;  // ~1 minute on Arbitrum
    if (header.number >= gas_checked_block_ + kEveryBlocks) {
        gas_checked_block_ = header.number;
        try {
            strategy_.set_gas_price(rpc_.gas_price());
        } catch (const std::exception& e) {
            log::warn("bot: gas price refresh failed: {}", e.what());
        }
    }
}

void Bot::on_block(const net::BlockHeader& header) {
    evaluate_pending();
    refresh_gas_price(header);
    constexpr std::uint64_t kHeartbeatBlocks = 40;  // ~10 s on Arbitrum
    if (header.number >= heartbeat_block_ + kHeartbeatBlocks) {
        heartbeat_block_ = header.number;
        publisher_.publish(telemetry::heartbeat_event(cfg_.chain_id, header.number,
                                                      market_.pools().size(),
                                                      executor_ ? "execute" : "monitor",
                                                      publisher_.dropped()));
    }
    if (changed_.empty()) {
        return;
    }
    for (const auto& group : market_.pair_groups()) {
        bool printed = false;
        for (const dex::V3Pool* pool : group) {
            if (changed_.contains(pool)) {
                publisher_.publish(telemetry::pool_snapshot_event(market_, *pool, header.number));
                if (!printed) {
                    print_group(group, header.number);
                    printed = true;
                }
            }
        }
    }
    changed_.clear();
}

}  // namespace hft::app
