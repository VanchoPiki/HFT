package com.hft.analytics.discovery;

import java.math.BigInteger;
import java.time.Instant;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import org.junit.jupiter.api.Test;
import tools.jackson.databind.json.JsonMapper;

import com.hft.analytics.chain.ChainProperties;

import static org.assertj.core.api.Assertions.assertThat;

class BotConfigExporterTests {

	static final Map<String, String> TOKENS = new LinkedHashMap<>(Map.of("WETH", "0x82af", "USDC", "0xaf88", "ARB",
			"0x912c", "DAI", "0xda10"));

	static final ChainProperties CHAIN = new ChainProperties("http://rpc", "http://rpc", 42161, "0xfactory", "0xlens",
			"0xquoter",
			List.of(100, 500, 3000, 10000), "WETH", TOKENS, 2000, 20);

	static DiscoveredPool pool(String address, String s0, String s1, int fee, long liquidity) {
		return new DiscoveredPool(address, TOKENS.get(s0), TOKENS.get(s1), s0, s1, 18, 6, fee, 10,
				BigInteger.valueOf(liquidity), BigInteger.ONE.shiftLeft(96), Instant.EPOCH);
	}

	final BotConfigExporter exporter = new BotConfigExporter(JsonMapper.builder().build(), CHAIN);

	final List<DiscoveredPool> pools = List.of(pool("0x1", "WETH", "USDC", 500, 10), pool("0x2", "WETH", "USDC", 3000, 5),
			pool("0x3", "WETH", "ARB", 500, 7), pool("0x4", "WETH", "ARB", 3000, 0), // empty pool
			pool("0x5", "USDC", "DAI", 100, 9), pool("0x6", "USDC", "DAI", 500, 9)); // no WETH

	@Test
	void onlyPairsWithWethAndTwoLiquidPoolsAreTradable() {
		assertThat(exporter.tradablePairs(pools)).containsExactly(Map.entry("WETH/USDC", 2));
	}

	@Test
	void exportsABotConfig() {
		var config = exporter.export(pools, 0.0001, 3);
		assertThat(config.get("chain_id").asLong()).isEqualTo(42161);
		assertThat(config.get("tokens").propertyNames()).containsExactly("WETH", "USDC");
		assertThat(config.get("pairs").toString()).isEqualTo("[[\"WETH\",\"USDC\"]]");
		assertThat(config.get("strategy").get("min_profit_weth").asDouble()).isEqualTo(0.0001);
		assertThat(config.get("uniswap_v3").get("factory").asString()).isEqualTo("0xfactory");
		assertThat(config.get("execution").get("enabled").asBoolean()).isFalse();
	}

}
