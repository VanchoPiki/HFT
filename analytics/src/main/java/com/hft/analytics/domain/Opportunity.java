package com.hft.analytics.domain;

import java.math.BigInteger;
import java.time.Instant;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.Table;

@Entity
@Table(name = "opportunities")
public class Opportunity {

	@Id
	@GeneratedValue(strategy = GenerationType.IDENTITY)
	private Long id;

	@Column(name = "run_id", nullable = false)
	private long runId;

	@Column(name = "opportunity_id", nullable = false)
	private long opportunityId;

	@Column(nullable = false)
	private Instant ts;

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

	@Column(name = "token_in", nullable = false, length = 42)
	private String tokenIn;

	@Column(name = "symbol_in", nullable = false, length = 16)
	private String symbolIn;

	@Column(name = "amount_in", nullable = false, precision = 78)
	private BigInteger amountIn;

	@Column(name = "amount_out", nullable = false, precision = 78)
	private BigInteger amountOut;

	@Column(name = "gross_profit", nullable = false, precision = 78)
	private BigInteger grossProfit;

	@Column(name = "gas_cost", nullable = false, precision = 78)
	private BigInteger gasCost;

	@Column(name = "net_profit", nullable = false, precision = 78)
	private BigInteger netProfit;

	@Column(name = "amount_in_human", nullable = false)
	private double amountInHuman;

	@Column(name = "net_profit_human", nullable = false)
	private double netProfitHuman;

	@Column(name = "net_profit_weth", nullable = false)
	private double netProfitWeth;

	@Column(name = "compute_us", nullable = false)
	private long computeUs;

	protected Opportunity() {
	}

	public Opportunity(long runId, long opportunityId, Instant ts, long block, String pair, String poolA, int feeA,
			String poolB, int feeB, String tokenIn, String symbolIn, BigInteger amountIn, BigInteger amountOut,
			BigInteger grossProfit, BigInteger gasCost, BigInteger netProfit, double amountInHuman,
			double netProfitHuman, double netProfitWeth, long computeUs) {
		this.runId = runId;
		this.opportunityId = opportunityId;
		this.ts = ts;
		this.block = block;
		this.pair = pair;
		this.poolA = poolA;
		this.feeA = feeA;
		this.poolB = poolB;
		this.feeB = feeB;
		this.tokenIn = tokenIn;
		this.symbolIn = symbolIn;
		this.amountIn = amountIn;
		this.amountOut = amountOut;
		this.grossProfit = grossProfit;
		this.gasCost = gasCost;
		this.netProfit = netProfit;
		this.amountInHuman = amountInHuman;
		this.netProfitHuman = netProfitHuman;
		this.netProfitWeth = netProfitWeth;
		this.computeUs = computeUs;
	}

	public Long getId() {
		return id;
	}

	public long getRunId() {
		return runId;
	}

	public long getOpportunityId() {
		return opportunityId;
	}

	public Instant getTs() {
		return ts;
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

	public String getSymbolIn() {
		return symbolIn;
	}

	public BigInteger getAmountIn() {
		return amountIn;
	}

	public BigInteger getAmountOut() {
		return amountOut;
	}

	public BigInteger getGrossProfit() {
		return grossProfit;
	}

	public BigInteger getGasCost() {
		return gasCost;
	}

	public BigInteger getNetProfit() {
		return netProfit;
	}

	public double getAmountInHuman() {
		return amountInHuman;
	}

	public double getNetProfitHuman() {
		return netProfitHuman;
	}

	public double getNetProfitWeth() {
		return netProfitWeth;
	}

	public long getComputeUs() {
		return computeUs;
	}

}
