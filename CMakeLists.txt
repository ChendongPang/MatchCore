cmake_minimum_required(VERSION 3.20)

project(MatchEngine
    VERSION 0.1.0
    DESCRIPTION "A simple matching engine project"
    LANGUAGES CXX
)

# ------------------------------
# Global build config
# ------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MATCH_ENGINE_BUILD_DEMO  "Build demo executable" ON)
option(MATCH_ENGINE_BUILD_BENCH "Build benchmark executable" ON)

# Default build type for single-config generators
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ------------------------------
# Core library
# ------------------------------
add_library(match_engine_core STATIC
    src/book/order_book.cpp
    src/engine/matching_engine.cpp
)

add_library(MatchEngine::core ALIAS match_engine_core)

target_include_directories(match_engine_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(match_engine_core
    PUBLIC
        cxx_std_20
)

if (MSVC)
    target_compile_options(match_engine_core
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(match_engine_core
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# ------------------------------
# Demo executable
# ------------------------------
if (MATCH_ENGINE_BUILD_DEMO)
    add_executable(demo_matching_loop
        src/demo/demo_matching_loop.cpp
    )

    target_link_libraries(demo_matching_loop
        PRIVATE
            MatchEngine::core
    )
endif()

# ------------------------------
# Benchmark executable
# ------------------------------
if (MATCH_ENGINE_BUILD_BENCH)
    add_executable(benchmark_driver
        src/bench/benchmark_driver.cpp
    )

    target_link_libraries(benchmark_driver
        PRIVATE
            MatchEngine::core
    )
endif()