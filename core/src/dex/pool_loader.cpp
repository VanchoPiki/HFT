#include "dex/pool_loader.hpp"

#include <stdexcept>

#include "eth/abi.hpp"
#include "util/log.hpp"

namespace hft::dex {
namespace {

using eth::abi::Encoder;
using eth::abi::Reader;

std::vector<std::uint8_t> call_data(const eth::abi::Contract& contract, std::string_view fn) {
    return Encoder(contract.function(fn).selector).finish();
}

const net::CallResult& checked(const net::CallResult& r, const eth::Address& target,
                               std::string_view what) {
    if (!r.ok) {
        throw std::runtime_error("loader: " + std::string(what) + " failed for " +
                                 target.to_hex() + ": " + r.error);
    }
    return r;
}

// Tick-bitmap word of a tick, as in TickBitmap.position (compressed tick >> 8, rounded down).
int word_of(int tick, int spacing) {
    int compressed = tick / spacing;
    if (tick < 0 && tick % spacing != 0) {
        --compressed;
    }
    return compressed >> 8;
}

}  // namespace

std::vector<net::CallResult> PoolLoader::run(std::span<const net::CallRequest> calls,
                                             std::uint64_t block) {
    return rpc_.call_batch(calls, block == 0 ? "latest" : net::block_tag(block));
}

std::vector<eth::Address> PoolLoader::discover(
    const eth::Address& factory, std::span<const std::pair<eth::Address, eth::Address>> pairs,
    std::span<const std::uint32_t> fee_tiers) {
    const auto selector = abi_.factory.function("getPool").selector;
    std::vector<net::CallRequest> calls;
    for (const auto& [a, b] : pairs) {
        for (std::uint32_t fee : fee_tiers) {
            calls.push_back({factory, Encoder(selector).add_address(a).add_address(b).add_uint(fee).finish()});
        }
    }
    std::vector<eth::Address> pools;
    for (const auto& r : run(calls, 0)) {
        const eth::Address pool = Reader(checked(r, factory, "getPool").data).read_address();
        if (!pool.is_zero()) {
            pools.push_back(pool);
        }
    }
    return pools;
}

std::vector<PoolInfo> PoolLoader::load_info(std::span<const eth::Address> pools) {
    static constexpr std::string_view kFns[] = {"token0", "token1", "fee", "tickSpacing"};
    std::vector<net::CallRequest> calls;
    for (const auto& pool : pools) {
        for (auto fn : kFns) {
            calls.push_back({pool, call_data(abi_.pool, fn)});
        }
    }
    const auto results = run(calls, 0);
    std::vector<PoolInfo> infos;
    for (std::size_t i = 0; i < pools.size(); ++i) {
        const auto* r = &results[4 * i];
        PoolInfo info;
        info.address = pools[i];
        info.token0 = Reader(checked(r[0], pools[i], "token0").data).read_address();
        info.token1 = Reader(checked(r[1], pools[i], "token1").data).read_address();
        info.fee = static_cast<std::uint32_t>(Reader(checked(r[2], pools[i], "fee").data).read_u64());
        info.tick_spacing =
            static_cast<int>(Reader(checked(r[3], pools[i], "tickSpacing").data).read_int());
        infos.push_back(info);
    }
    return infos;
}

void PoolLoader::load_decimals(std::span<Token> tokens) {
    std::vector<net::CallRequest> calls;
    for (const Token& t : tokens) {
        calls.push_back({t.address, call_data(abi_.erc20, "decimals")});
    }
    const auto results = run(calls, 0);
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        tokens[i].decimals = static_cast<int>(
            Reader(checked(results[i], tokens[i].address, "decimals").data).read_u64());
    }
}

void PoolLoader::load_state(std::span<V3Pool* const> pools, std::uint64_t block) {
    std::vector<net::CallRequest> calls;
    for (const V3Pool* pool : pools) {
        calls.push_back({pool->info().address, call_data(abi_.pool, "slot0")});
        calls.push_back({pool->info().address, call_data(abi_.pool, "liquidity")});
    }
    const auto results = run(calls, block);
    for (std::size_t i = 0; i < pools.size(); ++i) {
        const auto& address = pools[i]->info().address;
        Reader slot0(checked(results[2 * i], address, "slot0").data);
        const eth::u256 sqrt_price = slot0.read_uint();
        const int tick = static_cast<int>(slot0.read_int());
        const eth::u256 liquidity =
            Reader(checked(results[2 * i + 1], address, "liquidity").data).read_uint();
        pools[i]->reset(sqrt_price, tick, liquidity, block);
    }
}

void PoolLoader::load_ticks(const eth::Address& tick_lens, std::span<V3Pool* const> pools,
                            int words_each_side, std::uint64_t block) {
    const auto selector = abi_.tick_lens.function("getPopulatedTicksInWord").selector;
    struct Request {
        V3Pool* pool;
        int word;
    };
    std::vector<Request> requests;
    std::vector<net::CallRequest> calls;
    for (V3Pool* pool : pools) {
        const int spacing = pool->info().tick_spacing;
        const int center = word_of(pool->tick(), spacing);
        for (int w = center - words_each_side; w <= center + words_each_side; ++w) {
            requests.push_back({pool, w});
            calls.push_back(
                {tick_lens, Encoder(selector).add_address(pool->info().address).add_int(w).finish()});
        }
        // Each word covers 256 * spacing ticks.
        pool->set_known_tick_range((center - words_each_side) * 256 * spacing,
                                   (center + words_each_side + 1) * 256 * spacing - 1);
    }
    const auto results = run(calls, block);
    std::size_t total = 0;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        Reader reader(checked(results[i], requests[i].pool->info().address, "getPopulatedTicksInWord").data);
        Reader array = reader.read_dynamic();
        const std::uint64_t n = array.read_u64();
        for (std::uint64_t k = 0; k < n; ++k) {
            const int tick = static_cast<int>(array.read_int());
            const eth::i256 net = array.read_int();
            const eth::u256 gross = array.read_uint();
            requests[i].pool->set_tick(tick, net, gross);
        }
        total += n;
    }
    log::debug("loader: {} initialized ticks in {} words", total, requests.size());
}

}  // namespace hft::dex
