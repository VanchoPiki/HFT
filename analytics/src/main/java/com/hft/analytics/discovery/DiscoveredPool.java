package com.hft.analytics.discovery;

import java.math.BigInteger;
import java.time.Instant;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.Id;
import jakarta.persistence.Table;

/** A Uniswap V3 pool found through the factory, with its state at discovery time. */
@Entity
@Table(name = "discovered_pools")
public class DiscoveredPool {

	@Id
	@Column(length = 42)
	private String address;

	@Column(nullable = false, length = 42)
	private String token0;

	@Column(nullable = false, length = 42)
	private String token1;

	@Column(nullable = false, length = 16)
	private String symbol0;

	@Column(nullable = false, length = 16)
	private String symbol1;

	@Column(nullable = false)
	private int decimals0;

	@Column(nullable = false)
	private int decimals1;

	@Column(nullable = false)
	private int fee;

	@Column(name = "tick_spacing", nullable = false)
	private int tickSpacing;

	@Column(nullable = false, precision = 78)
	private BigInteger liquidity;

	@Column(name = "sqrt_price_x96", nullable = false, precision = 78)
	private BigInteger sqrtPriceX96;

	@Column(nullable = false)
	private double price;

	@Column(name = "discovered_at", nullable = false)
	private Instant discoveredAt;

	protected DiscoveredPool() {
	}

	public DiscoveredPool(String address, String token0, String token1, String symbol0, String symbol1, int decimals0,
			int decimals1, int fee, int tickSpacing, BigInteger liquidity, BigInteger sqrtPriceX96,
			Instant discoveredAt) {
		this.address = address;
		this.token0 = token0;
		this.token1 = token1;
		this.symbol0 = symbol0;
		this.symbol1 = symbol1;
		this.decimals0 = decimals0;
		this.decimals1 = decimals1;
		this.fee = fee;
		this.tickSpacing = tickSpacing;
		this.liquidity = liquidity;
		this.sqrtPriceX96 = sqrtPriceX96;
		this.price = priceOf(sqrtPriceX96, decimals0, decimals1);
		this.discoveredAt = discoveredAt;
	}

	/** token1 per token0 with decimals applied: (sqrtPriceX96 / 2^96)^2 * 10^(d0 - d1). */
	public static double priceOf(BigInteger sqrtPriceX96, int decimals0, int decimals1) {
		double sqrt = sqrtPriceX96.doubleValue() / Math.pow(2, 96);
		return sqrt * sqrt * Math.pow(10, decimals0 - decimals1);
	}

	public String pair() {
		return symbol0 + "/" + symbol1;
	}

	public String getAddress() {
		return address;
	}

	public String getToken0() {
		return token0;
	}

	public String getToken1() {
		return token1;
	}

	public String getSymbol0() {
		return symbol0;
	}

	public String getSymbol1() {
		return symbol1;
	}

	public int getDecimals0() {
		return decimals0;
	}

	public int getDecimals1() {
		return decimals1;
	}

	public int getFee() {
		return fee;
	}

	public int getTickSpacing() {
		return tickSpacing;
	}

	public BigInteger getLiquidity() {
		return liquidity;
	}

	public BigInteger getSqrtPriceX96() {
		return sqrtPriceX96;
	}

	public double getPrice() {
		return price;
	}

	public Instant getDiscoveredAt() {
		return discoveredAt;
	}

}
