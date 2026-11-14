package com.hft.analytics.chain;

import java.math.BigInteger;
import java.util.List;

import org.web3j.abi.EventEncoder;
import org.web3j.abi.FunctionEncoder;
import org.web3j.abi.FunctionReturnDecoder;
import org.web3j.abi.TypeReference;
import org.web3j.abi.datatypes.Address;
import org.web3j.abi.datatypes.Event;
import org.web3j.abi.datatypes.Function;
import org.web3j.abi.datatypes.Type;
import org.web3j.abi.datatypes.Utf8String;
import org.web3j.abi.datatypes.generated.Int24;
import org.web3j.abi.datatypes.generated.Int256;
import org.web3j.abi.datatypes.generated.Uint128;
import org.web3j.abi.datatypes.generated.Uint16;
import org.web3j.abi.datatypes.generated.Uint160;
import org.web3j.abi.datatypes.generated.Uint24;
import org.web3j.abi.datatypes.generated.Uint8;

public final class UniswapAbi {

	public static final Event SWAP = new Event("Swap",
			List.of(new TypeReference<Address>(true) {
			}, new TypeReference<Address>(true) {
			}, new TypeReference<Int256>() {
			}, new TypeReference<Int256>() {
			}, new TypeReference<Uint160>() {
			}, new TypeReference<Uint128>() {
			}, new TypeReference<Int24>() {
			}));

	public static final String SWAP_TOPIC = EventEncoder.encode(SWAP);

	private UniswapAbi() {
	}

	public static Function getPool(String tokenA, String tokenB, int fee) {
		return new Function("getPool", List.of(new Address(tokenA), new Address(tokenB), new Uint24(fee)),
				List.of(new TypeReference<Address>() {
				}));
	}

	public static Function tickSpacing() {
		return new Function("tickSpacing", List.of(), List.of(new TypeReference<Int24>() {
		}));
	}

	public static Function liquidity() {
		return new Function("liquidity", List.of(), List.of(new TypeReference<Uint128>() {
		}));
	}

	public static Function slot0() {
		return new Function("slot0", List.of(),
				List.of(new TypeReference<Uint160>() {
				}, new TypeReference<Int24>() {
				}, new TypeReference<Uint16>() {
				}, new TypeReference<Uint16>() {
				}, new TypeReference<Uint16>() {
				}, new TypeReference<Uint8>() {
				}, new TypeReference<org.web3j.abi.datatypes.Bool>() {
				}));
	}

	public static Function decimals() {
		return new Function("decimals", List.of(), List.of(new TypeReference<Uint8>() {
		}));
	}

	public static Function symbol() {
		return new Function("symbol", List.of(), List.of(new TypeReference<Utf8String>() {
		}));
	}

	public static String encode(Function function) {
		return FunctionEncoder.encode(function);
	}

	@SuppressWarnings("rawtypes")
	public static List<Type> decode(Function function, String hex) {
		if (hex == null || hex.equals("0x")) {
			return List.of();
		}
		return FunctionReturnDecoder.decode(hex, function.getOutputParameters());
	}

	public static BigInteger uint(List<?> outputs, int index) {
		return (BigInteger) ((Type<?>) outputs.get(index)).getValue();
	}

}
