package com.hft.analytics.ingest;

import java.math.BigInteger;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.context.annotation.Import;

import com.hft.analytics.TestcontainersConfiguration;
import com.hft.analytics.domain.Execution;
import com.hft.analytics.domain.ExecutionRepository;
import com.hft.analytics.domain.Opportunity;
import com.hft.analytics.domain.OpportunityRepository;
import com.hft.analytics.domain.PoolSnapshot;
import com.hft.analytics.domain.PoolSnapshotRepository;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

@Import(TestcontainersConfiguration.class)
@SpringBootTest(properties = "hft.bot.listener-enabled=false")
class EventIngestServiceTests {

	static final String SNAPSHOT = """
			{"type":"pool_snapshot","ts_ms":1793300000000,"block":519432121,
			 "pool":"0xc6962004f452be9203591991d15f6b388e09e8d0","pair":"WETH/USDC","fee":500,
			 "price":2342.57,"tick":-198736,
			 "sqrt_price_x96":"3833558700599279129460736","liquidity":"3883490737149040689"}""";

	static final String OPPORTUNITY = """
			{"type":"opportunity","ts_ms":1793300000000,"run":1793299000000,"id":12,"block":519432121,
			 "pair":"WETH/ARB","pool_a":"0xc6f780497a95e246eb9449f5e4770916dcd6396a","fee_a":500,
			 "pool_b":"0x89a4026e9ade251c67b7fb38054931a39936d9c5","fee_b":100,
			 "token_in":"0x912ce59144191c1204e64559fe8253a0e49e6548","symbol_in":"ARB",
			 "amount_in":"5713349708674793500","amount_out":"5730730000000000000",
			 "gross_profit":"17380291325206500","gas_cost":"96404000000000000","net_profit":"-79023708674793500",
			 "amount_in_human":5.7133,"net_profit_human":-0.079,"net_profit_weth":-0.000006,"compute_us":846}""";

	static final String EXECUTION = """
			{"type":"execution","ts_ms":1793300000100,"run":1793299000000,"opportunity_id":12,"status":"sent",
			 "tx_hash":"0x00000000000000000000000000000000000000000000000000000000000000aa","nonce":7,
			 "gas_limit":420000,"max_fee_per_gas":"20000000","error":"","simulate_us":3100,"sign_us":45,"send_us":2900}""";

	static final String TX_RESULT = """
			{"type":"tx_result","ts_ms":1793300000900,"run":1793299000000,"opportunity_id":12,
			 "tx_hash":"0x00000000000000000000000000000000000000000000000000000000000000aa","success":true,
			 "block":519432125,"gas_used":231000,"effective_gas_price":"10000000",
			 "realized_profit":"17000000000000000","realized_profit_weth":0.0000012}""";

	@Autowired
	EventIngestService ingest;

	@Autowired
	PoolSnapshotRepository snapshots;

	@Autowired
	OpportunityRepository opportunities;

	@Autowired
	ExecutionRepository executions;

	@Autowired
	BotStatus status;

	@BeforeEach
	void clean() {
		executions.deleteAll();
		opportunities.deleteAll();
		snapshots.deleteAll();
	}

	@Test
	void storesPoolSnapshotsWithFullPrecision() {
		ingest.ingest(SNAPSHOT);
		PoolSnapshot s = snapshots.findAll().getFirst();
		assertThat(s.getPair()).isEqualTo("WETH/USDC");
		assertThat(s.getTick()).isEqualTo(-198736);
		assertThat(s.getSqrtPriceX96()).isEqualTo(new BigInteger("3833558700599279129460736"));
		assertThat(s.getTs().toEpochMilli()).isEqualTo(1793300000000L);
	}

	@Test
	void storesEachOpportunityOnceAndKeepsNegativeAmounts() {
		ingest.ingest(OPPORTUNITY);
		ingest.ingest(OPPORTUNITY);
		assertThat(opportunities.count()).isEqualTo(1);
		Opportunity o = opportunities.findAll().getFirst();
		assertThat(o.getNetProfit()).isEqualTo(new BigInteger("-79023708674793500"));
		assertThat(o.getSymbolIn()).isEqualTo("ARB");
		assertThat(o.getComputeUs()).isEqualTo(846);
	}

	@Test
	void txResultCompletesTheExecution() {
		ingest.ingest(EXECUTION);
		ingest.ingest(TX_RESULT);
		assertThat(executions.count()).isEqualTo(1);
		Execution e = executions.findAll().getFirst();
		assertThat(e.getStatus()).isEqualTo("mined");
		assertThat(e.getSuccess()).isTrue();
		assertThat(e.getGasUsed()).isEqualTo(231000);
		assertThat(e.getError()).isNull();
		assertThat(executions.totalRealizedProfitWeth()).isEqualTo(0.0000012);
		assertThat(executions.totalGasWei()).isEqualTo(BigInteger.valueOf(231000L * 10000000L));
	}

	@Test
	void txResultWithoutExecutionStillCounts() {
		ingest.ingest(TX_RESULT);
		assertThat(executions.countBySuccess(true)).isEqualTo(1);
	}

	@Test
	void heartbeatUpdatesStatus() {
		ingest.ingest("""
				{"type":"heartbeat","ts_ms":1793300000000,"run":1,"chain_id":42161,"block":5,"pools":20,
				 "mode":"monitor","dropped_events":0}""");
		assertThat(status.lastHeartbeat().block()).isEqualTo(5);
		assertThat(status.lastHeartbeat().mode()).isEqualTo("monitor");
	}

	@Test
	void rejectsGarbageAndUnknownEvents() {
		assertThatThrownBy(() -> ingest.ingest("not json")).isInstanceOf(IllegalArgumentException.class);
		assertThatThrownBy(() -> ingest.ingest("{\"type\":\"nope\"}")).isInstanceOf(IllegalArgumentException.class);
		assertThatThrownBy(() -> ingest.ingest("{\"type\":\"pool_snapshot\"}"))
			.isInstanceOf(IllegalArgumentException.class);
	}

}
