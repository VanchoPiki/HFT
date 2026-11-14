package com.hft.analytics.domain;

import java.time.Instant;
import java.util.List;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

public interface PoolSnapshotRepository extends JpaRepository<PoolSnapshot, Long> {

	@Query(value = "select distinct on (pool) * from pool_snapshots order by pool, ts desc, id desc",
			nativeQuery = true)
	List<PoolSnapshot> findLatestPerPool();

	List<PoolSnapshot> findByPairAndTsAfterOrderByTsAsc(String pair, Instant after);

	@Query("select distinct s.pair from PoolSnapshot s order by s.pair")
	List<String> findPairs();

}
