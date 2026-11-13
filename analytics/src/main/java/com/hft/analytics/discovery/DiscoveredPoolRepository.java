package com.hft.analytics.discovery;

import java.util.List;

import org.springframework.data.jpa.repository.JpaRepository;

public interface DiscoveredPoolRepository extends JpaRepository<DiscoveredPool, String> {

	List<DiscoveredPool> findAllByOrderBySymbol0AscSymbol1AscFeeAsc();

}
