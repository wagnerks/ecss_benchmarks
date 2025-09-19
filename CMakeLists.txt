cmake_minimum_required(VERSION 3.20)
project(ecss_benchmarks LANGUAGES CXX)

# -------------------------------------
# Options
# -------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BENCHMARK_ENABLE_TESTING "Enable google benchmark tests" OFF)

# -------------------------------------
# Dependencies
# -------------------------------------
include(FetchContent)

# Google Benchmark
FetchContent_Declare(
    google_benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3 # stable release
)
FetchContent_MakeAvailable(google_benchmark)

# ecss (your repo as submodule or external)
set(ECSS_BUILD_TESTS OFF CACHE BOOL "" FORCE) # disable ecss tests
set(ECSS_BUILD_STATIC OFF CACHE BOOL "" FORCE) # disable ecss as static lib

FetchContent_Declare(
    ecss
    GIT_REPOSITORY https://github.com/wagnerks/ecss.git
    GIT_TAG main
)
FetchContent_MakeAvailable(ecss)

# EnTT (comparison)
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.0 # pick latest stable
)
FetchContent_MakeAvailable(entt)

# -------------------------------------
# Benchmarks
# -------------------------------------
file(GLOB_RECURSE BENCH_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(ecss_benchmarks ${BENCH_SOURCES})

target_link_libraries(ecss_benchmarks
    PRIVATE
        benchmark::benchmark
        ecss
        EnTT::EnTT
)

target_include_directories(ecss_benchmarks
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

