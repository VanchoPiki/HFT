package com.hft.analytics.domain;

import java.util.List;

import org.springframework.data.domain.Limit;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

public interface OpportunityRepository extends JpaRepository<Opportunity, Long> {

	boolean existsByRunIdAndOpportunityId(long runId, long opportunityId);

	List<Opportunity> findAllByOrderByTsDesc(Limit limit);

	@Query("select coalesce(avg(o.computeUs), 0) from Opportunity o")
	double averageComputeUs();

	@Query("select coalesce(sum(o.netProfitWeth), 0) from Opportunity o")
	double totalExpectedProfitWeth();

	@Query("select o.computeUs from Opportunity o")
	List<Long> findAllComputeUs();

}
