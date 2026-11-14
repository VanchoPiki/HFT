#include "dex/v3_events.hpp"

#include "eth/abi.hpp"

namespace hft::dex {
namespace {

int tick_from_topic(const eth::Hash& topic) {
    return static_cast<int>(eth::to_signed(eth::u256_from_be(topic)));
}

}

V3EventDecoder::V3EventDecoder(const UniswapV3Abi& abi)
    : swap_(abi.swap_topic), mint_(abi.mint_topic), burn_(abi.burn_topic) {}

PoolEvent V3EventDecoder::decode(const net::Log& log) const {
    if (log.topics.empty()) {
        return std::monostate{};
    }
    const eth::Hash& topic0 = log.topics[0];
    eth::abi::Reader data(log.data);

    if (topic0 == swap_) {
        SwapEvent e;
        e.amount0 = data.read_int();
        e.amount1 = data.read_int();
        e.sqrt_price_x96 = data.read_uint();
        e.liquidity = data.read_uint();
        e.tick = static_cast<int>(data.read_int());
        return e;
    }
    if (topic0 == mint_ || topic0 == burn_) {
        if (log.topics.size() < 4) {
            throw eth::abi::Error("v3: liquidity event without tick topics");
        }
        const bool mint = topic0 == mint_;
        if (mint) {
            data.skip();
        }
        const eth::i256 amount = static_cast<eth::i256>(data.read_uint());
        LiquidityEvent e;
        e.tick_lower = tick_from_topic(log.topics[2]);
        e.tick_upper = tick_from_topic(log.topics[3]);
        e.liquidity_delta = mint ? amount : eth::i256(-amount);
        return e;
    }
    return std::monostate{};
}

}
