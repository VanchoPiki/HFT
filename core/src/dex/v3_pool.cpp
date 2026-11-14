#include "dex/v3_pool.hpp"

#include <algorithm>
#include <limits>

#include "dex/v3_math.hpp"

namespace hft::dex {
namespace {

std::pair<int, bool> next_initialized_tick_within_one_word(const std::map<int, TickInfo>& ticks,
                                                            int tick, int spacing, bool lte) {
    int compressed = tick / spacing;
    if (tick < 0 && tick % spacing != 0) {
        --compressed;
    }
    if (lte) {
        const int word_start = compressed - (compressed & 0xff);
        auto it = ticks.upper_bound(compressed * spacing);
        if (it != ticks.begin() && (--it)->first >= word_start * spacing) {
            return {it->first, true};
        }
        return {word_start * spacing, false};
    }
    const int next = compressed + 1;
    const int word_end = next + (255 - (next & 0xff));
    const auto it = ticks.lower_bound(next * spacing);
    if (it != ticks.end() && it->first <= word_end * spacing) {
        return {it->first, true};
    }
    return {word_end * spacing, false};
}

}

void V3Pool::reset(const eth::u256& sqrt_price_x96, int tick, const eth::u256& liquidity,
                   std::uint64_t block) {
    sqrt_price_x96_ = sqrt_price_x96;
    tick_ = tick;
    liquidity_ = liquidity;
    block_ = block;
    log_index_ = std::numeric_limits<std::uint64_t>::max();
    ticks_.clear();
    known_min_tick_ = 1;
    known_max_tick_ = 0;
}

void V3Pool::set_tick(int tick, const eth::i256& liquidity_net, const eth::u256& liquidity_gross) {
    ticks_[tick] = TickInfo{liquidity_net, liquidity_gross};
}

void V3Pool::set_known_tick_range(int min_tick, int max_tick) {
    known_min_tick_ = min_tick;
    known_max_tick_ = max_tick;
}

bool V3Pool::advance(std::uint64_t block, std::uint64_t log_index) {
    if (block < block_ || (block == block_ && log_index <= log_index_)) {
        return false;
    }
    block_ = block;
    log_index_ = log_index;
    return true;
}

void V3Pool::update_tick(int tick, const eth::i256& delta, bool upper) {
    if (!has_tick_data() || tick < known_min_tick_ || tick > known_max_tick_) {
        return;
    }
    TickInfo& info = ticks_[tick];
    const eth::i256 gross = static_cast<eth::i256>(info.liquidity_gross) + delta;
    if (gross <= 0) {
        ticks_.erase(tick);
        return;
    }
    info.liquidity_gross = static_cast<eth::u256>(gross);
    info.liquidity_net += upper ? eth::i256(-delta) : delta;
}

bool V3Pool::apply(const PoolEvent& event, std::uint64_t block, std::uint64_t log_index) {
    if (std::holds_alternative<std::monostate>(event) || !advance(block, log_index)) {
        return false;
    }
    if (const auto* swap = std::get_if<SwapEvent>(&event)) {
        sqrt_price_x96_ = swap->sqrt_price_x96;
        liquidity_ = swap->liquidity;
        tick_ = swap->tick;
        return true;
    }
    const auto& liq = std::get<LiquidityEvent>(event);
    if (liq.liquidity_delta == 0) {
        return true;
    }
    update_tick(liq.tick_lower, liq.liquidity_delta, false);
    update_tick(liq.tick_upper, liq.liquidity_delta, true);
    if (liq.tick_lower <= tick_ && tick_ < liq.tick_upper) {
        liquidity_ = static_cast<eth::u256>(static_cast<eth::i256>(liquidity_) + liq.liquidity_delta);
    }
    return true;
}

double V3Pool::raw_price() const {
    const double sqrt_price = static_cast<double>(sqrt_price_x96_) / 0x1p96;
    return sqrt_price * sqrt_price;
}

std::optional<Quote> V3Pool::quote_exact_input(bool zero_for_one, const eth::u256& amount_in) const {
    if (!has_tick_data() || amount_in == 0) {
        return std::nullopt;
    }
    const eth::u256 limit = zero_for_one ? v3::kMinSqrtRatio + 1 : v3::kMaxSqrtRatio - 1;
    eth::u256 remaining = amount_in;
    eth::u256 sqrt_price = sqrt_price_x96_;
    eth::u256 liquidity = liquidity_;
    int tick = tick_;
    Quote quote;

    while (remaining != 0 && sqrt_price != limit) {
        const eth::u256 start = sqrt_price;
        auto [tick_next, initialized] =
            next_initialized_tick_within_one_word(ticks_, tick, info_.tick_spacing, zero_for_one);
        tick_next = std::clamp(tick_next, v3::kMinTick, v3::kMaxTick);
        if (tick_next < known_min_tick_ || tick_next > known_max_tick_) {
            return std::nullopt;
        }
        const eth::u256 sqrt_next = v3::sqrt_ratio_at_tick(tick_next);
        const bool past_limit = zero_for_one ? sqrt_next < limit : sqrt_next > limit;
        const v3::SwapStep step = v3::compute_swap_step(sqrt_price, past_limit ? limit : sqrt_next,
                                                        liquidity, remaining, info_.fee);
        sqrt_price = step.sqrt_price_next;
        remaining -= step.amount_in + step.fee_amount;
        quote.amount_out += step.amount_out;

        if (sqrt_price == sqrt_next) {
            if (initialized) {
                const eth::i256& net = ticks_.at(tick_next).liquidity_net;
                liquidity = v3::add_delta(liquidity, zero_for_one ? eth::i256(-net) : net);
                ++quote.ticks_crossed;
            }
            tick = zero_for_one ? tick_next - 1 : tick_next;
        } else if (sqrt_price != start) {
            tick = v3::tick_at_sqrt_ratio(sqrt_price);
        }
    }
    quote.sqrt_price_after = sqrt_price;
    quote.tick_after = tick;
    return quote;
}

}
