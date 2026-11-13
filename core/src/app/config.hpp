#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "eth/address.hpp"
#include "exec/executor.hpp"
#include "strategy/two_pool_arb.hpp"

namespace hft::app {

struct TokenConfig {
    std::string symbol;
    eth::Address address;
};

struct Config {
    std::uint64_t chain_id = 0;
    std::string rpc_http;
    std::string rpc_ws;  // empty: poll over HTTP
    std::chrono::milliseconds poll_interval{1000};
    // Per HTTP request. A local fork fetches state from upstream on first touch: be patient.
    std::chrono::milliseconds rpc_timeout{10'000};
    std::filesystem::path abi_dir;

    eth::Address factory;
    eth::Address quoter;
    eth::Address tick_lens;
    std::vector<std::uint32_t> fee_tiers;
    int tick_words_each_side = 2;

    std::vector<TokenConfig> tokens;
    std::vector<std::pair<std::string, std::string>> pairs;  // token symbols

    strategy::StrategyConfig strategy;

    std::string telemetry_endpoint;  // ZeroMQ PUB, e.g. tcp://127.0.0.1:5555; empty = off

    // Off by default: the bot only monitors. HFT_EXECUTOR overrides execution.contract.
    exec::ExecutionConfig execution;

    // JSON file; relative paths are resolved against its directory. The environment
    // variables named by "rpc_http_env" / "rpc_ws_env" (if present) override the endpoints.
    static Config load(const std::filesystem::path& path);

    const eth::Address& token_address(const std::string& symbol) const;
};

// Loads KEY=VALUE lines from the first .env found in the working directory or its
// parents, without overriding variables that are already set.
void load_dotenv();

}  // namespace hft::app
