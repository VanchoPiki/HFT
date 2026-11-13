package com.hft.analytics.ingest;

import java.time.Instant;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

import org.springframework.stereotype.Component;

/** Latest heartbeat of the bot and ingestion counters, kept in memory. */
@Component
public class BotStatus {

	public record Heartbeat(Instant ts, long run, long chainId, long block, int pools, String mode,
			long droppedEvents) {
	}

	private final AtomicReference<Heartbeat> lastHeartbeat = new AtomicReference<>();

	private final AtomicLong eventsReceived = new AtomicLong();

	private final AtomicLong eventsRejected = new AtomicLong();

	void heartbeat(Heartbeat heartbeat) {
		lastHeartbeat.set(heartbeat);
	}

	void received() {
		eventsReceived.incrementAndGet();
	}

	void rejected() {
		eventsRejected.incrementAndGet();
	}

	public Heartbeat lastHeartbeat() {
		return lastHeartbeat.get();
	}

	public long eventsReceived() {
		return eventsReceived.get();
	}

	public long eventsRejected() {
		return eventsRejected.get();
	}

}
