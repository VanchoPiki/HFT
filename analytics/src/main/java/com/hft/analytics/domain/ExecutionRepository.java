package com.hft.analytics.domain;

import java.math.BigInteger;
import java.util.List;
import java.util.Optional;

import org.springframework.data.domain.Limit;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

public interface ExecutionRepository extends JpaRepository<Execution, Long> {

	Optional<Execution> findByTxHash(String txHash);

	List<Execution> findAllByOrderByTsDesc(Limit limit);

	long countByStatus(String status);

	long countBySuccess(boolean success);

	@Query("select coalesce(sum(e.realizedProfitWeth), 0) from Execution e where e.success = true")
	double totalRealizedProfitWeth();

	@Query("select coalesce(sum(e.gasUsed * e.effectiveGasPrice), 0) from Execution e where e.gasUsed is not null")
	BigInteger totalGasWei();

	List<Execution> findAllByMinedAtIsNotNullOrderByMinedAtAsc();

	@Query("select coalesce(avg(e.simulateUs), 0) from Execution e where e.simulateUs is not null")
	double averageSimulateUs();

	@Query("select coalesce(avg(e.signUs), 0) from Execution e where e.signUs is not null and e.signUs > 0")
	double averageSignUs();

	@Query("select coalesce(avg(e.sendUs), 0) from Execution e where e.sendUs is not null and e.sendUs > 0")
	double averageSendUs();

}
