package com.hft.analytics.history;

import java.io.IOException;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.web3j.protocol.core.methods.response.Log;

import com.hft.analytics.chain.ChainClient;
import com.hft.analytics.chain.ChainProperties;
import com.hft.analytics.chain.UniswapAbi;
import com.hft.analytics.discovery.DiscoveredPool;
import com.hft.analytics.discovery.DiscoveredPoolRepository;

@Service
public class SwapHistory {

	public record SwapRecord(String pool, long block, int logIndex, BigInteger sqrtPriceX96, BigInteger liquidity,
			int tick) {
	}

	public record IndexResult(long fromBlock, long toBlock, int pools, int events) {
	}

	private static final Logger log = LoggerFactory.getLogger(SwapHistory.class);

	public static final int BLOCKS_PER_MINUTE = 240;

	private final ChainClient chain;

	private final ChainProperties properties;

	private final DiscoveredPoolRepository pools;

	private final JdbcTemplate jdbc;

	public SwapHistory(ChainClient chain, ChainProperties properties, DiscoveredPoolRepository pools,
			JdbcTemplate jdbc) {
		this.chain = chain;
		this.properties = properties;
		this.pools = pools;
		this.jdbc = jdbc;
	}

	@Transactional
	public IndexResult indexRecent(int minutes) throws IOException {
		List<String> addresses = pools.findAll()
			.stream()
			.filter(p -> p.getLiquidity().signum() > 0)
			.map(DiscoveredPool::getAddress)
			.toList();
		if (addresses.isEmpty()) {
			throw new IllegalStateException("no discovered pools: run discovery first");
		}
		long head = chain.blockNumber();
		long from = Math.max(0, head - (long) minutes * BLOCKS_PER_MINUTE);
		jdbc.update("delete from swap_events where block between ? and ?", from, head);

		int stored = 0;
		for (long start = from; start <= head; start += properties.logChunkBlocks()) {
			long end = Math.min(head, start + properties.logChunkBlocks() - 1);
			List<Log> logs = chain.logs(addresses, UniswapAbi.SWAP_TOPIC, start, end);
			stored += insert(logs);
			log.debug("history: blocks {}..{}: {} swaps", start, end, logs.size());
		}
		jdbc.update("insert into indexed_ranges (from_block, to_block, events, indexed_at) values (?, ?, ?, now())",
				from, head, stored);
		log.info("history: {} swaps of {} pools in blocks {}..{}", stored, addresses.size(), from, head);
		return new IndexResult(from, head, addresses.size(), stored);
	}

	private int insert(List<Log> logs) {
		List<Object[]> rows = new ArrayList<>(logs.size());
		for (Log l : logs) {
			if (l.isRemoved()) {
				continue;
			}
			SwapLogDecoder.SwapData d = SwapLogDecoder.decode(l.getData());
			rows.add(new Object[] { l.getAddress().toLowerCase(Locale.ROOT), l.getBlockNumber().longValueExact(),
					l.getLogIndex().intValueExact(), l.getTransactionHash(), d.amount0(), d.amount1(),
					d.sqrtPriceX96(), d.liquidity(), d.tick() });
		}
		jdbc.batchUpdate("""
				insert into swap_events (pool, block, log_index, tx_hash, amount0, amount1, sqrt_price_x96, liquidity, tick)
				values (?, ?, ?, ?, ?, ?, ?, ?, ?) on conflict do nothing""", rows);
		return rows.size();
	}

	public List<SwapRecord> load(long fromBlock, long toBlock) {
		return jdbc.query("""
				select pool, block, log_index, sqrt_price_x96, liquidity, tick from swap_events
				where block between ? and ? order by block, log_index""",
				(rs, i) -> new SwapRecord(rs.getString(1), rs.getLong(2), rs.getInt(3),
						rs.getBigDecimal(4).toBigIntegerExact(), rs.getBigDecimal(5).toBigIntegerExact(),
						rs.getInt(6)),
				fromBlock, toBlock);
	}

	public record Range(long fromBlock, long toBlock, long events) {
	}

	public java.util.Optional<Range> latestRange() {
		return jdbc.query("select from_block, to_block, events from indexed_ranges order by id desc limit 1",
				(rs, i) -> new Range(rs.getLong(1), rs.getLong(2), rs.getLong(3))).stream().findFirst();
	}

	public boolean isContiguous(long fromBlock, long toBlock) {
		Integer n = jdbc.queryForObject(
				"select count(*) from indexed_ranges where from_block <= ? and to_block >= ?", Integer.class, fromBlock,
				toBlock);
		return n != null && n > 0;
	}

	public record Coverage(long events, Long minBlock, Long maxBlock, Range latestRange) {
	}

	public Coverage coverage() {
		return jdbc.queryForObject("select count(*), min(block), max(block) from swap_events",
				(rs, i) -> new Coverage(rs.getLong(1), (Long) rs.getObject(2), (Long) rs.getObject(3),
						latestRange().orElse(null)));
	}

}
