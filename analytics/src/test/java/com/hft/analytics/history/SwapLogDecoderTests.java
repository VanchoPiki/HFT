package com.hft.analytics.history;

import java.math.BigInteger;

import org.junit.jupiter.api.Test;

import com.hft.analytics.chain.UniswapAbi;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

class SwapLogDecoderTests {

	static final String DATA = "0x0000000000000000000000000000000000000000000000001147a2bb08dfa66b"
			+ "ffffffffffffffffffffffffffffffffffffffffffffffffffffffff52538de2"
			+ "000000000000000000000000000000000000000000032bc9a2321a8e40000000"
			+ "00000000000000000000000000000000000000000000000035e4ee3ff449d831"
			+ "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffcf7b0";

	@Test
	void decodesARealSwap() {
		var d = SwapLogDecoder.decode(DATA);
		assertThat(d.amount0()).isEqualTo(new BigInteger("1245142746182690411"));
		assertThat(d.amount1()).isEqualTo(BigInteger.valueOf(-2913759774L));
		assertThat(d.sqrtPriceX96()).isEqualTo(new BigInteger("3833558700599279129460736"));
		assertThat(d.liquidity()).isEqualTo(new BigInteger("3883490737149040689"));
		assertThat(d.tick()).isEqualTo(-198736);
	}

	@Test
	void rejectsWrongLength() {
		assertThatThrownBy(() -> SwapLogDecoder.decode("0x00")).isInstanceOf(IllegalArgumentException.class);
	}

	@Test
	void swapTopicMatchesTheChain() {
		assertThat(UniswapAbi.SWAP_TOPIC)
			.isEqualTo("0xc42079f94a6350d7e6235f29174924f928cc2ac818eb64fed8004e115fbcca67");
	}

}
