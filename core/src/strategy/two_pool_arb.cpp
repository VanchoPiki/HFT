#include "strategy/two_pool_arb.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hft::strategy {
namespace {

using eth::i256;
using eth::u256;

constexpr double kInvPhi = 0.6180339887498949;  // golden ratio conjugate
constexpr double kWeiPerWeth = 1e18;

// Virtual reserves of a V3 pool at its current price: within the current tick range the
// pool behaves like x * y = L^2 with x = L / sqrtP (token0) and y = L * sqrtP (token1).
struct Side {
    double reserve_in;
    double reserve_out;
    double gamma;  // 1 - fee
};

Side side_of(const dex::V3Pool& pool, bool zero_for_one) {
    const double sqrt_p = static_cast<double>(pool.sqrt_price_x96()) / 0x1p96;
    const double l = static_cast<double>(pool.liquidity());
    const double x = l / sqrt_p;
    const double y = l * sqrt_p;
    const double gamma = 1.0 - pool.info().fee / 1e6;
    return zero_for_one ? Side{x, y, gamma} : Side{y, x, gamma};
}

// Two constant-product swaps chained: out = a*dx / (b + c*dx). Profit out - dx is maximal
// where a*b / (b + c*dx)^2 = 1, i.e. dx* = (sqrt(a*b) - b) / c.
double closed_form_optimum(const Side& first, const Side& second) {
    const double a = first.gamma * second.gamma * first.reserve_out * second.reserve_out;
    const double b = first.reserve_in * second.reserve_in;
    const double c = first.gamma * (second.reserve_in + second.gamma * first.reserve_out);
    if (a <= b || c <= 0) {
        return 0;
    }
    return (std::sqrt(a) * std::sqrt(b) - b) / c;
}

u256 to_u256(double v) {
    if (!(v >= 1.0)) {
        return 0;
    }
    // Exact for the integer part; v is far below 2^256 for any real pool.
    const int exp = std::ilogb(v);
    if (exp < 63) {
        return u256(static_cast<std::uint64_t>(v));
    }
    const auto mantissa = static_cast<std::uint64_t>(std::ldexp(v, 62 - exp));
    return u256(mantissa) << (exp - 62);
}

}  // namespace

TwoPoolArb::TwoPoolArb(const dex::Market& market, const eth::Address& weth, StrategyConfig config)
    : market_(market), weth_(weth), config_(config) {}

double TwoPoolArb::to_weth(const eth::Address& token, double amount) const {
    if (token == weth_) {
        return amount;
    }
    const dex::V3Pool* best = nullptr;
    for (const auto& group : market_.pair_groups()) {
        const auto& info = group.front()->info();
        const bool pairs_with_weth = (info.token0 == token && info.token1 == weth_) ||
                                     (info.token1 == token && info.token0 == weth_);
        if (!pairs_with_weth) {
            continue;
        }
        for (const dex::V3Pool* pool : group) {
            if (best == nullptr || pool->liquidity() > best->liquidity()) {
                best = pool;
            }
        }
    }
    if (best == nullptr || best->liquidity() == 0) {
        return 0;
    }
    const double price = best->raw_price();  // token1 per token0, raw units
    return best->info().token0 == token ? amount * price : amount / price;
}

