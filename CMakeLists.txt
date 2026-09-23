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

option(
    CMARKET_ENABLE_THREAD_SANITIZER
    "Enable ThreadSanitizer"
    OFF
)

option(
    CMARKET_ENABLE_LIBFUZZER
    "Enable libFuzzer for the matching engine fuzz target when supported"
    OFF
)

if(
    CMARKET_ENABLE_SANITIZERS
    AND
    CMARKET_ENABLE_THREAD_SANITIZER
)
    message(
        FATAL_ERROR
        "ASan/UBSan and ThreadSanitizer must use separate build directories."
    )
endif()

include(FetchContent)
include(CTest)
include(CheckCXXSourceCompiles)

find_package(Boost CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Threads REQUIRED)

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

function(cmarket_enable_thread_sanitizer target_name)
    if(NOT CMARKET_ENABLE_THREAD_SANITIZER)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            ${target_name}
            PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
        )

        target_link_options(
            ${target_name}
            PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
        )
    else()
        message(
            WARNING
            "ThreadSanitizer is only configured for Clang and GCC."
        )
    endif()
endfunction()

function(cmarket_enable_instrumentation target_name)
    cmarket_enable_sanitizers(${target_name})
    cmarket_enable_thread_sanitizer(${target_name})
endfunction()

function(cmarket_enable_libfuzzer target_name)
    if(NOT CMARKET_ENABLE_LIBFUZZER)
        message(
            STATUS
            "libFuzzer disabled; ${target_name} will use standalone stdin mode."
        )
        return()
    endif()

    if(CMARKET_ENABLE_THREAD_SANITIZER)
        message(
            WARNING
            "libFuzzer is disabled in the ThreadSanitizer build."
        )
        return()
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(
            WARNING
            "libFuzzer requested, but the active compiler is not Clang. "
            "${target_name} will use standalone stdin mode."
        )
        return()
    endif()

    set(
        CMAKE_REQUIRED_FLAGS
        "-fsanitize=fuzzer,address,undefined"
    )

    set(
        CMAKE_REQUIRED_LINK_OPTIONS
        "-fsanitize=fuzzer,address,undefined"
    )

    check_cxx_source_compiles(
        "
        #include <cstddef>
        #include <cstdint>

        extern \"C\" int LLVMFuzzerTestOneInput(
            const std::uint8_t* data,
            std::size_t size
        )
        {
            return data != nullptr && size > 0
                ? static_cast<int>(data[0] & 0U)
                : 0;
        }
        "
        CMARKET_LIBFUZZER_AVAILABLE
    )

    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)

    if(NOT CMARKET_LIBFUZZER_AVAILABLE)
        message(
            WARNING
            "libFuzzer was requested but its runtime is unavailable. "
            "${target_name} will use standalone stdin mode instead."
        )
        return()
    endif()

    target_compile_definitions(
        ${target_name}
        PRIVATE
        CMARKET_LIBFUZZER
    )

    target_compile_options(
        ${target_name}
        PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
    )

    target_link_options(
        ${target_name}
        PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
    )

    message(
        STATUS
        "libFuzzer enabled for ${target_name}."
    )
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

target_link_libraries(
    cmarket_order_book
    PUBLIC
    Threads::Threads
)

cmarket_enable_warnings(cmarket_order_book)
cmarket_enable_instrumentation(cmarket_order_book)

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
cmarket_enable_instrumentation(cmarket_order_manager)

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
cmarket_enable_instrumentation(cmarket_matching_engine)

add_library(
    cmarket_concurrent_matching_engine
    src/concurrent_matching_engine.cpp
)

target_include_directories(
    cmarket_concurrent_matching_engine
    PUBLIC
    include
)

target_link_libraries(
    cmarket_concurrent_matching_engine
    PUBLIC
    cmarket_matching_engine
    Threads::Threads
)

cmarket_enable_warnings(
    cmarket_concurrent_matching_engine
)

cmarket_enable_instrumentation(
    cmarket_concurrent_matching_engine
)

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
cmarket_enable_instrumentation(cmarket)

add_executable(
    baseline_benchmark
    benchmarks/baseline_benchmark.cpp
)

target_link_libraries(
    baseline_benchmark
    PRIVATE
    cmarket_matching_engine
)

cmarket_enable_warnings(baseline_benchmark)
cmarket_enable_instrumentation(baseline_benchmark)

add_executable(
    latency_benchmark
    benchmarks/latency_benchmark.cpp
)

target_link_libraries(
    latency_benchmark
    PRIVATE
    cmarket_matching_engine
)

cmarket_enable_warnings(latency_benchmark)
cmarket_enable_instrumentation(latency_benchmark)

add_executable(
    throughput_benchmark
    benchmarks/throughput_benchmark.cpp
)

target_link_libraries(
    throughput_benchmark
    PRIVATE
    cmarket_matching_engine
)

cmarket_enable_warnings(throughput_benchmark)
cmarket_enable_instrumentation(throughput_benchmark)

add_executable(
    concurrent_benchmark
    benchmarks/concurrent_benchmark.cpp
)

target_link_libraries(
    concurrent_benchmark
    PRIVATE
    cmarket_concurrent_matching_engine
    Threads::Threads
)

