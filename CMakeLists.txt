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

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(
    cmarket
    src/main.cpp
)

target_include_directories(
    cmarket
    PRIVATE
    include
)