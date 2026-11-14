// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

import {ArbExecutor} from "../src/ArbExecutor.sol";
import {IUniswapV3Pool} from "../src/interfaces/IUniswapV3Pool.sol";
import {IERC20, IWETH9} from "../src/interfaces/IERC20.sol";

interface Vm {
    function deal(address account, uint256 newBalance) external;
    function prank(address msgSender) external;
    function expectRevert(bytes4 selector) external;
    function expectRevert() external;
    function expectPartialRevert(bytes4 selector) external;
    function skip(bool skipTest) external;
}

contract Swapper {
    function swap(IUniswapV3Pool pool, bool zeroForOne, uint256 amountIn) external {
        pool.swap(
            address(this),
            zeroForOne,
            int256(amountIn),
            zeroForOne ? 4295128740 : 1461446703485210103287273052203988822378723970341,
            ""
        );
    }

    function uniswapV3SwapCallback(int256 amount0Delta, int256 amount1Delta, bytes calldata) external {
        IUniswapV3Pool pool = IUniswapV3Pool(msg.sender);
        if (amount0Delta > 0) IERC20(pool.token0()).transfer(msg.sender, uint256(amount0Delta));
        if (amount1Delta > 0) IERC20(pool.token1()).transfer(msg.sender, uint256(amount1Delta));
    }
}

contract ArbExecutorTest {
    Vm constant vm = Vm(address(uint160(uint256(keccak256("hevm cheat code")))));

    IWETH9 constant WETH = IWETH9(0x82aF49447D8a07e3bd95BD0d56f35241523fBab1);
    IERC20 constant USDC = IERC20(0xaf88d065e77c8cC2239327C5EDb3A432268e5831);
    IUniswapV3Pool constant POOL_005 = IUniswapV3Pool(0xC6962004f452bE9203591991D15f6b388e09E8D0);
    IUniswapV3Pool constant POOL_030 = IUniswapV3Pool(0xc473e2aEE3441BF9240Be85eb122aBB059A3B57c);

    ArbExecutor executor;
    Swapper whale;

    function setUp() public {
        vm.skip(block.chainid != 42161);
        executor = new ArbExecutor();
        whale = new Swapper();
        vm.deal(address(whale), 1_000 ether);
        vm.prank(address(whale));
        WETH.deposit{value: 1_000 ether}();
    }

    function test_unprofitableCycleReverts() public {
        vm.expectRevert();
        executor.execute(address(POOL_005), address(POOL_030), address(WETH), address(USDC), 1 ether, 1);
    }

    function test_arbitragesAPriceDislocation() public {
        whale.swap(POOL_030, true, 300 ether);
        uint256 profit =
            executor.execute(address(POOL_005), address(POOL_030), address(WETH), address(USDC), 20 ether, 1);
        require(profit > 0, "no profit");
        require(WETH.balanceOf(address(executor)) == profit, "profit not kept by the executor");
        executor.withdraw(address(WETH), address(this), profit);
        require(WETH.balanceOf(address(this)) == profit, "withdraw failed");
    }

    function test_minProfitIsEnforced() public {
        whale.swap(POOL_030, true, 300 ether);
        vm.expectPartialRevert(ArbExecutor.InsufficientProfit.selector);
        executor.execute(address(POOL_005), address(POOL_030), address(WETH), address(USDC), 20 ether, 1_000 ether);
    }

    function test_onlyOwnerCanExecute() public {
        vm.prank(address(0xBEEF));
        vm.expectRevert(ArbExecutor.NotOwner.selector);
        executor.execute(address(POOL_005), address(POOL_030), address(WETH), address(USDC), 1 ether, 0);
    }

    function test_callbackRejectsStrangers() public {
        vm.expectRevert(ArbExecutor.UnexpectedCallback.selector);
        executor.uniswapV3SwapCallback(1, -1, abi.encode(address(0), address(WETH), address(USDC)));
    }

    function test_rejectsAmountsThatWouldFlipToExactOutput() public {
        vm.expectRevert(ArbExecutor.AmountTooLarge.selector);
        executor.execute(address(POOL_005), address(POOL_030), address(WETH), address(USDC), 2 ** 255, 0);
    }

    function test_onlyOwnerCanWithdraw() public {
        vm.prank(address(0xBEEF));
        vm.expectRevert(ArbExecutor.NotOwner.selector);
        executor.withdraw(address(WETH), address(0xBEEF), 1);
    }
}
