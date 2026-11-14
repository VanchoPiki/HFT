#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "dex/v3_pool.hpp"

namespace hft::dex {

struct Token {
    eth::Address address;
    std::string symbol;
    int decimals = 18;
};

class Market {
public:
    void add_token(Token token);
    V3Pool& add_pool(PoolInfo info);

    V3Pool* find_pool(const eth::Address& address);
    const Token& token(const eth::Address& address) const;

    std::deque<V3Pool>& pools() { return pools_; }
    const std::deque<V3Pool>& pools() const { return pools_; }
    const std::vector<Token>& tokens() const { return tokens_; }
    std::vector<V3Pool*> pool_ptrs();

    const std::vector<std::vector<V3Pool*>>& pair_groups() const { return groups_; }
    const std::vector<V3Pool*>& group_of(const V3Pool& pool) const;

    double human_price(const V3Pool& pool) const;
    double human_amount(const eth::Address& token, double raw) const;
    std::string pair_name(const V3Pool& pool) const;

private:
    std::vector<Token> tokens_;
    std::deque<V3Pool> pools_;
    std::unordered_map<eth::Address, V3Pool*, eth::AddressHash> by_address_;
    std::vector<std::vector<V3Pool*>> groups_;
    std::unordered_map<const V3Pool*, std::size_t> group_index_;
};

std::string fee_label(std::uint32_t fee);

}
