// Stage 4 acceptance check: local Uniswap V3 quotes must equal QuoterV2 (eth_call at the
// same block) to the wei. Exit code 0 only if every compared quote matches.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <random>
#include <string>

#include "app/config.hpp"
#include "app/setup.hpp"
#include "eth/abi.hpp"
#include "util/log.hpp"

using namespace hft;

namespace {

struct Sample {
    const dex::V3Pool* pool;
    bool zero_for_one;
    eth::u256 amount_in;
    dex::Quote local;
    bool matched = false;
};

// Writes the state of the `max_pools` pools whose verified quotes crossed the most ticks,
// together with the chain's answers, as an offline regression fixture for unit tests.
void dump_fixture(const std::string& path, std::uint64_t block, const std::vector<Sample>& samples,
                  std::size_t max_pools) {
    std::map<const dex::V3Pool*, int> crossed;
    for (const Sample& s : samples) {
        if (s.matched) {
            crossed[s.pool] += s.local.ticks_crossed + 1;
        }
    }
    std::vector<std::pair<int, const dex::V3Pool*>> ranked;
    for (const auto& [pool, score] : crossed) {
        ranked.emplace_back(score, pool);
    }
    std::sort(ranked.rbegin(), ranked.rend());
    ranked.resize(std::min(ranked.size(), max_pools));

    std::ofstream out(path);
    out << "{\"block\":" << block << ",\"pools\":[\n";
    for (std::size_t p = 0; p < ranked.size(); ++p) {
        const dex::V3Pool& pool = *ranked[p].second;
        out << (p ? ",\n" : "") << "{\"address\":\"" << pool.info().address.to_hex()
            << "\",\"fee\":" << pool.info().fee << ",\"tick_spacing\":" << pool.info().tick_spacing
            << ",\"sqrt_price_x96\":\"" << pool.sqrt_price_x96().str() << "\",\"tick\":" << pool.tick()
            << ",\"liquidity\":\"" << pool.liquidity().str()
            << "\",\"known_min_tick\":" << pool.known_min_tick()
            << ",\"known_max_tick\":" << pool.known_max_tick() << ",\n \"ticks\":[";
        bool first = true;
        for (const auto& [tick, info] : pool.ticks()) {
            out << (first ? "" : ",") << "[" << tick << ",\"" << info.liquidity_net.str() << "\",\""
                << info.liquidity_gross.str() << "\"]";
            first = false;
        }
        out << "],\n \"quotes\":[";
        first = true;
        for (const Sample& s : samples) {
            if (s.pool == &pool && s.matched) {
                out << (first ? "" : ",") << "{\"zero_for_one\":" << (s.zero_for_one ? "true" : "false")
                    << ",\"amount_in\":\"" << s.amount_in.str() << "\",\"amount_out\":\""
                    << s.local.amount_out.str() << "\",\"sqrt_price_after\":\""
                    << s.local.sqrt_price_after.str() << "\"}";
                first = false;
            }
        }
        out << "]}";
    }
    out << "\n]}\n";
    std::printf("wrote %zu pools to %s\n", ranked.size(), path.c_str());
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path = "config/arbitrum.json";
    std::string dump_path;
    int per_direction = 4;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (std::strcmp(argv[i], "--samples") == 0 && i + 1 < argc) {
            per_direction = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            dump_path = argv[++i];
        } else {
            std::puts(
                "usage: hft_verify [--config FILE] [--samples N_PER_POOL_AND_DIRECTION] "
                "[--dump FIXTURE.json]");
            return 2;
        }
    }

    try {
        app::load_dotenv();
        app::Config cfg = app::Config::load(config_path);
        cfg.tick_words_each_side = std::max(cfg.tick_words_each_side, 4);
        net::JsonRpcClient rpc(cfg.rpc_http);
        const auto abi = dex::UniswapV3Abi::load(cfg.abi_dir);
        dex::Market market = app::discover_market(cfg, rpc, abi);
        const std::uint64_t block = rpc.block_number() - 1;
        app::sync_market(cfg, rpc, abi, market, block);
        std::printf("block %llu, %zu pools\n", static_cast<unsigned long long>(block),
                    market.pools().size());

        // Amounts are log-uniform between 1e-7 and 0.3 of the pool's virtual reserve of the
        // input token, so both tiny swaps and swaps crossing many ticks get tested.
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> log_fraction(-7.0, std::log10(0.3));
        std::vector<Sample> samples;
        int out_of_range = 0;
        for (const dex::V3Pool& pool : market.pools()) {
            if (pool.liquidity() == 0) {
                continue;
            }
            const eth::u256 reserve0 = (pool.liquidity() << 96) / pool.sqrt_price_x96();
            const eth::u256 reserve1 = (pool.liquidity() * pool.sqrt_price_x96()) >> 96;
            for (bool zero_for_one : {true, false}) {
                for (int k = 0; k < per_direction; ++k) {
                    const auto parts_per_billion =
                        static_cast<unsigned>(std::llround(std::pow(10.0, 9 + log_fraction(rng))));
                    const eth::u256 amount =
                        (zero_for_one ? reserve0 : reserve1) * parts_per_billion / 1'000'000'000;
                    if (amount == 0) {
                        continue;
                    }
                    const auto local = pool.quote_exact_input(zero_for_one, amount);
                    if (!local) {
                        ++out_of_range;
                        continue;
                    }
                    samples.push_back({&pool, zero_for_one, amount, *local});
                }
            }
        }

        const auto selector = abi.quoter.function("quoteExactInputSingle").selector;
        std::vector<net::CallRequest> calls;
        for (const Sample& s : samples) {
            const auto& info = s.pool->info();
            calls.push_back({cfg.quoter, eth::abi::Encoder(selector)
                                             .add_address(s.zero_for_one ? info.token0 : info.token1)
                                             .add_address(s.zero_for_one ? info.token1 : info.token0)
                                             .add_uint(s.amount_in)
                                             .add_uint(info.fee)
                                             .add_uint(0)
                                             .finish()});
        }
        const auto results = rpc.call_batch(calls, net::block_tag(block));

        int matched = 0;
        int mismatched = 0;
        int quoter_failed = 0;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            Sample& s = samples[i];
            if (!results[i].ok) {
                ++quoter_failed;
                hft::log::warn("quoter failed for {}: {}", s.pool->info().address.to_hex(),
                               results[i].error);
                continue;
            }
            eth::abi::Reader r(results[i].data);
            const eth::u256 amount_out = r.read_uint();
            const eth::u256 sqrt_after = r.read_uint();
            if (amount_out == s.local.amount_out && sqrt_after == s.local.sqrt_price_after) {
                ++matched;
                s.matched = true;
            } else {
                ++mismatched;
                std::printf("MISMATCH %s %s fee=%u in=%s: chain out=%s local out=%s\n",
                            market.pair_name(*s.pool).c_str(), s.zero_for_one ? "0->1" : "1->0",
                            s.pool->info().fee, s.amount_in.str().c_str(),
                            amount_out.str().c_str(), s.local.amount_out.str().c_str());
            }
        }
        std::printf("compared %zu quotes: %d match to the wei, %d mismatch, %d quoter errors, "
                    "%d skipped (beyond loaded ticks)\n",
                    samples.size(), matched, mismatched, quoter_failed, out_of_range);
        if (!dump_path.empty()) {
            dump_fixture(dump_path, block, samples, 3);
        }
        return mismatched == 0 && quoter_failed == 0 && matched > 0 ? 0 : 1;
    } catch (const std::exception& e) {
        hft::log::error("fatal: {}", e.what());
        return 1;
    }
}
