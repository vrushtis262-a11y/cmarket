cmake_minimum_required(VERSION 3.20)

project(
    CMarket
    VERSION 0.1.0
    DESCRIPTION "Low-Latency Prediction Market Engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(
    CMARKET_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

include(FetchContent)
include(CTest)

find_package(Boost CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

function(cmarket_enable_warnings target_name)
    target_compile_options(
        ${target_name}
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
    )
endfunction()

function(cmarket_enable_sanitizers target_name)
    if(NOT CMARKET_ENABLE_SANITIZERS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            ${target_name}
            PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )

        target_link_options(
            ${target_name}
            PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
    else()
        message(
            WARNING
            "Sanitizers are only configured for Clang and GCC."
        )
    endif()
endfunction()

add_library(
    cmarket_order_book
    src/order_book.cpp
)

target_include_directories(
    cmarket_order_book
    PUBLIC
    include
)

cmarket_enable_warnings(cmarket_order_book)
cmarket_enable_sanitizers(cmarket_order_book)

add_library(
    cmarket_order_manager
    src/order_manager.cpp
)

target_include_directories(
    cmarket_order_manager
    PUBLIC
    include
)

cmarket_enable_warnings(cmarket_order_manager)
cmarket_enable_sanitizers(cmarket_order_manager)

add_library(
    cmarket_matching_engine
    src/matching_engine.cpp
)

target_include_directories(
    cmarket_matching_engine
    PUBLIC
    include
)

target_link_libraries(
    cmarket_matching_engine
    PUBLIC
    cmarket_order_book
    cmarket_order_manager
)

cmarket_enable_warnings(cmarket_matching_engine)
cmarket_enable_sanitizers(cmarket_matching_engine)

add_executable(
    cmarket
    src/main.cpp
    src/http_client.cpp
    src/websocket_client.cpp
)

target_include_directories(
    cmarket
    PRIVATE
    include
)

target_compile_definitions(
    cmarket
    PRIVATE
    BOOST_ERROR_CODE_HEADER_ONLY
)

target_link_libraries(
    cmarket
    PRIVATE
    cmarket_matching_engine
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

cmarket_enable_warnings(cmarket)
cmarket_enable_sanitizers(cmarket)

if(BUILD_TESTING)
    FetchContent_Declare(
        googletest
        URL
        https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    set(
        gtest_force_shared_crt
        ON
        CACHE BOOL
        ""
        FORCE
    )

    FetchContent_MakeAvailable(googletest)

    add_executable(
        order_book_tests
        tests/order_book_tests.cpp
    )

    target_link_libraries(
        order_book_tests
        PRIVATE
        cmarket_order_book
        GTest::gtest_main
    )

    cmarket_enable_warnings(order_book_tests)
    cmarket_enable_sanitizers(order_book_tests)

    add_executable(
        order_book_exception_safety_tests
        tests/order_book_exception_safety_tests.cpp
    )

    target_link_libraries(
        order_book_exception_safety_tests
        PRIVATE
        cmarket_order_book
        GTest::gtest_main
    )

    cmarket_enable_warnings(
        order_book_exception_safety_tests
    )

    cmarket_enable_sanitizers(
        order_book_exception_safety_tests
    )

    add_executable(
        execution_tests
        tests/execution_tests.cpp
    )

    target_include_directories(
        execution_tests
        PRIVATE
        include
    )

    target_link_libraries(
        execution_tests
        PRIVATE
        GTest::gtest_main
    )

    cmarket_enable_warnings(execution_tests)
    cmarket_enable_sanitizers(execution_tests)

    add_executable(
        matching_engine_tests
        tests/matching_engine_tests.cpp
    )

    target_link_libraries(
        matching_engine_tests
        PRIVATE
        cmarket_matching_engine
        GTest::gtest_main
    )

    cmarket_enable_warnings(matching_engine_tests)
    cmarket_enable_sanitizers(matching_engine_tests)

    add_executable(
        matching_edge_cases_tests
        tests/matching_edge_cases_tests.cpp
    )

    target_link_libraries(
        matching_edge_cases_tests
        PRIVATE
        cmarket_matching_engine
        GTest::gtest_main
    )

    cmarket_enable_warnings(matching_edge_cases_tests)
    cmarket_enable_sanitizers(matching_edge_cases_tests)

    add_executable(
        trade_persistence_tests
        tests/trade_persistence_tests.cpp
    )

    target_link_libraries(
        trade_persistence_tests
        PRIVATE
        cmarket_matching_engine
        GTest::gtest_main
    )

    cmarket_enable_warnings(trade_persistence_tests)
    cmarket_enable_sanitizers(trade_persistence_tests)

    add_executable(
        trade_store_tests
        tests/trade_store_tests.cpp
    )

    target_include_directories(
        trade_store_tests
        PRIVATE
        include
    )

    target_link_libraries(
        trade_store_tests
        PRIVATE
        GTest::gtest_main
    )

    cmarket_enable_warnings(trade_store_tests)
    cmarket_enable_sanitizers(trade_store_tests)

    add_executable(
        order_manager_tests
        tests/order_manager_tests.cpp
    )

    target_link_libraries(
        order_manager_tests
        PRIVATE
        cmarket_order_manager
        GTest::gtest_main
    )

    cmarket_enable_warnings(order_manager_tests)
    cmarket_enable_sanitizers(order_manager_tests)

    add_executable(
        exception_safety_tests
        tests/exception_safety_tests.cpp
    )

    target_link_libraries(
        exception_safety_tests
        PRIVATE
        cmarket_matching_engine
        GTest::gtest_main
    )

    cmarket_enable_warnings(exception_safety_tests)
    cmarket_enable_sanitizers(exception_safety_tests)

    add_executable(
        randomized_matching_tests
        tests/randomized_matching_tests.cpp
    )

    target_link_libraries(
        randomized_matching_tests
        PRIVATE
        cmarket_matching_engine
        GTest::gtest_main
    )

    cmarket_enable_warnings(randomized_matching_tests)
    cmarket_enable_sanitizers(randomized_matching_tests)

    add_executable(
        matching_engine_fuzz
        tests/matching_engine_fuzz.cpp
    )

    target_link_libraries(
        matching_engine_fuzz
        PRIVATE
        cmarket_matching_engine
    )

    cmarket_enable_warnings(matching_engine_fuzz)
    cmarket_enable_sanitizers(matching_engine_fuzz)

    add_executable(
        limit_order_tests
        tests/limit_order_tests.cpp
    )

    target_include_directories(
        limit_order_tests
        PRIVATE
        include
    )

    target_link_libraries(
        limit_order_tests
        PRIVATE
        GTest::gtest_main
    )

    cmarket_enable_warnings(limit_order_tests)
    cmarket_enable_sanitizers(limit_order_tests)

    add_executable(
        order_id_generator_tests
        tests/order_id_generator_tests.cpp
    )

    target_include_directories(
        order_id_generator_tests
        PRIVATE
        include
    )

    target_link_libraries(
        order_id_generator_tests
        PRIVATE
        GTest::gtest_main
    )

    cmarket_enable_warnings(order_id_generator_tests)
    cmarket_enable_sanitizers(order_id_generator_tests)

    include(GoogleTest)

    gtest_discover_tests(order_book_tests)
    gtest_discover_tests(order_book_exception_safety_tests)
    gtest_discover_tests(execution_tests)
    gtest_discover_tests(matching_engine_tests)
    gtest_discover_tests(matching_edge_cases_tests)
    gtest_discover_tests(trade_persistence_tests)
    gtest_discover_tests(trade_store_tests)
    gtest_discover_tests(order_manager_tests)
    gtest_discover_tests(exception_safety_tests)
    gtest_discover_tests(randomized_matching_tests)
    gtest_discover_tests(limit_order_tests)
    gtest_discover_tests(order_id_generator_tests)
endif()