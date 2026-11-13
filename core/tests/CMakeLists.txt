find_package(GTest CONFIG REQUIRED)

add_executable(hft_tests
    dex/market_test.cpp
    dex/v3_events_test.cpp
    dex/v3_math_test.cpp
    dex/v3_pool_test.cpp
    dex/v3_quote_fixture_test.cpp
    eth/abi_json_test.cpp
    eth/abi_test.cpp
    eth/address_test.cpp
    eth/hex_test.cpp
    eth/keccak_test.cpp
    eth/rlp_test.cpp
    eth/tx_test.cpp
    eth/u256_test.cpp
    net/rpc_types_test.cpp
    net/url_test.cpp
    strategy/two_pool_arb_test.cpp
    telemetry/telemetry_test.cpp)
target_link_libraries(hft_tests PRIVATE hft_core hft_warnings GTest::gtest_main)
target_compile_definitions(hft_tests PRIVATE
    HFT_ABI_DIR="${PROJECT_SOURCE_DIR}/abi"
    HFT_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

include(GoogleTest)
gtest_discover_tests(hft_tests)
