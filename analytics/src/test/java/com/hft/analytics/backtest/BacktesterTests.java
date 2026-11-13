package com.hft.analytics.backtest;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.List;

import org.junit.jupiter.api.Test;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.within;

class BacktesterTests {

	static final Backtester.Pool CHEAP = new Backtester.Pool("0xa", "WETH/USDC", "WETH", "USDC", 18, 6, 500, 10);

	static final Backtester.Pool RICH = new Backtester.Pool("0xb", "WETH/USDC", "WETH", "USDC", 18, 6, 3000, 60);

	static final Backtester.Params NO_GAS = new Backtester.Params(0, 0, 0, "WETH");

	/** sqrtPriceX96 for a human price (USDC per WETH) of a WETH(18)/USDC(6) pool. */
	static BigInteger sqrtPriceX96(double usdcPerWeth) {
		double raw = usdcPerWeth * 1e6 / 1e18;
		return new BigDecimal(Math.sqrt(raw) * Math.pow(2, 96)).toBigInteger();
	}

	static final BigInteger LIQUIDITY = new BigInteger("4000000000000000000");

	static int tick(double usdcPerWeth) {
		return (int) Math.floor(Math.log(usdcPerWeth * 1e6 / 1e18) / Math.log(1.0001));
	}

	static Backtester.Swap swap(String pool, long block, int index, double price) {
		return new Backtester.Swap(pool, block, index, sqrtPriceX96(price), LIQUIDITY, tick(price));
	}

	@Test
	void noOpportunityWhileThePoolsAgree() {
		var result = Backtester.run(List.of(CHEAP, RICH),
				List.of(swap("0xa", 1, 0, 2340), swap("0xb", 1, 1, 2341), swap("0xa", 2, 0, 2342)), NO_GAS);
		assertThat(result.events()).isEqualTo(3);
		assertThat(result.opportunities()).isEmpty();
	}

	@Test
	void findsADislocationAndValuesItInWeth() {
		// WETH is 2% more expensive in the 0.3% pool: sell WETH there, buy it back in the 0.05% pool.
		var result = Backtester.run(List.of(CHEAP, RICH), List.of(swap("0xa", 1, 0, 2300), swap("0xb", 2, 0, 2346)),
				NO_GAS);
		assertThat(result.opportunities()).hasSize(1);
		var found = result.opportunities().getFirst();
		assertThat(found.block()).isEqualTo(2);
		// Either route exploits it: start with WETH (sell it in the rich pool first) or with
		// USDC (buy WETH in the cheap pool first). The more profitable one is kept.
		if (found.tokenIn().equals("WETH")) {
			assertThat(found.poolA()).isEqualTo("0xb");
		}
		else {
			assertThat(found.tokenIn()).isEqualTo("USDC");
			assertThat(found.poolA()).isEqualTo("0xa");
		}
		assertThat(found.profitWeth()).isPositive();
		assertThat(result.profitableBlocks()).isEqualTo(1);
		assertThat(result.totalProfitWeth()).isEqualTo(found.profitWeth());
	}

	@Test
	void closedFormBeatsEveryOtherSize() {
		double sa = Math.sqrt(1.02);
		double l = 1e21;
		var best = Backtester.optimalCycle(sa, l, 3000, 1.0, l, 500, true);
		assertThat(best).isNotNull();
		// brute force over the same two constant-product swaps
		double bestGrid = 0;
		for (double dx = 1e15; dx < 1e22; dx *= 1.01) {
			double outA = (1 - 0.003) * (l * sa) * dx / (l / sa + (1 - 0.003) * dx);
			double outB = (1 - 0.0005) * l * outA / (l + (1 - 0.0005) * outA);
			bestGrid = Math.max(bestGrid, outB - dx);
		}
		assertThat(best.profit()).isGreaterThanOrEqualTo(bestGrid * (1 - 1e-9));
		assertThat(best.profit()).isCloseTo(bestGrid, within(bestGrid * 1e-3));
	}

	@Test
	void gasAndMinimumProfitFilterOpportunities() {
		var swaps = List.of(swap("0xa", 1, 0, 2300), swap("0xb", 2, 0, 2346));
		double gross = Backtester.run(List.of(CHEAP, RICH), swaps, NO_GAS).totalProfitWeth();
		var expensiveGas = new Backtester.Params(0, 1_000_000_000L, gross * 2e9 / 1_000_000_000L * 1e9 / 1e9, "WETH");
		assertThat(Backtester.run(List.of(CHEAP, RICH), swaps, expensiveGas).opportunities()).isEmpty();
		var picky = new Backtester.Params(gross * 2, 0, 0, "WETH");
		assertThat(Backtester.run(List.of(CHEAP, RICH), swaps, picky).opportunities()).isEmpty();
	}

	@Test
	void keepsOnlyTheBestOpportunityPerBlockAndPair() {
		var result = Backtester.run(List.of(CHEAP, RICH),
				List.of(swap("0xa", 1, 0, 2300), swap("0xb", 2, 0, 2340), swap("0xb", 2, 1, 2360)), NO_GAS);
		assertThat(result.opportunities()).hasSize(1);
	}

	/**
	 * Liquidity can change at any initialized tick, so a trade that leaves the current
	 * tick-spacing interval cannot be priced from L alone: such trades must be capped.
	 */
	@Test
	void capsTradesToTheCurrentTickInterval() {
		var swaps = List.of(swap("0xa", 1, 0, 2300), swap("0xb", 2, 0, 2346));
		var narrow = List.of(new Backtester.Pool("0xa", "WETH/USDC", "WETH", "USDC", 18, 6, 500, 1),
				new Backtester.Pool("0xb", "WETH/USDC", "WETH", "USDC", 18, 6, 3000, 1));
		var wide = List.of(new Backtester.Pool("0xa", "WETH/USDC", "WETH", "USDC", 18, 6, 500, 2000),
				new Backtester.Pool("0xb", "WETH/USDC", "WETH", "USDC", 18, 6, 3000, 2000));
		double narrowProfit = Backtester.run(narrow, swaps, NO_GAS).totalProfitWeth();
		double wideProfit = Backtester.run(wide, swaps, NO_GAS).totalProfitWeth();
		assertThat(narrowProfit).isPositive();
		// A 1-tick interval (0.01% of price) allows far less volume than a 2000-tick one.
		assertThat(narrowProfit).isLessThan(wideProfit / 10);
	}

	@Test
	void ignoresPoolsItDoesNotKnow() {
		var result = Backtester.run(List.of(CHEAP), List.of(swap("0xzzz", 1, 0, 2300)), NO_GAS);
		assertThat(result.opportunities()).isEmpty();
	}

}
