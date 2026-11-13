#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "dex/market.hpp"

namespace hft::strategy {

struct StrategyConfig {
    double min_profit_weth = 0.00001;      // net of gas
    std::uint64_t gas_units = 350'000;     // executor tx with two swaps, incl. Arbitrum L1 part
    double max_input_weth = 50.0;          // risk cap per trade, in WETH value
};

// A two-pool cycle in one token pair: swap amount_in of token_in to token_mid in pool_a,
// then everything back to token_in in pool_b. Profit stays in token_in.
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
    eth::i256 gross_profit;  // amount_out - amount_in, token_in units
    eth::u256 gas_cost;      // token_in units
    eth::i256 net_profit;
    double net_profit_weth = 0;
    std::chrono::nanoseconds compute_time{0};
};

class TwoPoolArb {
public:
    TwoPoolArb(const dex::Market& market, const eth::Address& weth, StrategyConfig config);

    void set_gas_price(const eth::u256& wei_per_gas) { gas_price_ = wei_per_gas; }
    const eth::u256& gas_price() const { return gas_price_; }

    // Best cycle between `changed` and the other pools of its pair that clears the
    // minimum profit, or nullopt.
    std::optional<Opportunity> on_pool_changed(const dex::V3Pool& changed, std::uint64_t block);

    // Optimal size and exact profit (before the min-profit filter) of one cycle:
    // token0 -> token1 in `a` and back in `b` if start_with_token0, else the other way.
    // nullopt if there is no positive gross profit at any size.
    std::optional<Opportunity> evaluate(const dex::V3Pool& a, const dex::V3Pool& b,
                                        bool start_with_token0) const;

    // Value of `amount` raw units of `token` in WETH wei, via the deepest pool pairing it
    // with WETH (0 if there is none).
    double to_weth(const eth::Address& token, double amount) const;

private:
    const dex::Market& market_;
    eth::Address weth_;
    StrategyConfig config_;
    eth::u256 gas_price_{10'000'000};  // 0.01 gwei, typical Arbitrum base fee
    std::uint64_t next_id_ = 1;
};

}  // namespace hft::strategy
