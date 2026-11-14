#include "dex/market.hpp"

#include <cmath>
#include <format>
#include <stdexcept>

namespace hft::dex {

void Market::add_token(Token token) { tokens_.push_back(std::move(token)); }

V3Pool& Market::add_pool(PoolInfo info) {
    V3Pool& pool = pools_.emplace_back(info);
    by_address_[info.address] = &pool;
    for (std::size_t g = 0; g < groups_.size(); ++g) {
        const PoolInfo& other = groups_[g].front()->info();
        if (other.token0 == info.token0 && other.token1 == info.token1) {
            groups_[g].push_back(&pool);
            group_index_[&pool] = g;
            return pool;
        }
    }
    group_index_[&pool] = groups_.size();
    groups_.push_back({&pool});
    return pool;
}

V3Pool* Market::find_pool(const eth::Address& address) {
    const auto it = by_address_.find(address);
    return it == by_address_.end() ? nullptr : it->second;
}

const Token& Market::token(const eth::Address& address) const {
    for (const Token& t : tokens_) {
        if (t.address == address) {
            return t;
        }
    }
    throw std::out_of_range("market: unknown token " + address.to_hex());
}

std::vector<V3Pool*> Market::pool_ptrs() {
    std::vector<V3Pool*> out;
    for (V3Pool& p : pools_) {
        out.push_back(&p);
    }
    return out;
}

const std::vector<V3Pool*>& Market::group_of(const V3Pool& pool) const {
    return groups_.at(group_index_.at(&pool));
}

double Market::human_price(const V3Pool& pool) const {
    const int d0 = token(pool.info().token0).decimals;
    const int d1 = token(pool.info().token1).decimals;
    return pool.raw_price() * std::pow(10.0, d0 - d1);
}

double Market::human_amount(const eth::Address& t, double raw) const {
    return raw / std::pow(10.0, token(t).decimals);
}

std::string Market::pair_name(const V3Pool& pool) const {
    return token(pool.info().token0).symbol + "/" + token(pool.info().token1).symbol;
}

std::string fee_label(std::uint32_t fee) {
    return std::format("{:g}%", static_cast<double>(fee) / 10000.0);
}

}
