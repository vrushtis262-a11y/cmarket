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

find_package(Boost CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(
    cmarket
    src/main.cpp
    src/http_client.cpp
    src/order_book.cpp
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