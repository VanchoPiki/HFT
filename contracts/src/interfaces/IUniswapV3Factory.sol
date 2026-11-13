// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

interface IUniswapV3Factory {
    event PoolCreated(
        address indexed token0, address indexed token1, uint24 indexed fee, int24 tickSpacing, address pool
    );

    function getPool(address tokenA, address tokenB, uint24 fee) external view returns (address pool);
}
