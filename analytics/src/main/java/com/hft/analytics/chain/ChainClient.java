package com.hft.analytics.chain;

import java.io.IOException;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.List;

import jakarta.annotation.PreDestroy;
import org.springframework.stereotype.Component;
import org.web3j.protocol.Web3j;
import org.web3j.protocol.core.BatchRequest;
import org.web3j.protocol.core.DefaultBlockParameter;
import org.web3j.protocol.core.DefaultBlockParameterName;
import org.web3j.protocol.core.Response;
import org.web3j.protocol.core.methods.request.EthFilter;
import org.web3j.protocol.core.methods.request.Transaction;
import org.web3j.protocol.core.methods.response.EthCall;
import org.web3j.protocol.core.methods.response.EthLog;
import org.web3j.protocol.core.methods.response.Log;
import org.web3j.protocol.exceptions.ClientConnectionException;
import org.web3j.protocol.http.HttpService;

/**
 * Read-only access to the chain through web3j. Used for research (discovery, history),
 * never on the trading path: that lives in the C++ bot.
 */
@Component
public class ChainClient {

	/** One eth_call: target contract and ABI-encoded calldata. */
	public record Call(String to, String data) {
	}

	/** Free plans meter compute units per second (Alchemy: ~500 CU/s, 26 per eth_call). */
	private static final int BATCH_SIZE = 10;

	private static final int RATE_LIMIT_ROUNDS = 6;

	private final Web3j web3j;

	private final Web3j logsWeb3j;

	private final double callsPerSecond;

	private long nextSlotNanos = System.nanoTime();

	public ChainClient(ChainProperties properties) {
		this.web3j = Web3j.build(new HttpService(properties.rpcUrl()));
		this.logsWeb3j = properties.logsRpcUrl().equals(properties.rpcUrl()) ? web3j
				: Web3j.build(new HttpService(properties.logsRpcUrl()));
		this.callsPerSecond = Math.max(1, properties.maxCallsPerSecond());
	}

	/** Spreads requests out so that {@code calls} more stay within the per-second budget. */
	private synchronized void pace(int calls) throws IOException {
		long now = System.nanoTime();
		long start = Math.max(now, nextSlotNanos);
		nextSlotNanos = start + (long) (calls * 1e9 / callsPerSecond);
		if (start > now) {
			sleep((start - now) / 1_000_000);
		}
	}

	@FunctionalInterface
	private interface RpcCall<T> {

		T send() throws IOException;

	}

	/** Retries a request the provider rejected with HTTP 429, pausing longer each time. */
	private static <T> T withBackoff(RpcCall<T> call) throws IOException {
		for (int round = 0;; round++) {
			try {
				return call.send();
			}
			catch (ClientConnectionException ex) {
				if (!String.valueOf(ex.getMessage()).contains("429") || round + 1 >= RATE_LIMIT_ROUNDS) {
					throw ex;
				}
				sleep(500L << round);
			}
		}
	}

	public long blockNumber() throws IOException {
		pace(1);
		return withBackoff(() -> web3j.ethBlockNumber().send()).getBlockNumber().longValueExact();
	}

	/**
	 * Runs many eth_calls as JSON-RPC batches. Each result is the hex return data, or null
	 * if that call reverted. Calls the provider throttled are retried after a pause.
	 */
	public List<String> callAll(List<Call> calls) throws IOException {
		List<String> results = new ArrayList<>(java.util.Collections.nCopies(calls.size(), (String) null));
		List<Integer> pending = new ArrayList<>();
		for (int i = 0; i < calls.size(); i++) {
			pending.add(i);
		}
		for (int round = 0; !pending.isEmpty(); round++) {
			List<Integer> throttled = new ArrayList<>();
			for (int start = 0; start < pending.size(); start += BATCH_SIZE) {
				List<Integer> chunk = pending.subList(start, Math.min(pending.size(), start + BATCH_SIZE));
				BatchRequest batch = web3j.newBatch();
				for (int i : chunk) {
					Call call = calls.get(i);
					batch.add(web3j.ethCall(Transaction.createEthCallTransaction(null, call.to(), call.data()),
							DefaultBlockParameterName.LATEST));
				}
				pace(chunk.size());
				List<? extends Response<?>> responses = withBackoff(batch::send).getResponses();
				for (int k = 0; k < responses.size(); k++) {
					EthCall result = (EthCall) responses.get(k);
					if (result.hasError() && isRateLimit(result.getError())) {
						throttled.add(chunk.get(k));
					}
					else if (!result.hasError() && !result.isReverted()) {
						results.set(chunk.get(k), result.getValue());
					}
				}
			}
			if (throttled.isEmpty()) {
				break;
			}
			if (round + 1 >= RATE_LIMIT_ROUNDS) {
				throw new IOException(throttled.size() + " calls still rate limited by the provider");
			}
			sleep(500L << round);
			pending = throttled;
		}
		return results;
	}

	private static boolean isRateLimit(Response.Error error) {
		String message = String.valueOf(error.getMessage());
		return error.getCode() == 429 || message.contains("capacity") || message.contains("rate limit")
				|| message.contains("Too Many");
	}

	private static void sleep(long millis) throws IOException {
		try {
			Thread.sleep(millis);
		}
		catch (InterruptedException ex) {
			Thread.currentThread().interrupt();
			throw new IOException("interrupted", ex);
		}
	}

	/** Logs of {@code topic0} emitted by {@code addresses} in [fromBlock, toBlock]. */
	public List<Log> logs(List<String> addresses, String topic0, long fromBlock, long toBlock) throws IOException {
		EthFilter filter = new EthFilter(DefaultBlockParameter.valueOf(BigInteger.valueOf(fromBlock)),
				DefaultBlockParameter.valueOf(BigInteger.valueOf(toBlock)), addresses);
		filter.addSingleTopic(topic0);
		pace(1);
		EthLog response = withBackoff(() -> logsWeb3j.ethGetLogs(filter).send());
		if (response.hasError()) {
			throw new IOException("eth_getLogs: " + response.getError().getMessage());
		}
		List<Log> logs = new ArrayList<>();
		for (EthLog.LogResult<?> result : response.getLogs()) {
			logs.add((Log) result.get());
		}
		return logs;
	}

	@PreDestroy
	void close() {
		web3j.shutdown();
		if (logsWeb3j != web3j) {
			logsWeb3j.shutdown();
		}
	}

}
