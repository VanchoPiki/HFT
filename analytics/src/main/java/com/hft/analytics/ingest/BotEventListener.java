package com.hft.analytics.ingest;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.SmartLifecycle;
import org.springframework.stereotype.Component;
import org.zeromq.SocketType;
import org.zeromq.ZContext;
import org.zeromq.ZMQ;

@Component
public class BotEventListener implements SmartLifecycle {

	private static final Logger log = LoggerFactory.getLogger(BotEventListener.class);

	private final EventIngestService ingest;

	private final String endpoint;

	private final boolean enabled;

	private volatile boolean running;

	private Thread thread;

	public BotEventListener(EventIngestService ingest, @Value("${hft.bot.events-endpoint}") String endpoint,
			@Value("${hft.bot.listener-enabled:true}") boolean enabled) {
		this.ingest = ingest;
		this.endpoint = endpoint;
		this.enabled = enabled;
	}

	@Override
	public void start() {
		if (!enabled) {
			log.info("bot event listener disabled");
			return;
		}
		running = true;
		thread = Thread.ofPlatform().name("bot-events").daemon().start(this::loop);
	}

	private void loop() {
		try (ZContext context = new ZContext(); ZMQ.Socket socket = context.createSocket(SocketType.SUB)) {
			socket.subscribe(ZMQ.SUBSCRIPTION_ALL);
			socket.setReceiveTimeOut(500);
			socket.connect(endpoint);
			log.info("listening for bot events on {}", endpoint);
			while (running) {
				String message = socket.recvStr();
				if (message == null) {
					continue;
				}
				try {
					ingest.ingest(message);
				}
				catch (RuntimeException ex) {
					log.warn("dropped bot event: {}", ex.getMessage());
				}
			}
		}
	}

	@Override
	public void stop() {
		running = false;
		if (thread != null) {
			try {
				thread.join(2000);
			}
			catch (InterruptedException ex) {
				Thread.currentThread().interrupt();
			}
		}
	}

	@Override
	public boolean isRunning() {
		return running;
	}

}
