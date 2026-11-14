#pragma once

#include <cstdint>
#include <map>
#include <optional>

#include "dex/v3_events.hpp"
#include "eth/address.hpp"
#include "eth/u256.hpp"

namespace hft::dex {

struct PoolInfo {
    eth::Address address;
    eth::Address token0;
    eth::Address token1;
    std::uint32_t fee = 0;
    int tick_spacing = 0;
};

struct TickInfo {
    eth::i256 liquidity_net;
    eth::u256 liquidity_gross;
};

struct Quote {
    eth::u256 amount_out;
    eth::u256 sqrt_price_after;
    int tick_after = 0;
    int ticks_crossed = 0;
};

class V3Pool {
public:
    explicit V3Pool(PoolInfo info) : info_(info) {}

    const PoolInfo& info() const { return info_; }
    const eth::u256& sqrt_price_x96() const { return sqrt_price_x96_; }
    int tick() const { return tick_; }
    const eth::u256& liquidity() const { return liquidity_; }
    std::uint64_t block() const { return block_; }
    const std::map<int, TickInfo>& ticks() const { return ticks_; }

    void reset(const eth::u256& sqrt_price_x96, int tick, const eth::u256& liquidity,
               std::uint64_t block);
    void set_tick(int tick, const eth::i256& liquidity_net, const eth::u256& liquidity_gross);
    void set_known_tick_range(int min_tick, int max_tick);
    int known_min_tick() const { return known_min_tick_; }
    int known_max_tick() const { return known_max_tick_; }
    bool has_tick_data() const { return known_min_tick_ <= known_max_tick_; }

    bool apply(const PoolEvent& event, std::uint64_t block, std::uint64_t log_index);

    double raw_price() const;

    std::optional<Quote> quote_exact_input(bool zero_for_one, const eth::u256& amount_in) const;

private:
    bool advance(std::uint64_t block, std::uint64_t log_index);
    void update_tick(int tick, const eth::i256& delta, bool upper);

    PoolInfo info_;
    eth::u256 sqrt_price_x96_;
    int tick_ = 0;
    eth::u256 liquidity_;
    std::uint64_t block_ = 0;
    std::uint64_t log_index_ = 0;
    std::map<int, TickInfo> ticks_;
    int known_min_tick_ = 1;
    int known_max_tick_ = 0;
};

}
