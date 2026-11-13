package com.hft.analytics;

import org.springframework.boot.SpringApplication;

// Runs the app against a throwaway Postgres container: ./gradlew bootTestRun
public class TestAnalyticsApplication {

	public static void main(String[] args) {
		SpringApplication.from(AnalyticsApplication::main).with(TestcontainersConfiguration.class).run(args);
	}

}
