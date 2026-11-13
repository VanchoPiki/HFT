package com.hft.analytics.backtest;

import java.time.Instant;
import java.util.Comparator;
import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.domain.Limit;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import com.hft.analytics.chain.ChainProperties;
import com.hft.analytics.discovery.DiscoveredPoolRepository;
import com.hft.analytics.history.SwapHistory;

/** Runs the backtester over the indexed swap history and stores the results. */
@Service
public class BacktestService {

	public record Report(BacktestRun run, List<BacktestOpportunity> top) {
	}

	private static final Logger log = LoggerFactory.getLogger(BacktestService.class);

	private static final int STORED_OPPORTUNITIES = 500;

	private final SwapHistory history;

	private final DiscoveredPoolRepository pools;

	private final ChainProperties chain;

	private final BacktestRunRepository runs;

	private final BacktestOpportunityRepository opportunities;

	BacktestService(SwapHistory history, DiscoveredPoolRepository pools, ChainProperties chain,
			BacktestRunRepository runs, BacktestOpportunityRepository opportunities) {
		this.history = history;
		this.pools = pools;
		this.chain = chain;
		this.runs = runs;
		this.opportunities = opportunities;
	}

	@Transactional
	public Report run(Long fromBlock, Long toBlock, double minProfitWeth, long gasUnits, double gasPriceGwei) {
		SwapHistory.Range latest = history.latestRange()
			.orElseThrow(() -> new IllegalStateException("no swap history: index it first"));
		long from = fromBlock != null ? fromBlock : latest.fromBlock();
		long to = toBlock != null ? toBlock : latest.toBlock();
		if (!history.isContiguous(from, to)) {
			// Across a gap a pool's last swap is stale and would show dislocations that never existed.
			throw new IllegalStateException("blocks " + from + ".." + to + " are not covered by one indexing run");
		}

		List<Backtester.Pool> poolList = pools.findAll()
			.stream()
			.map(p -> new Backtester.Pool(p.getAddress(), p.pair(), p.getSymbol0(), p.getSymbol1(), p.getDecimals0(),
					p.getDecimals1(), p.getFee(), p.getTickSpacing()))
			.toList();
		List<Backtester.Swap> swaps = history.load(from, to)
			.stream()
			.map(s -> new Backtester.Swap(s.pool(), s.block(), s.logIndex(), s.sqrtPriceX96(), s.liquidity(),
					s.tick()))
			.toList();

		Backtester.Params params = new Backtester.Params(minProfitWeth, gasUnits, gasPriceGwei, chain.weth());
		long started = System.nanoTime();
		Backtester.Result result = Backtester.run(poolList, swaps, params);
		log.info("backtest: {} swaps in blocks {}..{} -> {} opportunities, {} WETH in {} ms", swaps.size(), from, to,
				result.opportunities().size(), result.totalProfitWeth(), (System.nanoTime() - started) / 1_000_000);

		BacktestRun run = runs.save(new BacktestRun(Instant.now(), from, to, params, result));
		opportunities.saveAll(result.opportunities()
			.stream()
			.sorted(Comparator.comparingDouble(Backtester.Found::profitWeth).reversed())
			.limit(STORED_OPPORTUNITIES)
			.map(f -> new BacktestOpportunity(run.getId(), f))
			.toList());
		return report(run.getId());
	}

	public Report report(long runId) {
		BacktestRun run = runs.findById(runId).orElseThrow();
		return new Report(run, opportunities.findByRunIdOrderByProfitWethDesc(runId, Limit.of(50)));
	}

	public List<BacktestRun> recentRuns() {
		return runs.findAllByOrderByIdDesc(Limit.of(20));
	}

}
