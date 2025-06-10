cmake_minimum_required(VERSION 3.16)
project(market_making_engine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Library ──────────────────────────────────────────────────────────────────
add_library(mme_core
    src/market_data_aggregator.cpp
    src/risk_manager.cpp
    src/quote_engine.cpp
    src/venue_router.cpp
    src/sim_execution_gateway.cpp
    src/market_maker_controller.cpp
    src/metrics.cpp
    src/backtest_runner.cpp
)

target_include_directories(mme_core PUBLIC include)

# ── Main executable ──────────────────────────────────────────────────────────
add_executable(market_maker src/main.cpp)
target_link_libraries(market_maker PRIVATE mme_core)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Unit tests
add_executable(unit_tests
    tests/unit/test_market_data_aggregator.cpp
    tests/unit/test_risk_manager.cpp
    tests/unit/test_quote_engine.cpp
    tests/unit/test_venue_router.cpp
    tests/unit/test_execution_gateway.cpp
)
target_link_libraries(unit_tests PRIVATE mme_core GTest::gtest_main)
target_include_directories(unit_tests PRIVATE include)

# Integration tests
add_executable(integration_tests
    tests/integration/test_end_to_end.cpp
)
target_link_libraries(integration_tests PRIVATE mme_core GTest::gtest_main)
target_include_directories(integration_tests PRIVATE include)

add_test(NAME UnitTests COMMAND unit_tests)
add_test(NAME IntegrationTests COMMAND integration_tests)
