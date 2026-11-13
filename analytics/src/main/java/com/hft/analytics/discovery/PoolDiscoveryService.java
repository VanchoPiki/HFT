package com.hft.analytics.discovery;

import java.io.IOException;
import java.math.BigInteger;
import java.time.Instant;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.web3j.abi.datatypes.Function;

import com.hft.analytics.chain.ChainClient;
import com.hft.analytics.chain.ChainClient.Call;
import com.hft.analytics.chain.ChainProperties;
import com.hft.analytics.chain.UniswapAbi;

/**
 * Finds every Uniswap V3 pool between the configured tokens: asks the factory for each
 * token pair and fee tier, then reads each pool's state. Not latency sensitive, so it lives
 * here rather than in the bot.
 */
@Service
public class PoolDiscoveryService {

	private static final Logger log = LoggerFactory.getLogger(PoolDiscoveryService.class);

	private static final String ZERO = "0x0000000000000000000000000000000000000000";

	private final ChainClient chain;

	private final ChainProperties properties;

	private final DiscoveredPoolRepository pools;

	public PoolDiscoveryService(ChainClient chain, ChainProperties properties, DiscoveredPoolRepository pools) {
		this.chain = chain;
		this.properties = properties;
		this.pools = pools;
	}

	private record Candidate(String address, String symbolA, String symbolB, int fee) {
	}

	@Transactional
	public List<DiscoveredPool> discover() throws IOException {
		List<String> symbols = new ArrayList<>(properties.tokens().keySet());
		Map<String, String> addressOf = new HashMap<>();
		symbols.forEach(s -> addressOf.put(s, properties.tokens().get(s).toLowerCase(Locale.ROOT)));

		// 1. factory.getPool for every unordered token pair and fee tier
		List<Call> calls = new ArrayList<>();
		List<Candidate> asked = new ArrayList<>();
		for (int i = 0; i < symbols.size(); i++) {
			for (int j = i + 1; j < symbols.size(); j++) {
				for (int fee : properties.feeTiers()) {
					String a = addressOf.get(symbols.get(i));
					String b = addressOf.get(symbols.get(j));
					calls.add(new Call(properties.factory(), UniswapAbi.encode(UniswapAbi.getPool(a, b, fee))));
					asked.add(new Candidate(null, symbols.get(i), symbols.get(j), fee));
				}
			}
		}
		List<String> results = chain.callAll(calls);
		List<Candidate> found = new ArrayList<>();
		for (int k = 0; k < results.size(); k++) {
			Function fn = UniswapAbi.getPool(ZERO, ZERO, 0);
			var out = UniswapAbi.decode(fn, results.get(k));
			String pool = out.isEmpty() ? ZERO : out.getFirst().toString();
			if (!pool.equals(ZERO)) {
				Candidate c = asked.get(k);
				found.add(new Candidate(pool.toLowerCase(Locale.ROOT), c.symbolA(), c.symbolB(), c.fee()));
			}
		}

		// 2. decimals of every token
		List<Call> decimalCalls = symbols.stream()
			.map(s -> new Call(addressOf.get(s), UniswapAbi.encode(UniswapAbi.decimals())))
			.toList();
		List<String> decimalResults = chain.callAll(decimalCalls);
		Map<String, Integer> decimals = new HashMap<>();
		for (int k = 0; k < symbols.size(); k++) {
			var out = UniswapAbi.decode(UniswapAbi.decimals(), decimalResults.get(k));
			decimals.put(symbols.get(k), out.isEmpty() ? 18 : UniswapAbi.uint(out, 0).intValue());
		}

		// 3. tickSpacing, liquidity and slot0 of every pool
		List<Call> stateCalls = new ArrayList<>();
		for (Candidate c : found) {
			stateCalls.add(new Call(c.address(), UniswapAbi.encode(UniswapAbi.tickSpacing())));
			stateCalls.add(new Call(c.address(), UniswapAbi.encode(UniswapAbi.liquidity())));
			stateCalls.add(new Call(c.address(), UniswapAbi.encode(UniswapAbi.slot0())));
		}
		List<String> state = chain.callAll(stateCalls);

		Instant now = Instant.now();
		List<DiscoveredPool> discovered = new ArrayList<>();
		for (int k = 0; k < found.size(); k++) {
			Candidate c = found.get(k);
			var spacing = UniswapAbi.decode(UniswapAbi.tickSpacing(), state.get(3 * k));
			var liquidity = UniswapAbi.decode(UniswapAbi.liquidity(), state.get(3 * k + 1));
			var slot0 = UniswapAbi.decode(UniswapAbi.slot0(), state.get(3 * k + 2));
			if (spacing.isEmpty() || liquidity.isEmpty() || slot0.isEmpty()) {
				continue;
			}
			// Uniswap orders a pool's tokens by address.
			boolean aIsToken0 = addressOf.get(c.symbolA()).compareTo(addressOf.get(c.symbolB())) < 0;
			String s0 = aIsToken0 ? c.symbolA() : c.symbolB();
			String s1 = aIsToken0 ? c.symbolB() : c.symbolA();
			discovered.add(new DiscoveredPool(c.address(), addressOf.get(s0), addressOf.get(s1), s0, s1,
					decimals.get(s0), decimals.get(s1), c.fee(), UniswapAbi.uint(spacing, 0).intValue(),
					UniswapAbi.uint(liquidity, 0), UniswapAbi.uint(slot0, 0), now));
		}
		pools.deleteAllInBatch();
		pools.saveAll(discovered);
		log.info("discovery: {} pools among {} tokens ({} getPool calls)", discovered.size(), symbols.size(),
				calls.size());
		return discovered;
	}

}
