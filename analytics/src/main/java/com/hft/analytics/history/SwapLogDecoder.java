package com.hft.analytics.history;

import java.math.BigInteger;

/**
 * Decodes the data of a Uniswap V3 Swap log by hand: five 32-byte words, two of them signed
 * (two's complement). Kept independent of web3j so it can be tested on raw hex.
 */
public final class SwapLogDecoder {

	/** Non-indexed fields of Swap(sender, recipient, amount0, amount1, sqrtPriceX96, liquidity, tick). */
	public record SwapData(BigInteger amount0, BigInteger amount1, BigInteger sqrtPriceX96, BigInteger liquidity,
			int tick) {
	}

	private static final BigInteger TWO_256 = BigInteger.ONE.shiftLeft(256);

	private static final BigInteger TWO_255 = BigInteger.ONE.shiftLeft(255);

	private SwapLogDecoder() {
	}

	public static SwapData decode(String dataHex) {
		String hex = dataHex.startsWith("0x") ? dataHex.substring(2) : dataHex;
		if (hex.length() != 5 * 64) {
			throw new IllegalArgumentException("Swap data must be 5 words, got " + hex.length() / 2 + " bytes");
		}
		return new SwapData(signed(word(hex, 0)), signed(word(hex, 1)), word(hex, 2), word(hex, 3),
				signed(word(hex, 4)).intValueExact());
	}

	private static BigInteger word(String hex, int index) {
		return new BigInteger(hex.substring(64 * index, 64 * (index + 1)), 16);
	}

	private static BigInteger signed(BigInteger word) {
		return word.compareTo(TWO_255) >= 0 ? word.subtract(TWO_256) : word;
	}

}
