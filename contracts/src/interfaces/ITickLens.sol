// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

/// Uniswap V3 TickLens: all initialized ticks of one tick-bitmap word in a single call.
interface ITickLens {
    struct PopulatedTick {
        int24 tick;
        int128 liquidityNet;
        uint128 liquidityGross;
    }

    function getPopulatedTicksInWord(address pool, int16 tickBitmapIndex)
        external
        view
        returns (PopulatedTick[] memory populatedTicks);
}
