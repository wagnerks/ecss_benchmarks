cmake_minimum_required(VERSION 3.20)
project(ecss_benchmarks LANGUAGES CXX)

# -------------------------------------
# Options
# -------------------------------------
set(CMAKE_CXX_STANDARD 23)
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

# ecss
set(ECSS_BUILD_TESTS OFF CACHE BOOL "" FORCE) # disable ecss tests
set(ECSS_BUILD_STATIC OFF CACHE BOOL "" FORCE) # disable ecss as static lib

FetchContent_Declare(
    ecss
    GIT_REPOSITORY https://github.com/wagnerks/ecss.git
    GIT_TAG main
)
FetchContent_MakeAvailable(ecss)

# EnTT (header-only)
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.15.0
)
FetchContent_MakeAvailable(entt)

# Flecs options (only static lib enabled, so link flecs_static target)
set(FLECS_STATIC ON CACHE BOOL "Build flecs as a static library" FORCE)
set(FLECS_SHARED OFF CACHE BOOL "Disable shared flecs build" FORCE)
set(FLECS_CPP ON CACHE BOOL "Enable flecs C++ API" FORCE)


FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
    GIT_TAG v4.1.1
)
FetchContent_MakeAvailable(flecs)

# -------------------------------------
# Build type (must be before targets)
# -------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# -------------------------------------
# Benchmarks
# -------------------------------------
file(GLOB_RECURSE BENCH_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(ecss_benchmarks ${BENCH_SOURCES})

# Optimization flags (applied to ecss_benchmarks target)
if(MSVC)
    target_compile_options(ecss_benchmarks PRIVATE /O2 /DNDEBUG /fp:fast /arch:AVX2 /Qvec-report:2)
else()
    target_compile_options(ecss_benchmarks PRIVATE -O3 -DNDEBUG -march=native -ffast-math -fopt-info-vec)
endif()

target_compile_definitions(ecss_benchmarks PRIVATE
    FLECS_NO_LOG
    FLECS_NO_WARNINGS
    FLECS_NO_THREADS
    FLECS_NO_READER_WRITER_LOCKS
)

# Link libraries (static flecs target is flecs_static -> alias flecs::flecs_static)
# Previous link to flecs::flecs failed because shared lib disabled.
target_link_libraries(ecss_benchmarks
    PRIVATE
        benchmark::benchmark
        ecss
        EnTT::EnTT
        flecs::flecs_static
)

# Explicitly add flecs include dir
FetchContent_GetProperties(flecs)

target_include_directories(ecss_benchmarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_custom_target(run_benchmarks_json
    COMMAND ecss_benchmarks
        --benchmark_format=json
        --benchmark_out=${CMAKE_BINARY_DIR}/benchmarks.json
    DEPENDS ecss_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Run benchmarks and output JSON results"
)