#include "app/config.hpp"

#include <simdjson.h>

#include <cstdlib>
#include <fstream>
#include <stdexcept>

#include "util/log.hpp"

namespace hft::app {
namespace {

eth::Address parse_address(std::string_view hex, std::string_view what) {
    const auto address = eth::Address::from_hex(hex);
    if (!address) {
        throw std::runtime_error("config: bad address for " + std::string(what));
    }
    return *address;
}

std::string env_or(const char* name, std::string fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0' ? std::string(value) : std::move(fallback);
}

// Optional field: `out` keeps its default when the key is missing or has another type.
template <typename T>
void read_optional(simdjson::dom::element object, const char* key, T& out) {
    T value{};
    if (object[key].get(value) == simdjson::SUCCESS) {
        out = value;
    }
}

std::string trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r");
    const auto last = s.find_last_not_of(" \t\r");
    return first == std::string::npos ? std::string() : s.substr(first, last - first + 1);
}

}  // namespace

Config Config::load(const std::filesystem::path& path) {
    simdjson::dom::parser parser;
    const simdjson::padded_string json = simdjson::padded_string::load(path.string());
    const simdjson::dom::element doc = parser.parse(json);
    const auto dir = path.parent_path();

    Config cfg;
    cfg.chain_id = doc["chain_id"];
    // Each config names the environment variables allowed to override its endpoints, so a
    // mainnet key in .env can never leak into e.g. the local fork config.
    cfg.rpc_http = std::string_view(doc["rpc_http"]);
    cfg.rpc_ws = std::string_view(doc["rpc_ws"]);
    std::string_view env_name;
    if (doc["rpc_http_env"].get(env_name) == simdjson::SUCCESS) {
        cfg.rpc_http = env_or(std::string(env_name).c_str(), cfg.rpc_http);
    }
    if (doc["rpc_ws_env"].get(env_name) == simdjson::SUCCESS) {
        cfg.rpc_ws = env_or(std::string(env_name).c_str(), cfg.rpc_ws);
    }
    cfg.poll_interval = std::chrono::milliseconds(std::int64_t(doc["poll_interval_ms"]));
    std::uint64_t timeout_ms = 0;
    if (doc["rpc_timeout_ms"].get(timeout_ms) == simdjson::SUCCESS) {
        cfg.rpc_timeout = std::chrono::milliseconds(timeout_ms);
    }
    cfg.abi_dir = dir / std::string_view(doc["abi_dir"]);

    const auto uni = doc["uniswap_v3"];
    cfg.factory = parse_address(uni["factory"], "uniswap_v3.factory");
    cfg.quoter = parse_address(uni["quoter_v2"], "uniswap_v3.quoter_v2");
    cfg.tick_lens = parse_address(uni["tick_lens"], "uniswap_v3.tick_lens");
    for (std::uint64_t fee : doc["fee_tiers"].get_array()) {
        cfg.fee_tiers.push_back(static_cast<std::uint32_t>(fee));
    }
    cfg.tick_words_each_side = static_cast<int>(std::int64_t(doc["tick_words_each_side"]));

    for (auto [symbol, address] : doc["tokens"].get_object()) {
        cfg.tokens.push_back({std::string(symbol), parse_address(address, symbol)});
    }
    for (simdjson::dom::array pair : doc["pairs"].get_array()) {
        cfg.pairs.emplace_back(std::string_view(pair.at(0)), std::string_view(pair.at(1)));
        (void)cfg.token_address(cfg.pairs.back().first);  // validate symbols early
        (void)cfg.token_address(cfg.pairs.back().second);
    }
    (void)cfg.token_address("WETH");  // profits and gas are valued in WETH

    // Optional sections: missing keys keep the defaults from the structs.
    simdjson::dom::element strat;
    if (doc["strategy"].get(strat) == simdjson::SUCCESS) {
        read_optional(strat, "min_profit_weth", cfg.strategy.min_profit_weth);
        read_optional(strat, "gas_units", cfg.strategy.gas_units);
        read_optional(strat, "max_input_weth", cfg.strategy.max_input_weth);
    }
    std::string_view endpoint;
    if (doc["telemetry"]["zmq_endpoint"].get(endpoint) == simdjson::SUCCESS) {
        cfg.telemetry_endpoint = endpoint;
    }
    if (doc["telemetry"]["zmq_endpoint_env"].get(env_name) == simdjson::SUCCESS) {
        cfg.telemetry_endpoint = env_or(std::string(env_name).c_str(), cfg.telemetry_endpoint);
    }

    simdjson::dom::element ex;
    if (doc["execution"].get(ex) == simdjson::SUCCESS) {
        auto& e = cfg.execution;
        read_optional(ex, "enabled", e.enabled);
        std::string_view contract;
        if (ex["contract"].get(contract) == simdjson::SUCCESS && !contract.empty()) {
            e.contract = parse_address(contract, "execution.contract");
        }
        std::string_view key_env;
        if (ex["private_key_env"].get(key_env) == simdjson::SUCCESS) {
            e.private_key_env = key_env;
        }
        read_optional(ex, "gas_limit", e.gas_limit);
        read_optional(ex, "max_fee_multiplier", e.max_fee_multiplier);
        read_optional(ex, "min_profit_share", e.min_profit_share);
        read_optional(ex, "simulate", e.simulate);
        std::uint64_t receipt_ms = 0;
        if (ex["receipt_timeout_ms"].get(receipt_ms) == simdjson::SUCCESS) {
            e.receipt_timeout = std::chrono::milliseconds(receipt_ms);
        }
    }
    if (const char* contract = std::getenv("HFT_EXECUTOR"); contract != nullptr && *contract != '\0') {
        cfg.execution.contract = parse_address(contract, "HFT_EXECUTOR");
    }
    return cfg;
}

const eth::Address& Config::token_address(const std::string& symbol) const {
    for (const auto& t : tokens) {
        if (t.symbol == symbol) {
            return t.address;
        }
    }
    throw std::runtime_error("config: unknown token symbol '" + symbol + "'");
}

void load_dotenv() {
    auto dir = std::filesystem::current_path();
    for (int depth = 0; depth < 4; ++depth, dir = dir.parent_path()) {
        const auto file = dir / ".env";
        std::ifstream in(file);
        if (!in) {
            continue;
        }
        std::string line;
        while (std::getline(in, line)) {
            line = trim(line);
            const auto eq = line.find('=');
            if (line.empty() || line[0] == '#' || eq == std::string::npos) {
                continue;
            }
            const std::string key = trim(line.substr(0, eq));
            const std::string value = trim(line.substr(eq + 1));
            if (!value.empty()) {
                setenv(key.c_str(), value.c_str(), /*overwrite=*/0);
            }
        }
        log::info("config: loaded {}", file.string());
        return;
    }
}

}  // namespace hft::app
