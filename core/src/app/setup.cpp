#include "app/setup.hpp"

#include <format>
#include <stdexcept>

#include "dex/pool_loader.hpp"

namespace hft::app {

dex::Market discover_market(const Config& cfg, net::JsonRpcClient& rpc,
                            const dex::UniswapV3Abi& abi) {
    const std::uint64_t chain_id = rpc.chain_id();
    if (chain_id != cfg.chain_id) {
        throw std::runtime_error(
            std::format("setup: RPC chain id {} != configured {}", chain_id, cfg.chain_id));
    }
    dex::PoolLoader loader(rpc, abi);
    dex::Market market;

    std::vector<dex::Token> tokens;
    for (const auto& t : cfg.tokens) {
        tokens.push_back({t.address, t.symbol, 18});
    }
    loader.load_decimals(tokens);
    for (auto& t : tokens) {
        market.add_token(std::move(t));
    }

    std::vector<std::pair<eth::Address, eth::Address>> pairs;
    for (const auto& [a, b] : cfg.pairs) {
        pairs.emplace_back(cfg.token_address(a), cfg.token_address(b));
    }
    for (const auto& info : loader.load_info(loader.discover(cfg.factory, pairs, cfg.fee_tiers))) {
        market.add_pool(info);
    }
    return market;
}

void sync_market(const Config& cfg, net::JsonRpcClient& rpc, const dex::UniswapV3Abi& abi,
                 dex::Market& market, std::uint64_t block) {
    dex::PoolLoader loader(rpc, abi);
    const auto pools = market.pool_ptrs();
    loader.load_state(pools, block);
    loader.load_ticks(cfg.tick_lens, pools, cfg.tick_words_each_side, block);
}

}  // namespace hft::app