cmarket_enable_warnings(concurrent_benchmark)
cmarket_enable_instrumentation(concurrent_benchmark)

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
    cmarket_enable_instrumentation(order_book_tests)

    add_executable(
        order_book_concurrency_tests
        tests/order_book_concurrency_tests.cpp
    )

    target_link_libraries(
        order_book_concurrency_tests
        PRIVATE
        cmarket_order_book
        Threads::Threads
        GTest::gtest_main
    )

    cmarket_enable_warnings(
        order_book_concurrency_tests
    )

    cmarket_enable_instrumentation(
        order_book_concurrency_tests
    )

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

    cmarket_enable_instrumentation(
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
    cmarket_enable_instrumentation(execution_tests)

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
    cmarket_enable_instrumentation(matching_engine_tests)

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
    cmarket_enable_instrumentation(matching_edge_cases_tests)

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
    cmarket_enable_instrumentation(trade_persistence_tests)

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
    cmarket_enable_instrumentation(trade_store_tests)

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
    cmarket_enable_instrumentation(order_manager_tests)

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
    cmarket_enable_instrumentation(exception_safety_tests)

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
    cmarket_enable_instrumentation(randomized_matching_tests)

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
    cmarket_enable_instrumentation(matching_engine_fuzz)
    cmarket_enable_libfuzzer(matching_engine_fuzz)

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
    cmarket_enable_instrumentation(limit_order_tests)

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
    cmarket_enable_instrumentation(order_id_generator_tests)

    add_executable(
        blocking_queue_tests
        tests/blocking_queue_tests.cpp
    )

    target_include_directories(
        blocking_queue_tests
        PRIVATE
        include
    )

    target_link_libraries(
        blocking_queue_tests
        PRIVATE
        Threads::Threads
        GTest::gtest_main
    )

    cmarket_enable_warnings(blocking_queue_tests)
    cmarket_enable_instrumentation(blocking_queue_tests)

    add_executable(
        concurrent_matching_engine_tests
        tests/concurrent_matching_engine_tests.cpp
    )

    target_link_libraries(
        concurrent_matching_engine_tests
        PRIVATE
        cmarket_concurrent_matching_engine
        Threads::Threads
        GTest::gtest_main
    )

    cmarket_enable_warnings(
        concurrent_matching_engine_tests
    )

    cmarket_enable_instrumentation(
        concurrent_matching_engine_tests
    )

    add_executable(
        concurrent_stress_tests
        tests/concurrent_stress_tests.cpp
    )

    target_link_libraries(
        concurrent_stress_tests
        PRIVATE
        cmarket_concurrent_matching_engine
        Threads::Threads
        GTest::gtest_main
    )

    cmarket_enable_warnings(
        concurrent_stress_tests
    )

    cmarket_enable_instrumentation(
        concurrent_stress_tests
    )

    add_executable(
        reconnect_backoff_tests
        tests/reconnect_backoff_tests.cpp
    )

    target_include_directories(
        reconnect_backoff_tests
        PRIVATE
        include
    )

    target_link_libraries(
        reconnect_backoff_tests
        PRIVATE
        GTest::gtest_main
    )

    cmarket_enable_warnings(
        reconnect_backoff_tests
    )

    cmarket_enable_instrumentation(
        reconnect_backoff_tests
    )

    add_executable(
        heartbeat_state_tests
        tests/heartbeat_state_tests.cpp
    )

    target_include_directories(
        heartbeat_state_tests
        PRIVATE
        include
    )

    cmarket_enable_warnings(
        heartbeat_state_tests
    )

    cmarket_enable_instrumentation(
        heartbeat_state_tests
    )

    add_test(
        NAME HeartbeatStateTest
        COMMAND heartbeat_state_tests
    )

    add_executable(
        websocket_message_parser_tests
        tests/websocket_message_parser_tests.cpp
    )

    target_include_directories(
        websocket_message_parser_tests
        PRIVATE
        include
    )

    target_link_libraries(
        websocket_message_parser_tests
        PRIVATE
        nlohmann_json::nlohmann_json
    )

    cmarket_enable_warnings(
        websocket_message_parser_tests
    )

    cmarket_enable_instrumentation(
        websocket_message_parser_tests
    )

    add_test(
        NAME WebSocketMessageParserTest
        COMMAND websocket_message_parser_tests
    )

    add_executable(
        websocket_payload_dispatcher_tests
        tests/websocket_payload_dispatcher_tests.cpp
    )

    target_include_directories(
        websocket_payload_dispatcher_tests
        PRIVATE
        include
    )

    target_link_libraries(
        websocket_payload_dispatcher_tests
        PRIVATE
        nlohmann_json::nlohmann_json
    )

    cmarket_enable_warnings(
        websocket_payload_dispatcher_tests
    )

    cmarket_enable_instrumentation(
        websocket_payload_dispatcher_tests
    )

    add_test(
        NAME WebSocketPayloadDispatcherTest
        COMMAND websocket_payload_dispatcher_tests
    )

    include(GoogleTest)

    gtest_discover_tests(order_book_tests)
    gtest_discover_tests(order_book_concurrency_tests)
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
    gtest_discover_tests(blocking_queue_tests)
    gtest_discover_tests(concurrent_matching_engine_tests)
    gtest_discover_tests(concurrent_stress_tests)
    gtest_discover_tests(reconnect_backoff_tests)
endif()