std::optional<Opportunity> TwoPoolArb::evaluate(const dex::V3Pool& a, const dex::V3Pool& b,
                                                bool start_with_token0) const {
    if (a.liquidity() == 0 || b.liquidity() == 0 || !a.has_tick_data() || !b.has_tick_data()) {
        return std::nullopt;
    }
    const bool zfo_a = start_with_token0;  // first swap sells the start token in pool a
    const bool zfo_b = !start_with_token0;
    const Side first = side_of(a, zfo_a);
    const Side second = side_of(b, zfo_b);

    // Marginal rate of the whole cycle at size -> 0. The profit curve is concave, so if
    // the very first unit loses money, every size does.
    const double marginal = first.gamma * first.reserve_out / first.reserve_in * second.gamma *
                            second.reserve_out / second.reserve_in;
    if (marginal <= 1.0) {
        return std::nullopt;
    }

    const eth::Address& token_in = start_with_token0 ? a.info().token0 : a.info().token1;
    const double wei_per_unit = to_weth(token_in, 1.0);  // WETH wei per raw unit of token_in
    const double cap = wei_per_unit > 0 ? config_.max_input_weth * kWeiPerWeth / wei_per_unit
                                        : std::numeric_limits<double>::infinity();

    // Exact profit via the tick-by-tick simulation; -inf where the quote is unavailable.
    auto profit = [&](double x) -> double {
        const u256 in = to_u256(x);
        if (in == 0) {
            return 0.0;
        }
        const auto qa = a.quote_exact_input(zfo_a, in);
        if (!qa || qa->amount_out == 0) {
            return -std::numeric_limits<double>::infinity();
        }
        const auto qb = b.quote_exact_input(zfo_b, qa->amount_out);
        if (!qb) {
            return -std::numeric_limits<double>::infinity();
        }
        return static_cast<double>(qb->amount_out) - static_cast<double>(in);
    };

    // Bracket: start from the closed form (exact inside the current ticks) and widen while
    // the optimum sits on the upper edge (deeper liquidity after crossing ticks).
    double guess = std::min(closed_form_optimum(first, second), cap);
    if (guess < 1.0) {
        return std::nullopt;
    }
    double hi = std::min(3.0 * guess, cap);
    while (hi < cap && profit(hi) > profit(hi / 1.5)) {
        hi = std::min(hi * 2.0, cap);
    }

    // Golden-section search for the maximum of the concave profit on [0, hi]. Near the
    // optimum profit is flat (the error is quadratic), so 1e-5 relative is plenty.
    double lo = 0.0;
    double x1 = hi - kInvPhi * (hi - lo);
    double x2 = lo + kInvPhi * (hi - lo);
    double f1 = profit(x1);
    double f2 = profit(x2);
    while (hi - lo > std::max(1.0, 1e-5 * hi)) {
        if (f1 < f2) {
            lo = x1;
            x1 = x2;
            f1 = f2;
            x2 = lo + kInvPhi * (hi - lo);
            f2 = profit(x2);
        } else {
            hi = x2;
            x2 = x1;
            f2 = f1;
            x1 = hi - kInvPhi * (hi - lo);
            f1 = profit(x1);
        }
    }

    Opportunity opp;
    opp.amount_in = to_u256(f1 > f2 ? x1 : x2);
    const auto qa = a.quote_exact_input(zfo_a, opp.amount_in);
    if (!qa) {
        return std::nullopt;
    }
    const auto qb = b.quote_exact_input(zfo_b, qa->amount_out);
    if (!qb || qb->amount_out <= opp.amount_in) {
        return std::nullopt;
    }
    opp.pool_a = &a;
    opp.pool_b = &b;
    opp.token_in = token_in;
    opp.token_mid = start_with_token0 ? a.info().token1 : a.info().token0;
    opp.amount_mid = qa->amount_out;
    opp.amount_out = qb->amount_out;
    opp.gross_profit = static_cast<i256>(opp.amount_out) - static_cast<i256>(opp.amount_in);

    const u256 gas_wei = gas_price_ * config_.gas_units;
    opp.gas_cost = wei_per_unit > 0 ? to_u256(static_cast<double>(gas_wei) / wei_per_unit) : u256(0);
    opp.net_profit = opp.gross_profit - static_cast<i256>(opp.gas_cost);
    opp.net_profit_weth = to_weth(token_in, static_cast<double>(opp.net_profit)) / kWeiPerWeth;
    return opp;
}

std::optional<Opportunity> TwoPoolArb::on_pool_changed(const dex::V3Pool& changed,
                                                       std::uint64_t block) {
    using PoolPair = std::pair<const dex::V3Pool*, const dex::V3Pool*>;
    const auto started = std::chrono::steady_clock::now();
    std::optional<Opportunity> best;
    for (const dex::V3Pool* other : market_.group_of(changed)) {
        if (other == &changed) {
            continue;
        }
        for (const auto& [a, b] : {PoolPair{&changed, other}, PoolPair{other, &changed}}) {
            for (bool start0 : {true, false}) {
                auto opp = evaluate(*a, *b, start0);
                if (opp && (!best || opp->net_profit_weth > best->net_profit_weth)) {
                    best = std::move(opp);
                }
            }
        }
    }
    if (!best || best->net_profit_weth < config_.min_profit_weth) {
        return std::nullopt;
    }
    best->id = next_id_++;
    best->block = block;
    best->compute_time = std::chrono::steady_clock::now() - started;
    return best;
}

}  // namespace hft::strategy
