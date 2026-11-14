// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

import {IUniswapV3Pool} from "./interfaces/IUniswapV3Pool.sol";

contract ArbExecutor {
    uint160 internal constant MIN_SQRT_RATIO_PLUS_ONE = 4295128740;
    uint160 internal constant MAX_SQRT_RATIO_MINUS_ONE = 1461446703485210103287273052203988822378723970341;

    address public immutable owner;

    address private expectedCaller;

    event Arbitrage(
        address indexed poolA, address indexed poolB, address indexed tokenIn, uint256 amountIn, uint256 profit
    );

    error NotOwner();
    error AmountTooLarge();
    error UnexpectedCallback();
    error InsufficientProfit(int256 balanceChange, uint256 minProfit);
    error TransferFailed();

    constructor() {
        owner = msg.sender;
    }

    modifier onlyOwner() {
        if (msg.sender != owner) revert NotOwner();
        _;
    }

    function execute(
        address poolA,
        address poolB,
        address tokenIn,
        address tokenOut,
        uint256 amountIn,
        uint256 minProfit
    ) external onlyOwner returns (uint256 profit) {
        if (amountIn > uint256(type(int256).max)) revert AmountTooLarge();
        uint256 balanceBefore = _balanceOf(tokenIn);
        bool zeroForOne = tokenIn < tokenOut;

        expectedCaller = poolA;
        IUniswapV3Pool(poolA).swap(
            address(this),
            zeroForOne,
            int256(amountIn),
            zeroForOne ? MIN_SQRT_RATIO_PLUS_ONE : MAX_SQRT_RATIO_MINUS_ONE,
            abi.encode(poolB, tokenIn, tokenOut)
        );
        expectedCaller = address(0);

        uint256 balanceAfter = _balanceOf(tokenIn);
        if (balanceAfter < balanceBefore + minProfit) {
            revert InsufficientProfit(int256(balanceAfter) - int256(balanceBefore), minProfit);
        }
        profit = balanceAfter - balanceBefore;
        emit Arbitrage(poolA, poolB, tokenIn, amountIn, profit);
    }

    function uniswapV3SwapCallback(int256 amount0Delta, int256 amount1Delta, bytes calldata data) external {
        if (msg.sender != expectedCaller) revert UnexpectedCallback();
        (address poolB, address payToken, address otherToken) = abi.decode(data, (address, address, address));
        uint256 owed = uint256(amount0Delta > 0 ? amount0Delta : amount1Delta);

        if (poolB != address(0)) {
            uint256 received = uint256(-(amount0Delta > 0 ? amount1Delta : amount0Delta));
            bool zeroForOne = otherToken < payToken;
            expectedCaller = poolB;
            IUniswapV3Pool(poolB).swap(
                address(this),
                zeroForOne,
                int256(received),
                zeroForOne ? MIN_SQRT_RATIO_PLUS_ONE : MAX_SQRT_RATIO_MINUS_ONE,
                abi.encode(address(0), otherToken, payToken)
            );
        }
        _transfer(payToken, msg.sender, owed);
    }

    function withdraw(address token, address to, uint256 amount) external onlyOwner {
        _transfer(token, to, amount);
    }

    function _balanceOf(address token) private view returns (uint256) {
        (bool ok, bytes memory ret) = token.staticcall(abi.encodeWithSignature("balanceOf(address)", address(this)));
        if (!ok || ret.length < 32) revert TransferFailed();
        return abi.decode(ret, (uint256));
    }

    function _transfer(address token, address to, uint256 amount) private {
        (bool ok, bytes memory ret) = token.call(abi.encodeWithSignature("transfer(address,uint256)", to, amount));
        if (!ok || (ret.length != 0 && !abi.decode(ret, (bool)))) revert TransferFailed();
    }
}
