# ── GPU Detection ────────────────────────────────────────────────────────────
# Sets: MEMBENCH_HAS_CUDA, MEMBENCH_HAS_HIP

set(MEMBENCH_HAS_CUDA OFF)
set(MEMBENCH_HAS_HIP  OFF)

# ── CUDA ──
include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set(MEMBENCH_HAS_CUDA ON)
    message(STATUS "CUDA found: ${CMAKE_CUDA_COMPILER}")
    # Require compute capability 5.0+ (Maxwell and newer)
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
        set(CMAKE_CUDA_ARCHITECTURES "50;60;70;80;90" CACHE STRING "CUDA architectures")
    endif()
else()
    message(STATUS "CUDA not found – NVIDIA GPU benchmarks disabled")
endif()

# ── HIP (AMD ROCm) ──
find_package(hip QUIET CONFIG)
if(hip_FOUND)
    set(MEMBENCH_HAS_HIP ON)
    message(STATUS "HIP found: ${hip_VERSION}")
else()
    message(STATUS "HIP not found – AMD GPU benchmarks disabled")
endif()

if(NOT MEMBENCH_HAS_CUDA AND NOT MEMBENCH_HAS_HIP)
    message(STATUS "No GPU SDK found – GPU benchmarks will be skipped entirely")
endif()
