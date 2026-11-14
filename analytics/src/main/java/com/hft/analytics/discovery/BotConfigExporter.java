package com.hft.analytics.discovery;

import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import org.springframework.stereotype.Component;
import tools.jackson.databind.json.JsonMapper;
import tools.jackson.databind.node.ArrayNode;
import tools.jackson.databind.node.ObjectNode;

import com.hft.analytics.chain.ChainProperties;

@Component
public class BotConfigExporter {

	private final JsonMapper json;

	private final ChainProperties chain;

	public BotConfigExporter(JsonMapper json, ChainProperties chain) {
		this.json = json;
		this.chain = chain;
	}

	public Map<String, Integer> tradablePairs(List<DiscoveredPool> pools) {
		Map<String, Integer> liquidPools = new TreeMap<>();
		for (DiscoveredPool p : pools) {
			boolean withWeth = p.getSymbol0().equals(chain.weth()) || p.getSymbol1().equals(chain.weth());
			if (withWeth && p.getLiquidity().signum() > 0) {
				liquidPools.merge(p.pair(), 1, Integer::sum);
			}
		}
		liquidPools.values().removeIf(count -> count < 2);
		return liquidPools;
	}

	public ObjectNode export(List<DiscoveredPool> pools, double minProfitWeth, double maxInputWeth) {
		Map<String, Integer> pairs = tradablePairs(pools);
		Map<String, String> tokens = new LinkedHashMap<>();
		tokens.put(chain.weth(), chain.wethAddress());
		ArrayNode pairList = json.createArrayNode();
		for (String pair : pairs.keySet()) {
			String[] symbols = pair.split("/");
			for (String s : symbols) {
				tokens.putIfAbsent(s, chain.tokens().get(s));
			}
			pairList.add(json.createArrayNode().add(symbols[0]).add(symbols[1]));
		}

		ObjectNode config = json.createObjectNode();
		config.put("chain_id", chain.chainId());
		config.put("rpc_http", "https://arb1.arbitrum.io/rpc");
		config.put("rpc_http_env", "ARBITRUM_RPC_URL");
		config.put("rpc_ws", "");
		config.put("rpc_ws_env", "ARBITRUM_WS_URL");
		config.put("poll_interval_ms", 1000);
		config.put("rpc_timeout_ms", 10000);
		config.put("abi_dir", "../abi");
		ObjectNode uniswap = config.putObject("uniswap_v3");
		uniswap.put("factory", chain.factory());
		uniswap.put("quoter_v2", chain.quoter());
		uniswap.put("tick_lens", chain.tickLens());
		ArrayNode fees = config.putArray("fee_tiers");
		chain.feeTiers().forEach(fees::add);
		config.put("tick_words_each_side", 2);
		ObjectNode tokenNode = config.putObject("tokens");
		tokens.forEach(tokenNode::put);
		config.set("pairs", pairList);
		ObjectNode strategy = config.putObject("strategy");
		strategy.put("min_profit_weth", minProfitWeth);
		strategy.put("gas_units", 350000);
		strategy.put("max_input_weth", maxInputWeth);
		config.putObject("telemetry").put("zmq_endpoint", "tcp://127.0.0.1:5555");
		ObjectNode execution = config.putObject("execution");
		execution.put("enabled", false);
		execution.put("contract", "");
		execution.put("private_key_env", "PRIVATE_KEY");
		return config;
	}

}
