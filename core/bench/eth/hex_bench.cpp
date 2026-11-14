#include "eth/hex.hpp"

#include <benchmark/benchmark.h>

#include <array>
#include <vector>

namespace {

std::vector<std::uint8_t> make_bytes(std::size_t n) {
    std::vector<std::uint8_t> bytes(n);
    for (std::size_t i = 0; i < n; ++i) {
        bytes[i] = static_cast<std::uint8_t>(i * 31 + 7);
    }
    return bytes;
}

void BM_ToHex(benchmark::State& state) {
    const auto bytes = make_bytes(static_cast<std::size_t>(state.range(0)));
    for (auto _ : state) {
        benchmark::DoNotOptimize(hft::eth::to_hex(bytes));
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_ToHex)->Arg(32)->Arg(1024);

void BM_FromHexFixed32(benchmark::State& state) {
    const auto hex = hft::eth::to_hex(make_bytes(32));
    std::array<std::uint8_t, 32> out{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(hft::eth::from_hex(hex, out));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_FromHexFixed32);

void BM_FromHexVector(benchmark::State& state) {
    const auto hex = hft::eth::to_hex(make_bytes(static_cast<std::size_t>(state.range(0))));
    for (auto _ : state) {
        benchmark::DoNotOptimize(hft::eth::from_hex(hex));
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_FromHexVector)->Arg(32)->Arg(1024);

}
