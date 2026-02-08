/**
 * membench/bench_gpu.h — GPU memory benchmark routines (CUDA / HIP).
 *
 * Latency and bandwidth for GPU global memory (VRAM).
 * Only available when compiled with CUDA or HIP support.
 */
#ifndef MEMBENCH_BENCH_GPU_H
#define MEMBENCH_BENCH_GPU_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── GPU device info ──────────────────────────────────────────────────────── */

typedef struct {
    char     name[256];
    size_t   total_mem_bytes;
    int      memory_bus_width;    /* bits */
    int      memory_clock_mhz;
    double   theoretical_bw_gbps; /* computed from bus width + clock */
} membench_gpu_info_t;

/* ── GPU result structures ────────────────────────────────────────────────── */

typedef struct {
    size_t   buffer_size;
    double   avg_latency_ns;
    uint64_t accesses;
} membench_gpu_latency_result_t;

typedef struct {
    size_t   buffer_size;
    double   bandwidth_gbps;
    uint64_t bytes_moved;
} membench_gpu_bandwidth_result_t;

/* ── Functions ────────────────────────────────────────────────────────────── */

/**
 * Query GPU info. Returns 0 on success.
 * device_id: GPU index (0 for first GPU).
 */
int membench_gpu_get_info(int device_id, membench_gpu_info_t *info);

/**
 * GPU global memory read latency (pointer-chase kernel).
 */
int membench_gpu_read_latency(int device_id, size_t buffer_size,
                              uint64_t iterations,
                              membench_gpu_latency_result_t *result);

/**
 * GPU global memory bandwidth (device-to-device copy kernel).
 */
int membench_gpu_read_bandwidth(int device_id, size_t buffer_size,
                                uint64_t iterations,
                                membench_gpu_bandwidth_result_t *result);

int membench_gpu_write_bandwidth(int device_id, size_t buffer_size,
                                 uint64_t iterations,
                                 membench_gpu_bandwidth_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* MEMBENCH_BENCH_GPU_H */
