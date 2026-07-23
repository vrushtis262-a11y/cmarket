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

include(FetchContent)
include(CTest)

find_package(Boost CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(
    cmarket_order_book
    src/order_book.cpp
)

target_include_directories(
    cmarket_order_book
    PUBLIC
    include
)

target_compile_options(
    cmarket_order_book
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(
    cmarket
    src/main.cpp
    src/http_client.cpp
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

target_compile_options(
    cmarket
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if(BUILD_TESTING)
    FetchContent_Declare(
        googletest
        URL
        https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
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

    target_compile_options(
        order_book_tests
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    include(GoogleTest)

    gtest_discover_tests(order_book_tests)
endif()