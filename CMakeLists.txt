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
    cmarket_order_book
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

    include(GoogleTest)

    gtest_discover_tests(order_book_tests)
endif()