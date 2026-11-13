package com.hft.analytics.backtest;

import java.util.List;

import org.springframework.data.domain.Limit;
import org.springframework.data.jpa.repository.JpaRepository;

public interface BacktestRunRepository extends JpaRepository<BacktestRun, Long> {

	List<BacktestRun> findAllByOrderByIdDesc(Limit limit);

}
