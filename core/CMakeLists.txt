cmake_minimum_required(VERSION 3.25)

# Use vcpkg (manifest: vcpkg.json) when it is installed, otherwise fall back to
# system packages (Homebrew). Must run before project().
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(hft_core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HFT_BUILD_TESTS "Build unit tests" ON)
option(HFT_BUILD_BENCH "Build benchmarks" ON)

add_library(hft_warnings INTERFACE)
# Designated initializers that skip fields with default values are intentional; GCC's
# -Wextra would flag every one of them.
target_compile_options(hft_warnings INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow
                                      -Wno-missing-field-initializers>)

# Homebrew's OpenSSL is keg-only: point CMake at it unless told otherwise.
if(APPLE AND NOT DEFINED OPENSSL_ROOT_DIR AND EXISTS "/opt/homebrew/opt/openssl@3")
    set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
endif()

find_package(Boost 1.83 CONFIG REQUIRED)  # header-only: Multiprecision, Beast, Asio
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(cppzmq CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SECP256K1 REQUIRED IMPORTED_TARGET libsecp256k1)

# Beast/Asio translation units need ~1 GB of RAM each to compile: never build more than
# two at once (under memory pressure parallel builds produced broken objects).
set_property(GLOBAL APPEND PROPERTY JOB_POOLS heavy_compile=2)
add_library(hft_net STATIC
    src/net/http_client.cpp
    src/net/url.cpp
    src/net/ws_client.cpp)
target_include_directories(hft_net PUBLIC src)
target_link_libraries(hft_net
    PUBLIC Boost::headers OpenSSL::SSL OpenSSL::Crypto Threads::Threads
    PRIVATE hft_warnings)
set_property(TARGET hft_net PROPERTY JOB_POOL_COMPILE heavy_compile)

add_library(hft_core
    src/app/bot.cpp
    src/app/config.cpp
    src/app/setup.cpp
    src/dex/market.cpp
    src/dex/pool_loader.cpp
    src/dex/uniswap_abi.cpp
    src/dex/v3_events.cpp
    src/dex/v3_math.cpp
    src/dex/v3_pool.cpp
    src/eth/abi.cpp
    src/eth/abi_json.cpp
    src/eth/address.cpp
    src/eth/hex.cpp
    src/eth/keccak.cpp
    src/eth/rlp.cpp
    src/eth/signer.cpp
    src/eth/tx.cpp
    src/eth/u256.cpp
    src/exec/executor.cpp
    src/net/feed.cpp
    src/net/json_rpc.cpp
    src/net/rpc_types.cpp
    src/strategy/two_pool_arb.cpp
    src/telemetry/events.cpp
    src/telemetry/publisher.cpp
    src/util/log.cpp)
target_include_directories(hft_core PUBLIC src)
target_link_libraries(hft_core
    PUBLIC hft_net simdjson::simdjson cppzmq PkgConfig::SECP256K1
    PRIVATE hft_warnings)

add_executable(hft_bot src/main.cpp)
target_link_libraries(hft_bot PRIVATE hft_core hft_warnings)

add_executable(hft_verify tools/verify_quotes.cpp)
target_link_libraries(hft_verify PRIVATE hft_core hft_warnings)

if(HFT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HFT_BUILD_BENCH)
    add_subdirectory(bench)
endif()
