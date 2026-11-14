#include "dex/uniswap_abi.hpp"

namespace hft::dex {

UniswapV3Abi UniswapV3Abi::load(const std::filesystem::path& dir) {
    UniswapV3Abi abi;
    abi.pool = eth::abi::Contract::load_file(dir / "IUniswapV3Pool.json");
    abi.factory = eth::abi::Contract::load_file(dir / "IUniswapV3Factory.json");
    abi.quoter = eth::abi::Contract::load_file(dir / "IQuoterV2.json");
    abi.tick_lens = eth::abi::Contract::load_file(dir / "ITickLens.json");
    abi.erc20 = eth::abi::Contract::load_file(dir / "IERC20.json");
    abi.swap_topic = abi.pool.event("Swap").topic;
    abi.mint_topic = abi.pool.event("Mint").topic;
    abi.burn_topic = abi.pool.event("Burn").topic;
    return abi;
}

}
