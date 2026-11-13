#include <benchmark/benchmark.h>

#include <cmath>

#include "dex/v3_math.hpp"
#include "eth/keccak.hpp"
#include "strategy/two_pool_arb.hpp"

namespace {

using hft::eth::u256;
namespace dex = hft::dex;

const auto kToken0 = *hft::eth::Address::from_hex("0x1000000000000000000000000000000000000000");
const auto kToken1 = *hft::eth::Address::from_hex("0x2000000000000000000000000000000000000000");

u256 sqrt_price_x96(double price) {
    return u256(static_cast<std::uint64_t>(std::sqrt(price) * 0x1p62)) << 34;
}

// A pool with 100 stacked positions around the price (one every 60 ticks), so a large
// swap has to cross many initialized ticks like on a real WETH/USDC pool.
dex::V3Pool& add_pool(dex::Market& m, std::uint8_t n, std::uint32_t fee, double price) {
    hft::eth::Address address;
    address.bytes[19] = n;
    dex::V3Pool& pool = m.add_pool({address, kToken0, kToken1, fee, 60});
    const u256 sqrt_p = sqrt_price_x96(price);
    const int tick = dex::v3::tick_at_sqrt_ratio(sqrt_p);
    const u256 per_position("100000000000000000000");
    pool.reset(sqrt_p, tick, per_position * 50, 1);
    pool.set_known_tick_range(tick - 20000, tick + 20000);
    const int center = tick / 60 * 60;
    for (int k = 1; k <= 50; ++k) {
        pool.set_tick(center - 60 * k, hft::eth::i256(per_position), per_position);
        pool.set_tick(center + 60 * k, -hft::eth::i256(per_position), per_position);
    }
    return pool;
}

struct Fixture {
    Fixture() {
        market.add_token({kToken0, "T0", 18});
        market.add_token({kToken1, "T1", 18});
        cheap = &add_pool(market, 1, 500, 1.00);
        rich = &add_pool(market, 2, 3000, 1.02);
    }
    dex::Market market;
    dex::V3Pool* cheap;
    dex::V3Pool* rich;
};

void BM_Keccak256_64B(benchmark::State& state) {
    std::array<std::uint8_t, 64> data{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(hft::eth::keccak256(data));
    }
}
BENCHMARK(BM_Keccak256_64B);

void BM_SqrtRatioAtTick(benchmark::State& state) {
    int tick = -198736;
    for (auto _ : state) {
        benchmark::DoNotOptimize(dex::v3::sqrt_ratio_at_tick(tick));
        tick ^= 1;
    }
}
BENCHMARK(BM_SqrtRatioAtTick);

void BM_QuoteWithinOneTick(benchmark::State& state) {
    Fixture f;
    const u256 amount("1000000000000000");
    for (auto _ : state) {
        benchmark::DoNotOptimize(f.cheap->quote_exact_input(true, amount));
    }
}
BENCHMARK(BM_QuoteWithinOneTick)->Unit(benchmark::kMicrosecond);

void BM_QuoteCrossingTicks(benchmark::State& state) {
    Fixture f;
    const u256 amount("250000000000000000000");  // moves the price ~12%
    int crossed = 0;
    for (auto _ : state) {
        auto q = f.cheap->quote_exact_input(true, amount);
        if (!q) {
            state.SkipWithError("swap left the loaded tick range");
            break;
        }
        crossed = q->ticks_crossed;
        benchmark::DoNotOptimize(q);
    }
    state.counters["ticks_crossed"] = crossed;
}
BENCHMARK(BM_QuoteCrossingTicks)->Unit(benchmark::kMicrosecond);

// The hot-path decision: optimal size + exact profit of the best cycle after a pool update.
void BM_OnPoolChanged(benchmark::State& state) {
    Fixture f;
    hft::strategy::TwoPoolArb arb(f.market, kToken0, {.min_profit_weth = 0});
    for (auto _ : state) {
        benchmark::DoNotOptimize(arb.on_pool_changed(*f.rich, 1));
    }
}
BENCHMARK(BM_OnPoolChanged)->Unit(benchmark::kMicrosecond);

}  // namespace
