#pragma once

#include <filesystem>

#include "eth/abi_json.hpp"

namespace hft::dex {

struct UniswapV3Abi {
    eth::abi::Contract pool;
    eth::abi::Contract factory;
    eth::abi::Contract quoter;
    eth::abi::Contract tick_lens;
    eth::abi::Contract erc20;

    eth::Hash swap_topic{};
    eth::Hash mint_topic{};
    eth::Hash burn_topic{};

    static UniswapV3Abi load(const std::filesystem::path& dir);
};

}
