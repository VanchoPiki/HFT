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
@Table(name = "executions")
public class Execution {

	@Id
	@GeneratedValue(strategy = GenerationType.IDENTITY)
	private Long id;

	@Column(name = "run_id", nullable = false)
	private long runId;

	@Column(name = "opportunity_id", nullable = false)
	private long opportunityId;

	@Column(nullable = false)
	private Instant ts;

	@Column(nullable = false, length = 32)
	private String status;

	@Column(name = "tx_hash", length = 66, unique = true)
	private String txHash;

	private Long nonce;

	@Column(name = "gas_limit")
	private Long gasLimit;

	@Column(name = "max_fee_per_gas", precision = 78)
	private BigInteger maxFeePerGas;

	private String error;

	@Column(name = "simulate_us")
	private Long simulateUs;

	@Column(name = "sign_us")
	private Long signUs;

	@Column(name = "send_us")
	private Long sendUs;

	private Boolean success;

	@Column(name = "mined_block")
	private Long minedBlock;

	@Column(name = "gas_used")
	private Long gasUsed;

	@Column(name = "effective_gas_price", precision = 78)
	private BigInteger effectiveGasPrice;

	@Column(name = "realized_profit", precision = 78)
	private BigInteger realizedProfit;

	@Column(name = "realized_profit_weth")
	private Double realizedProfitWeth;

	@Column(name = "mined_at")
	private Instant minedAt;

	protected Execution() {
	}

	public Execution(long runId, long opportunityId, Instant ts, String status, String txHash, Long nonce,
			Long gasLimit, BigInteger maxFeePerGas, String error, Long simulateUs, Long signUs, Long sendUs) {
		this.runId = runId;
		this.opportunityId = opportunityId;
		this.ts = ts;
		this.status = status;
		this.txHash = txHash;
		this.nonce = nonce;
		this.gasLimit = gasLimit;
		this.maxFeePerGas = maxFeePerGas;
		this.error = error;
		this.simulateUs = simulateUs;
		this.signUs = signUs;
		this.sendUs = sendUs;
	}

	public void recordResult(boolean success, long minedBlock, long gasUsed, BigInteger effectiveGasPrice,
			BigInteger realizedProfit, double realizedProfitWeth, Instant minedAt) {
		this.success = success;
		this.status = success ? "mined" : "reverted";
		this.minedBlock = minedBlock;
		this.gasUsed = gasUsed;
		this.effectiveGasPrice = effectiveGasPrice;
		this.realizedProfit = realizedProfit;
		this.realizedProfitWeth = realizedProfitWeth;
		this.minedAt = minedAt;
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

	public String getStatus() {
		return status;
	}

	public String getTxHash() {
		return txHash;
	}

	public Long getNonce() {
		return nonce;
	}

	public Long getGasLimit() {
		return gasLimit;
	}

	public BigInteger getMaxFeePerGas() {
		return maxFeePerGas;
	}

	public String getError() {
		return error;
	}

	public Long getSimulateUs() {
		return simulateUs;
	}

	public Long getSignUs() {
		return signUs;
	}

	public Long getSendUs() {
		return sendUs;
	}

	public Boolean getSuccess() {
		return success;
	}

	public Long getMinedBlock() {
		return minedBlock;
	}

	public Long getGasUsed() {
		return gasUsed;
	}

	public BigInteger getEffectiveGasPrice() {
		return effectiveGasPrice;
	}

	public BigInteger getRealizedProfit() {
		return realizedProfit;
	}

	public Double getRealizedProfitWeth() {
		return realizedProfitWeth;
	}

	public Instant getMinedAt() {
		return minedAt;
	}

}
