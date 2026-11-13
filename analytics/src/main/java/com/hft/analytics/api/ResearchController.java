package com.hft.analytics.api;

import java.io.IOException;
import java.util.List;
import java.util.Map;

import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import tools.jackson.databind.node.ObjectNode;

import com.hft.analytics.backtest.BacktestRun;
import com.hft.analytics.backtest.BacktestService;
import com.hft.analytics.discovery.BotConfigExporter;
import com.hft.analytics.discovery.DiscoveredPool;
import com.hft.analytics.discovery.DiscoveredPoolRepository;
import com.hft.analytics.discovery.PoolDiscoveryService;
import com.hft.analytics.history.SwapHistory;

/** Research tools: pool discovery, bot config export, swap history and backtests. */
@RestController
@RequestMapping("/api")
public class ResearchController {

	private final PoolDiscoveryService discovery;

	private final DiscoveredPoolRepository discovered;

	private final BotConfigExporter exporter;

	private final SwapHistory history;

	private final BacktestService backtests;

	public ResearchController(PoolDiscoveryService discovery, DiscoveredPoolRepository discovered,
			BotConfigExporter exporter, SwapHistory history, BacktestService backtests) {
		this.discovery = discovery;
		this.discovered = discovered;
		this.exporter = exporter;
		this.history = history;
		this.backtests = backtests;
	}

	@PostMapping("/discovery/run")
	public List<DiscoveredPool> runDiscovery() throws IOException {
		return discovery.discover();
	}

	@GetMapping("/discovery")
	public Map<String, Object> discoveredPools() {
		List<DiscoveredPool> pools = discovered.findAllByOrderBySymbol0AscSymbol1AscFeeAsc();
		return Map.of("pools", pools, "tradablePairs", exporter.tradablePairs(pools));
	}

	/** A ready-to-use config for the C++ bot: save as core/config/generated.json. */
	@GetMapping("/bot-config")
	public ResponseEntity<ObjectNode> botConfig(@RequestParam(defaultValue = "0.00001") double minProfitWeth,
			@RequestParam(defaultValue = "5") double maxInputWeth) {
		return ResponseEntity.ok()
			.header(HttpHeaders.CONTENT_DISPOSITION, "attachment; filename=generated.json")
			.body(exporter.export(discovered.findAll(), minProfitWeth, maxInputWeth));
	}

	@PostMapping("/history/index")
	public SwapHistory.IndexResult indexHistory(@RequestParam(defaultValue = "60") int minutes) throws IOException {
		return history.indexRecent(Math.clamp(minutes, 1, 24 * 60));
	}

	@GetMapping("/history")
	public SwapHistory.Coverage historyCoverage() {
		return history.coverage();
	}

	@PostMapping("/backtest")
	public BacktestService.Report backtest(@RequestParam(required = false) Long fromBlock,
			@RequestParam(required = false) Long toBlock, @RequestParam(defaultValue = "0.00001") double minProfitWeth,
			@RequestParam(defaultValue = "350000") long gasUnits,
			@RequestParam(defaultValue = "0.01") double gasPriceGwei) {
		return backtests.run(fromBlock, toBlock, minProfitWeth, gasUnits, gasPriceGwei);
	}

	@GetMapping("/backtests")
	public List<BacktestRun> backtests() {
		return backtests.recentRuns();
	}

	@GetMapping("/backtests/{id}")
	public BacktestService.Report backtest(@PathVariable long id) {
		return backtests.report(id);
	}

	@ExceptionHandler(IllegalStateException.class)
	public ResponseEntity<Map<String, String>> precondition(IllegalStateException e) {
		return ResponseEntity.status(HttpStatus.CONFLICT).body(Map.of("error", e.getMessage()));
	}

	@ExceptionHandler(IOException.class)
	public ResponseEntity<Map<String, String>> chainError(IOException e) {
		return ResponseEntity.status(HttpStatus.BAD_GATEWAY).body(Map.of("error", "RPC: " + e.getMessage()));
	}

}
