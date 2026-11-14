package com.hft.analytics.backtest;

import java.math.BigInteger;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class Backtester {

	public record Pool(String address, String pair, String symbol0, String symbol1, int decimals0, int decimals1,
			int fee, int tickSpacing) {
	}

	public record Swap(String pool, long block, int logIndex, BigInteger sqrtPriceX96, BigInteger liquidity,
			int tick) {
	}

	public record Params(double minProfitWeth, long gasUnits, double gasPriceGwei, String weth) {

		double gasWeth() {
			return gasUnits * gasPriceGwei * 1e-9;
		}

	}

	public record Found(long block, String pair, String poolA, int feeA, String poolB, int feeB, String tokenIn,
			double amountIn, double profitWeth) {
	}

	public record Result(int events, List<Found> opportunities, int profitableBlocks, double totalProfitWeth,
			double bestProfitWeth) {
	}

	record Cycle(double amountIn, double profit) {
	}

	private record State(double sqrtPrice, double liquidity, double sqrtLower, double sqrtUpper) {

		static State of(Swap s, int tickSpacing) {
			int lower = Math.floorDiv(s.tick(), tickSpacing) * tickSpacing;
			return new State(s.sqrtPriceX96().doubleValue() / 0x1p96, s.liquidity().doubleValue(),
					Math.pow(1.0001, lower / 2.0), Math.pow(1.0001, (lower + tickSpacing) / 2.0));
		}

		double reserve0() {
			return liquidity / sqrtPrice;
		}

		double reserve1() {
			return liquidity * sqrtPrice;
		}

		double maxIn(boolean zeroForOne, double gamma) {
			return zeroForOne ? liquidity * (1 / sqrtLower - 1 / sqrtPrice) / gamma
					: liquidity * (sqrtUpper - sqrtPrice) / gamma;
		}

	}

	private Backtester() {
	}

	public static Result run(List<Pool> pools, List<Swap> swaps, Params params) {
		Map<String, Pool> byAddress = new HashMap<>();
		Map<String, List<Pool>> byPair = new HashMap<>();
		for (Pool p : pools) {
			byAddress.put(p.address(), p);
			byPair.computeIfAbsent(p.pair(), k -> new ArrayList<>()).add(p);
		}
		Map<String, State> state = new HashMap<>();
		Map<String, Found> best = new LinkedHashMap<>();

		int i = 0;
		while (i < swaps.size()) {
			long block = swaps.get(i).block();
			java.util.Set<String> touchedPairs = new java.util.LinkedHashSet<>();
			for (; i < swaps.size() && swaps.get(i).block() == block; i++) {
				Swap s = swaps.get(i);
				Pool changed = byAddress.get(s.pool());
				if (changed != null && s.liquidity().signum() != 0) {
					state.put(s.pool(), State.of(s, changed.tickSpacing()));
					touchedPairs.add(changed.pair());
				}
			}
			for (String pair : touchedPairs) {
				List<Pool> group = byPair.get(pair);
				for (Pool a : group) {
					for (Pool b : group) {
						State sa = state.get(a.address());
						State sb = state.get(b.address());
						if (a != b && sa != null && sb != null) {
							evaluate(a, sa, b, sb, block, params, best);
						}
					}
				}
			}
		}

		List<Found> found = new ArrayList<>(best.values());
		double total = found.stream().mapToDouble(Found::profitWeth).sum();
		double max = found.stream().mapToDouble(Found::profitWeth).max().orElse(0);
		long blocks = found.stream().mapToLong(Found::block).distinct().count();
		return new Result(swaps.size(), found, (int) blocks, total, max);
	}

	private static void evaluate(Pool a, State sa, Pool b, State sb, long block, Params params,
			Map<String, Found> best) {
		for (boolean startWithToken0 : new boolean[] { true, false }) {
			Cycle c = optimalCycle(sa, a.fee(), sb, b.fee(), startWithToken0);
			if (c == null) {
				continue;
			}
			String tokenIn = startWithToken0 ? a.symbol0() : a.symbol1();
			int decimals = startWithToken0 ? a.decimals0() : a.decimals1();
			double profitWeth = toWeth(a, sa, tokenIn, c.profit(), params.weth());
			if (Double.isNaN(profitWeth)) {
				continue;
			}
			double net = profitWeth - params.gasWeth();
			if (net < params.minProfitWeth()) {
				continue;
			}
			String key = block + "|" + a.pair();
			Found f = new Found(block, a.pair(), a.address(), a.fee(), b.address(), b.fee(), tokenIn,
					c.amountIn() / Math.pow(10, decimals), net);
			best.merge(key, f, (old, neu) -> neu.profitWeth() > old.profitWeth() ? neu : old);
		}
	}

	static Cycle optimalCycle(State sa, int feeA, State sb, int feeB, boolean startWithToken0) {
		double inA = startWithToken0 ? sa.reserve0() : sa.reserve1();
		double outA = startWithToken0 ? sa.reserve1() : sa.reserve0();
		double inB = startWithToken0 ? sb.reserve1() : sb.reserve0();
		double outB = startWithToken0 ? sb.reserve0() : sb.reserve1();
		double ga = 1 - feeA / 1e6;
		double gb = 1 - feeB / 1e6;
		double a = ga * gb * outA * outB;
		double b = inA * inB;
		double c = ga * (inB + gb * outA);
		if (a <= b || c <= 0) {
			return null;
		}
		double dx = (Math.sqrt(a) * Math.sqrt(b) - b) / c;

		double capA = sa.maxIn(startWithToken0, ga);
		double maxInB = sb.maxIn(!startWithToken0, gb);
		double capB = maxInB >= ga * outA ? Double.POSITIVE_INFINITY : maxInB * inA / (ga * (outA - maxInB));
		dx = Math.min(dx, Math.min(capA, capB));
		if (!(dx > 0)) {
			return null;
		}
		double profit = a * dx / (b + c * dx) - dx;
		return profit > 0 ? new Cycle(dx, profit) : null;
	}

	static Cycle optimalCycle(double sqrtPriceA, double liquidityA, int feeA, double sqrtPriceB, double liquidityB,
			int feeB, boolean startWithToken0) {
		return optimalCycle(new State(sqrtPriceA, liquidityA, 0, Double.POSITIVE_INFINITY), feeA,
				new State(sqrtPriceB, liquidityB, 0, Double.POSITIVE_INFINITY), feeB, startWithToken0);
	}

	private static double toWeth(Pool p, State s, String token, double raw, String weth) {
		double rawPrice = s.sqrtPrice() * s.sqrtPrice();
		if (token.equals(weth)) {
			return raw / 1e18;
		}
		if (p.symbol0().equals(token) && p.symbol1().equals(weth)) {
			return raw * rawPrice / 1e18;
		}
		if (p.symbol1().equals(token) && p.symbol0().equals(weth)) {
			return raw / rawPrice / 1e18;
		}
		return Double.NaN;
	}

}
