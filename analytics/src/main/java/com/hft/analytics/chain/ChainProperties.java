package com.hft.analytics.chain;

import java.util.List;
import java.util.Map;

import org.springframework.boot.context.properties.ConfigurationProperties;

/**
 * {@code hft.chain.*}: which chain, contracts and tokens the research tools look at.
 * {@code logsRpcUrl} serves eth_getLogs separately because free provider plans cap its
 * block range (Alchemy: 10 blocks), which would make history indexing crawl.
 */
@ConfigurationProperties("hft.chain")
public record ChainProperties(String rpcUrl, String logsRpcUrl, long chainId, String factory, String tickLens,
		String quoter, List<Integer> feeTiers, String weth, Map<String, String> tokens, int logChunkBlocks,
		int maxCallsPerSecond) {

	public String wethAddress() {
		return tokens.get(weth);
	}

}
