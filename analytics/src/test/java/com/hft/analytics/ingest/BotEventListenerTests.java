package com.hft.analytics.ingest;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.context.annotation.Import;
import org.zeromq.SocketType;
import org.zeromq.ZContext;
import org.zeromq.ZMQ;

import com.hft.analytics.TestcontainersConfiguration;
import com.hft.analytics.domain.PoolSnapshotRepository;

import static org.assertj.core.api.Assertions.assertThat;

@Import(TestcontainersConfiguration.class)
@SpringBootTest(properties = { "hft.bot.listener-enabled=true", "hft.bot.events-endpoint=tcp://127.0.0.1:55872" })
class BotEventListenerTests {

	@Autowired
	PoolSnapshotRepository snapshots;

	@Test
	void storesEventsPublishedByTheBot() throws InterruptedException {
		String event = """
				{"type":"pool_snapshot","ts_ms":1793300000000,"block":42,"pool":"0xzmq","pair":"WETH/USDC",
				 "fee":3000,"price":2348.6,"tick":-198700,"sqrt_price_x96":"1","liquidity":"1"}""";
		try (ZContext context = new ZContext(); ZMQ.Socket pub = context.createSocket(SocketType.PUB)) {
			pub.bind("tcp://127.0.0.1:55872");
			for (int i = 0; i < 100 && snapshots.findPairs().isEmpty(); i++) {
				pub.send(event);
				Thread.sleep(100);
			}
		}
		assertThat(snapshots.findAll()).anyMatch(s -> s.getPool().equals("0xzmq") && s.getBlock() == 42);
	}

}
