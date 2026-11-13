package com.hft.analytics.backtest;

import java.time.Instant;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.Table;

@Entity
@Table(name = "backtest_runs")
public class BacktestRun {

	@Id
	@GeneratedValue(strategy = GenerationType.IDENTITY)
	private Long id;

	@Column(name = "created_at", nullable = false)
	private Instant createdAt;

	@Column(name = "from_block", nullable = false)
	private long fromBlock;

	@Column(name = "to_block", nullable = false)
	private long toBlock;

	@Column(name = "swap_events", nullable = false)
	private int swapEvents;

	@Column(name = "min_profit_weth", nullable = false)
	private double minProfitWeth;

	@Column(name = "gas_units", nullable = false)
	private long gasUnits;

	@Column(name = "gas_price_gwei", nullable = false)
	private double gasPriceGwei;

	@Column(nullable = false)
	private int opportunities;

	@Column(name = "profitable_blocks", nullable = false)
	private int profitableBlocks;

	@Column(name = "total_profit_weth", nullable = false)
	private double totalProfitWeth;

	@Column(name = "best_profit_weth", nullable = false)
	private double bestProfitWeth;

	protected BacktestRun() {
	}

	public BacktestRun(Instant createdAt, long fromBlock, long toBlock, Backtester.Params params,
			Backtester.Result result) {
		this.createdAt = createdAt;
		this.fromBlock = fromBlock;
		this.toBlock = toBlock;
		this.swapEvents = result.events();
		this.minProfitWeth = params.minProfitWeth();
		this.gasUnits = params.gasUnits();
		this.gasPriceGwei = params.gasPriceGwei();
		this.opportunities = result.opportunities().size();
		this.profitableBlocks = result.profitableBlocks();
		this.totalProfitWeth = result.totalProfitWeth();
		this.bestProfitWeth = result.bestProfitWeth();
	}

	public Long getId() {
		return id;
	}

	public Instant getCreatedAt() {
		return createdAt;
	}

	public long getFromBlock() {
		return fromBlock;
	}

	public long getToBlock() {
		return toBlock;
	}

	public int getSwapEvents() {
		return swapEvents;
	}

	public double getMinProfitWeth() {
		return minProfitWeth;
	}

	public long getGasUnits() {
		return gasUnits;
	}

	public double getGasPriceGwei() {
		return gasPriceGwei;
	}

	public int getOpportunities() {
		return opportunities;
	}

	public int getProfitableBlocks() {
		return profitableBlocks;
	}

	public double getTotalProfitWeth() {
		return totalProfitWeth;
	}

	public double getBestProfitWeth() {
		return bestProfitWeth;
	}

}
