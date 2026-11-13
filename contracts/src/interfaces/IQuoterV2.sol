// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

/// Uniswap V3 QuoterV2. Not a view function on-chain (it reverts internally), so it is
/// only ever used through eth_call.
interface IQuoterV2 {
    struct QuoteExactInputSingleParams {
        address tokenIn;
        address tokenOut;
        uint256 amountIn;
        uint24 fee;
        uint160 sqrtPriceLimitX96;
    }

    function quoteExactInputSingle(QuoteExactInputSingleParams memory params)
        external
        returns (uint256 amountOut, uint160 sqrtPriceX96After, uint32 initializedTicksCrossed, uint256 gasEstimate);
}
