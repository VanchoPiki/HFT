package com.hft.analytics.chain;

import java.util.List;
import java.util.Map;

import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties("hft.chain")
public record ChainProperties(String rpcUrl, String logsRpcUrl, long chainId, String factory, String tickLens,
		String quoter, List<Integer> feeTiers, String weth, Map<String, String> tokens, int logChunkBlocks,
		int maxCallsPerSecond) {

	public String wethAddress() {
		return tokens.get(weth);
	}

}
