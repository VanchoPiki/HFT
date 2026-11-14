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
    std::string rpc_ws;
    std::chrono::milliseconds poll_interval{1000};
    std::chrono::milliseconds rpc_timeout{10'000};
    std::filesystem::path abi_dir;

    eth::Address factory;
    eth::Address quoter;
    eth::Address tick_lens;
    std::vector<std::uint32_t> fee_tiers;
    int tick_words_each_side = 2;

    std::vector<TokenConfig> tokens;
    std::vector<std::pair<std::string, std::string>> pairs;

    strategy::StrategyConfig strategy;

    std::string telemetry_endpoint;

    exec::ExecutionConfig execution;

    static Config load(const std::filesystem::path& path);

    const eth::Address& token_address(const std::string& symbol) const;
};

void load_dotenv();

}
