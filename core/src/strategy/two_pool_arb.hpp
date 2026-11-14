#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "dex/market.hpp"

namespace hft::strategy {

struct StrategyConfig {
    double min_profit_weth = 0.00001;
    std::uint64_t gas_units = 350'000;
    double max_input_weth = 50.0;
};

struct Opportunity {
    std::uint64_t id = 0;
    std::uint64_t block = 0;
    const dex::V3Pool* pool_a = nullptr;
    const dex::V3Pool* pool_b = nullptr;
    eth::Address token_in;
    eth::Address token_mid;
    eth::u256 amount_in;
    eth::u256 amount_mid;
    eth::u256 amount_out;
    eth::i256 gross_profit;
    eth::u256 gas_cost;
    eth::i256 net_profit;
    double net_profit_weth = 0;
    std::chrono::nanoseconds compute_time{0};
};

class TwoPoolArb {
public:
    TwoPoolArb(const dex::Market& market, const eth::Address& weth, StrategyConfig config);

    void set_gas_price(const eth::u256& wei_per_gas) { gas_price_ = wei_per_gas; }
    const eth::u256& gas_price() const { return gas_price_; }

    std::optional<Opportunity> on_pool_changed(const dex::V3Pool& changed, std::uint64_t block);

    std::optional<Opportunity> evaluate(const dex::V3Pool& a, const dex::V3Pool& b,
                                        bool start_with_token0) const;

    double to_weth(const eth::Address& token, double amount) const;

private:
    const dex::Market& market_;
    eth::Address weth_;
    StrategyConfig config_;
    eth::u256 gas_price_{10'000'000};
    std::uint64_t next_id_ = 1;
};

}
