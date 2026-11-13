package com.hft.analytics.api;

import java.time.Instant;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.context.annotation.Import;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.setup.MockMvcBuilders;
import org.springframework.web.context.WebApplicationContext;

import com.hft.analytics.TestcontainersConfiguration;
import com.hft.analytics.ingest.EventIngestService;

import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.Matchers.hasItem;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@Import(TestcontainersConfiguration.class)
@SpringBootTest(properties = "hft.bot.listener-enabled=false")
class DashboardControllerTests {

	@Autowired
	WebApplicationContext context;

	@Autowired
	EventIngestService ingest;

	MockMvc mvc;

	@BeforeEach
	void setUp() {
		mvc = MockMvcBuilders.webAppContextSetup(context).build();
		ingest.ingest("""
				{"type":"pool_snapshot","ts_ms":%d,"block":1,"pool":"0xpool","pair":"WETH/USDC","fee":500,
				 "price":2342.5,"tick":-198736,"sqrt_price_x96":"1","liquidity":"1"}"""
			.formatted(Instant.now().toEpochMilli()));
	}

	@Test
	void pricesOfAPairForTheChart() throws Exception {
		mvc.perform(get("/api/prices").param("pair", "WETH/USDC").param("minutes", "5"))
			.andExpect(status().isOk())
			.andExpect(jsonPath("$[*].fee").value(hasItem(500)))
			.andExpect(jsonPath("$[*].price").value(hasItem(2342.5)));
	}

	@Test
	void pairsAndLatestPools() throws Exception {
		mvc.perform(get("/api/pairs")).andExpect(status().isOk()).andExpect(jsonPath("$").value(hasItem("WETH/USDC")));
		mvc.perform(get("/api/pools")).andExpect(status().isOk()).andExpect(jsonPath("$[*].pool").value(hasItem("0xpool")));
	}

	@Test
	void stats() throws Exception {
		mvc.perform(get("/api/stats"))
			.andExpect(status().isOk())
			.andExpect(jsonPath("$.poolSnapshots").value(greaterThanOrEqualTo(1)))
			.andExpect(jsonPath("$.gasSpentWei").exists());
	}

	@Test
	void tablesClampTheLimit() throws Exception {
		mvc.perform(get("/api/opportunities").param("limit", "100000")).andExpect(status().isOk());
		mvc.perform(get("/api/executions").param("limit", "0")).andExpect(status().isOk());
	}

}
