package com.hft.analytics.api;

import java.math.BigInteger;
import java.time.Duration;
import java.time.Instant;
import java.util.List;

import org.springframework.data.domain.Limit;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import com.hft.analytics.domain.Execution;
import com.hft.analytics.domain.ExecutionRepository;
import com.hft.analytics.domain.Opportunity;
import com.hft.analytics.domain.OpportunityRepository;
import com.hft.analytics.domain.PoolSnapshot;
import com.hft.analytics.domain.PoolSnapshotRepository;
import com.hft.analytics.ingest.BotStatus;

@RestController
@RequestMapping("/api")
public class DashboardController {

	public record PricePoint(Instant ts, long block, String pool, int fee, double price) {
	}

	public record Stats(long poolSnapshots, long opportunities, double expectedProfitWeth, double avgComputeUs,
			long executions, long mined, long reverted, double successRate, double realizedProfitWeth,
			BigInteger gasSpentWei, long eventsReceived, long eventsRejected, BotStatus.Heartbeat lastHeartbeat) {
	}

	private final PoolSnapshotRepository snapshots;

	private final OpportunityRepository opportunities;

	private final ExecutionRepository executions;

	private final BotStatus status;

	public DashboardController(PoolSnapshotRepository snapshots, OpportunityRepository opportunities,
			ExecutionRepository executions, BotStatus status) {
		this.snapshots = snapshots;
		this.opportunities = opportunities;
		this.executions = executions;
		this.status = status;
	}

	@GetMapping("/pairs")
	public List<String> pairs() {
		return snapshots.findPairs();
	}

	@GetMapping("/pools")
	public List<PoolSnapshot> latestPools() {
		return snapshots.findLatestPerPool();
	}

	@GetMapping("/prices")
	public List<PricePoint> prices(@RequestParam String pair, @RequestParam(defaultValue = "60") int minutes) {
		Instant after = Instant.now().minus(Duration.ofMinutes(Math.clamp(minutes, 1, 24 * 60)));
		return snapshots.findByPairAndTsAfterOrderByTsAsc(pair, after)
			.stream()
			.map(s -> new PricePoint(s.getTs(), s.getBlock(), s.getPool(), s.getFee(), s.getPrice()))
			.toList();
	}

	@GetMapping("/opportunities")
	public List<Opportunity> opportunities(@RequestParam(defaultValue = "50") int limit) {
		return opportunities.findAllByOrderByTsDesc(Limit.of(Math.clamp(limit, 1, 1000)));
	}

	@GetMapping("/executions")
	public List<Execution> executions(@RequestParam(defaultValue = "50") int limit) {
		return executions.findAllByOrderByTsDesc(Limit.of(Math.clamp(limit, 1, 1000)));
	}

	public record PnlPoint(Instant ts, double realizedWeth, double cumulativeWeth) {
	}

	@GetMapping("/pnl")
	public List<PnlPoint> pnl() {
		List<PnlPoint> points = new java.util.ArrayList<>();
		double total = 0;
		for (Execution e : executions.findAllByMinedAtIsNotNullOrderByMinedAtAsc()) {
			double realized = e.getRealizedProfitWeth() == null ? 0 : e.getRealizedProfitWeth();
			total += realized;
			points.add(new PnlPoint(e.getMinedAt(), realized, total));
		}
		return points;
	}

	public record Bucket(long fromUs, long toUs, long count) {
	}

	public record Latency(List<Bucket> decision, double p50DecisionUs, double p99DecisionUs, double avgSimulateUs,
			double avgSignUs, double avgSendUs) {
	}

	@GetMapping("/latency")
	public Latency latency() {
		long[] samples = opportunities.findAllComputeUs().stream().mapToLong(Long::longValue).sorted().toArray();
		long[] edges = { 0, 100, 250, 500, 1000, 2500, 5000, 10000, Long.MAX_VALUE };
		List<Bucket> buckets = new java.util.ArrayList<>();
		for (int i = 0; i + 1 < edges.length; i++) {
			long lo = edges[i];
			long hi = edges[i + 1];
			buckets.add(new Bucket(lo, hi, java.util.Arrays.stream(samples).filter(v -> v >= lo && v < hi).count()));
		}
		return new Latency(buckets, percentile(samples, 0.50), percentile(samples, 0.99),
				executions.averageSimulateUs(), executions.averageSignUs(), executions.averageSendUs());
	}

	private static double percentile(long[] sorted, double q) {
		return sorted.length == 0 ? 0 : sorted[(int) Math.min(sorted.length - 1, Math.floor(q * sorted.length))];
	}

	@GetMapping("/stats")
	public Stats stats() {
		long mined = executions.countBySuccess(true);
		long reverted = executions.countBySuccess(false);
		long finished = mined + reverted;
		return new Stats(snapshots.count(), opportunities.count(), opportunities.totalExpectedProfitWeth(),
				opportunities.averageComputeUs(), executions.count(), mined, reverted,
				finished == 0 ? 0 : (double) mined / finished, executions.totalRealizedProfitWeth(),
				executions.totalGasWei(), status.eventsReceived(), status.eventsRejected(), status.lastHeartbeat());
	}

}
