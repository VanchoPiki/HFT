#pragma once

#include <atomic>
#include <memory>
#include <unordered_set>

#include "app/config.hpp"
#include "dex/market.hpp"
#include "dex/uniswap_abi.hpp"
#include "dex/v3_events.hpp"
#include "net/feed.hpp"
#include "net/json_rpc.hpp"
#include "strategy/two_pool_arb.hpp"
#include "telemetry/publisher.hpp"

namespace hft::app {

class Bot final : public net::FeedHandler {
public:
    explicit Bot(Config config);

    void bootstrap();
    void print_snapshot() const;
    void run(const std::atomic<bool>& stop);

    void on_connected() override;
    void on_block(const net::BlockHeader& header) override;
    void on_log(const net::Log& log) override;
    void on_idle() override;

private:
    void evaluate_pending();
    void resync();
    void refresh_gas_price(const net::BlockHeader& header);
    void handle(const strategy::Opportunity& opp);
    void print_group(const std::vector<dex::V3Pool*>& group, std::uint64_t block) const;
    net::LogFilter pool_filter() const;

    Config cfg_;
    telemetry::Publisher publisher_;
    std::unique_ptr<exec::Executor> executor_;
    net::JsonRpcClient rpc_;
    dex::UniswapV3Abi abi_;
    dex::V3EventDecoder decoder_;
    dex::Market market_;
    strategy::TwoPoolArb strategy_;
    std::unordered_set<const dex::V3Pool*> changed_;
    std::vector<const dex::V3Pool*> pending_;
    eth::Hash pending_tx_{};
    std::uint64_t pending_block_ = 0;
    std::uint64_t gas_checked_block_ = 0;
    std::uint64_t heartbeat_block_ = 0;
    bool bootstrapped_ = false;
    bool first_connect_ = true;
};

}
