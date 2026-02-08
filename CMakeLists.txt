cmake_minimum_required(VERSION 3.20)
project(volatile-membench
    VERSION 0.1.0
    LANGUAGES C
    DESCRIPTION "Benchmarking tool for volatile memory performance"
)

# ── Options ──────────────────────────────────────────────────────────────────
option(MEMBENCH_ENABLE_GPU   "Build GPU (CUDA/HIP) benchmarks"  ON)
option(MEMBENCH_ENABLE_TESTS "Build tests"                      ON)

# ── Global compiler settings ────────────────────────────────────────────────
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler warnings (C/C++ only — avoid passing MSVC flags to nvcc)
if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/W4>)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Release with debug info by default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# ── Platform / architecture detection ───────────────────────────────────────
include(cmake/DetectPlatform.cmake)
include(cmake/DetectArch.cmake)

# ── GPU detection (optional) ────────────────────────────────────────────────
if(MEMBENCH_ENABLE_GPU)
    include(cmake/DetectGPU.cmake)
endif()

# ── Source targets ──────────────────────────────────────────────────────────
add_subdirectory(src)

# ── Tests ───────────────────────────────────────────────────────────────────
if(MEMBENCH_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
