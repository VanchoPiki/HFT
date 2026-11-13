find_package(benchmark CONFIG REQUIRED)

add_executable(hft_bench
    eth/hex_bench.cpp
    strategy_bench.cpp)
target_link_libraries(hft_bench PRIVATE hft_core benchmark::benchmark_main)
