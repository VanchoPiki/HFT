package com.hft.analytics.backtest;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.Table;

@Entity
@Table(name = "backtest_opportunities")
public class BacktestOpportunity {

	@Id
	@GeneratedValue(strategy = GenerationType.IDENTITY)
	private Long id;

	@Column(name = "run_id", nullable = false)
	private long runId;

	@Column(nullable = false)
	private long block;

	@Column(nullable = false, length = 32)
	private String pair;

	@Column(name = "pool_a", nullable = false, length = 42)
	private String poolA;

	@Column(name = "fee_a", nullable = false)
	private int feeA;

	@Column(name = "pool_b", nullable = false, length = 42)
	private String poolB;

	@Column(name = "fee_b", nullable = false)
	private int feeB;

	@Column(name = "token_in", nullable = false, length = 16)
	private String tokenIn;

	@Column(name = "amount_in", nullable = false)
	private double amountIn;

	@Column(name = "profit_weth", nullable = false)
	private double profitWeth;

	protected BacktestOpportunity() {
	}

	public BacktestOpportunity(long runId, Backtester.Found f) {
		this.runId = runId;
		this.block = f.block();
		this.pair = f.pair();
		this.poolA = f.poolA();
		this.feeA = f.feeA();
		this.poolB = f.poolB();
		this.feeB = f.feeB();
		this.tokenIn = f.tokenIn();
		this.amountIn = f.amountIn();
		this.profitWeth = f.profitWeth();
	}

	public Long getId() {
		return id;
	}

	public long getRunId() {
		return runId;
	}

	public long getBlock() {
		return block;
	}

	public String getPair() {
		return pair;
	}

	public String getPoolA() {
		return poolA;
	}

	public int getFeeA() {
		return feeA;
	}

	public String getPoolB() {
		return poolB;
	}

	public int getFeeB() {
		return feeB;
	}

	public String getTokenIn() {
		return tokenIn;
	}

	public double getAmountIn() {
		return amountIn;
	}

	public double getProfitWeth() {
		return profitWeth;
	}

}
