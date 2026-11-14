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
@Table(name = "pool_snapshots")
public class PoolSnapshot {

	@Id
	@GeneratedValue(strategy = GenerationType.IDENTITY)
	private Long id;

	@Column(nullable = false)
	private Instant ts;

	@Column(nullable = false)
	private long block;

	@Column(nullable = false, length = 42)
	private String pool;

	@Column(nullable = false, length = 32)
	private String pair;

	@Column(nullable = false)
	private int fee;

	@Column(nullable = false)
	private double price;

	@Column(nullable = false)
	private int tick;

	@Column(name = "sqrt_price_x96", nullable = false, precision = 78)
	private BigInteger sqrtPriceX96;

	@Column(nullable = false, precision = 78)
	private BigInteger liquidity;

	protected PoolSnapshot() {
	}

	public PoolSnapshot(Instant ts, long block, String pool, String pair, int fee, double price, int tick,
			BigInteger sqrtPriceX96, BigInteger liquidity) {
		this.ts = ts;
		this.block = block;
		this.pool = pool;
		this.pair = pair;
		this.fee = fee;
		this.price = price;
		this.tick = tick;
		this.sqrtPriceX96 = sqrtPriceX96;
		this.liquidity = liquidity;
	}

	public Long getId() {
		return id;
	}

	public Instant getTs() {
		return ts;
	}

	public long getBlock() {
		return block;
	}

	public String getPool() {
		return pool;
	}

	public String getPair() {
		return pair;
	}

	public int getFee() {
		return fee;
	}

	public double getPrice() {
		return price;
	}

	public int getTick() {
		return tick;
	}

	public BigInteger getSqrtPriceX96() {
		return sqrtPriceX96;
	}

	public BigInteger getLiquidity() {
		return liquidity;
	}

}
