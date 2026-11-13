package com.hft.analytics.backtest;

import java.util.List;

import org.springframework.data.domain.Limit;
import org.springframework.data.jpa.repository.JpaRepository;

public interface BacktestOpportunityRepository extends JpaRepository<BacktestOpportunity, Long> {

	List<BacktestOpportunity> findByRunIdOrderByProfitWethDesc(long runId, Limit limit);

}
