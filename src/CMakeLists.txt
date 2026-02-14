# ── src/ CMakeLists.txt ──────────────────────────────────────────────────────

# ── Core library (timer, allocator, CLI, sysinfo, output) ───────────────────
add_library(membench_core STATIC
    core/timer.c
    core/alloc.c
    core/sysinfo.c
    core/cli.c
    core/cli_interactive.c
    core/output.c
)
target_include_directories(membench_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Platform libraries
if(MEMBENCH_PLATFORM STREQUAL "LINUX")
    if(RT_LIB)
        target_link_libraries(membench_core PUBLIC ${RT_LIB})
    endif()
    if(PTHREAD_LIB)
        target_link_libraries(membench_core PUBLIC ${PTHREAD_LIB})
    endif()
endif()

# Pass platform/arch as compile definitions
target_compile_definitions(membench_core PUBLIC
    MEMBENCH_PLATFORM_${MEMBENCH_PLATFORM}=1
)

# ── CPU benchmarks library ──────────────────────────────────────────────────
add_library(membench_cpu STATIC
    cpu/latency.c
    cpu/bandwidth.c
    cpu/cache_detect.c
)
target_link_libraries(membench_cpu PUBLIC membench_core)

# Math library needed for pow() in cache_detect.c
if(NOT MSVC)
    target_link_libraries(membench_cpu PUBLIC m)
endif()

# ── GPU benchmarks library (conditional) ────────────────────────────────────
if(MEMBENCH_HAS_CUDA)
    add_library(membench_gpu STATIC
        gpu/gpu_cuda.cu
    )
    target_link_libraries(membench_gpu PUBLIC membench_core)
    set_target_properties(membench_gpu PROPERTIES
        CUDA_SEPARABLE_COMPILATION OFF
        CUDA_RESOLVE_DEVICE_SYMBOLS ON
    )
    target_compile_definitions(membench_gpu PUBLIC MEMBENCH_HAS_CUDA=1)
elseif(MEMBENCH_HAS_HIP)
    add_library(membench_gpu STATIC
        gpu/gpu_cuda.cu  # HIP can compile CUDA sources with hipify
    )
    target_link_libraries(membench_gpu PUBLIC membench_core hip::device)
    target_compile_definitions(membench_gpu PUBLIC MEMBENCH_HAS_HIP=1)
else()
    # Stub: no GPU support
    add_library(membench_gpu STATIC
        gpu/gpu_stub.c
    )
    target_link_libraries(membench_gpu PUBLIC membench_core)
endif()

# ── Main executable ─────────────────────────────────────────────────────────
add_executable(membench main.c)
target_link_libraries(membench PRIVATE
    membench_core
    membench_cpu
    membench_gpu
)

# Install target
install(TARGETS membench RUNTIME DESTINATION bin)
