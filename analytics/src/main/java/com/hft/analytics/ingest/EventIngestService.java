package com.hft.analytics.ingest;

import java.math.BigInteger;
import java.time.Instant;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import tools.jackson.databind.JsonNode;
import tools.jackson.databind.json.JsonMapper;

import com.hft.analytics.domain.Execution;
import com.hft.analytics.domain.ExecutionRepository;
import com.hft.analytics.domain.Opportunity;
import com.hft.analytics.domain.OpportunityRepository;
import com.hft.analytics.domain.PoolSnapshot;
import com.hft.analytics.domain.PoolSnapshotRepository;

/** Turns one JSON event from the bot (format: schema/events.md) into database rows. */
@Service
public class EventIngestService {

	private static final Logger log = LoggerFactory.getLogger(EventIngestService.class);

	private final JsonMapper json;

	private final PoolSnapshotRepository snapshots;

	private final OpportunityRepository opportunities;

	private final ExecutionRepository executions;

	private final BotStatus status;

	public EventIngestService(JsonMapper json, PoolSnapshotRepository snapshots, OpportunityRepository opportunities,
			ExecutionRepository executions, BotStatus status) {
		this.json = json;
		this.snapshots = snapshots;
		this.opportunities = opportunities;
		this.executions = executions;
		this.status = status;
	}

	/**
	 * Stores the event.
	 * @throws IllegalArgumentException if the message is not a valid event
	 */
	@Transactional
	public void ingest(String message) {
		final JsonNode e;
		try {
			e = json.readTree(message);
		}
		catch (RuntimeException ex) {
			status.rejected();
			throw new IllegalArgumentException("not JSON: " + ex.getMessage(), ex);
		}
		status.received();
		String type = e.path("type").asString();
		switch (type) {
			case "pool_snapshot" -> snapshots.save(new PoolSnapshot(ts(e), e.path("block").asLong(),
					text(e, "pool"), text(e, "pair"), e.path("fee").asInt(), e.path("price").asDouble(),
					e.path("tick").asInt(), big(e, "sqrt_price_x96"), big(e, "liquidity")));
			case "opportunity" -> ingestOpportunity(e);
			case "execution" -> executions.save(new Execution(e.path("run").asLong(),
					e.path("opportunity_id").asLong(), ts(e), text(e, "status"), optionalText(e, "tx_hash"),
					optionalLong(e, "nonce"), optionalLong(e, "gas_limit"), optionalBig(e, "max_fee_per_gas"),
					optionalText(e, "error"), optionalLong(e, "simulate_us"), optionalLong(e, "sign_us"),
					optionalLong(e, "send_us")));
			case "tx_result" -> ingestTxResult(e);
			case "heartbeat" -> status.heartbeat(new BotStatus.Heartbeat(ts(e), e.path("run").asLong(),
					e.path("chain_id").asLong(), e.path("block").asLong(), e.path("pools").asInt(),
					text(e, "mode"), e.path("dropped_events").asLong()));
			default -> {
				status.rejected();
				throw new IllegalArgumentException("unknown event type '" + type + "'");
			}
		}
	}

	private void ingestOpportunity(JsonNode e) {
		long run = e.path("run").asLong();
		long id = e.path("id").asLong();
		if (opportunities.existsByRunIdAndOpportunityId(run, id)) {
			return; // duplicate delivery
		}
		opportunities.save(new Opportunity(run, id, ts(e), e.path("block").asLong(), text(e, "pair"),
				text(e, "pool_a"), e.path("fee_a").asInt(), text(e, "pool_b"), e.path("fee_b").asInt(),
				text(e, "token_in"), text(e, "symbol_in"), big(e, "amount_in"), big(e, "amount_out"),
				big(e, "gross_profit"), big(e, "gas_cost"), big(e, "net_profit"), e.path("amount_in_human").asDouble(),
				e.path("net_profit_human").asDouble(), e.path("net_profit_weth").asDouble(),
				e.path("compute_us").asLong()));
	}

	private void ingestTxResult(JsonNode e) {
		String txHash = text(e, "tx_hash");
		Execution execution = executions.findByTxHash(txHash).orElse(null);
		if (execution == null) {
			// The execution event was lost (e.g. analytics started later): keep the result anyway.
			execution = new Execution(e.path("run").asLong(), e.path("opportunity_id").asLong(), ts(e), "sent", txHash,
					null, null, null, null, null, null, null);
		}
		execution.recordResult(e.path("success").asBoolean(), e.path("block").asLong(), e.path("gas_used").asLong(),
				big(e, "effective_gas_price"), big(e, "realized_profit"), e.path("realized_profit_weth").asDouble(),
				ts(e));
		executions.save(execution);
		log.info("tx {} mined in block {}: success={}", txHash, e.path("block").asLong(), e.path("success").asBoolean());
	}

	private static Instant ts(JsonNode e) {
		return Instant.ofEpochMilli(e.path("ts_ms").asLong());
	}

	private static String text(JsonNode e, String field) {
		JsonNode node = e.get(field);
		if (node == null || !node.isString()) {
			throw new IllegalArgumentException("missing string field '" + field + "'");
		}
		return node.asString();
	}

	private static String optionalText(JsonNode e, String field) {
		JsonNode node = e.get(field);
		return node == null || node.isNull() || node.asString().isEmpty() ? null : node.asString();
	}

	private static Long optionalLong(JsonNode e, String field) {
		JsonNode node = e.get(field);
		return node == null || node.isNull() ? null : node.asLong();
	}

	private static BigInteger big(JsonNode e, String field) {
		return new BigInteger(text(e, field));
	}

	private static BigInteger optionalBig(JsonNode e, String field) {
		String value = optionalText(e, field);
		return value == null ? null : new BigInteger(value);
	}

}
