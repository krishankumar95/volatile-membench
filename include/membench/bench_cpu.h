/**
 * membench/bench_cpu.h — CPU memory benchmark routines.
 *
 * Latency (pointer-chase), bandwidth (streaming), and cache-level detection.
 */
#ifndef MEMBENCH_BENCH_CPU_H
#define MEMBENCH_BENCH_CPU_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Result structures ────────────────────────────────────────────────────── */

typedef struct {
    size_t   buffer_size;    /* bytes */
    double   avg_latency_ns; /* average per-access latency */
    uint64_t accesses;       /* total accesses performed */
} membench_latency_result_t;

typedef struct {
    size_t buffer_size;      /* bytes */
    double bandwidth_gbps;   /* GB/s */
    double avg_latency_ns;   /* per-element average (informational) */
    uint64_t bytes_moved;    /* total bytes read or written */
} membench_bandwidth_result_t;

typedef struct {
    size_t l1_size_bytes;    /* 0 if not detected */
    size_t l2_size_bytes;
    size_t l3_size_bytes;
    size_t num_samples;      /* Number of (size, latency) points */
    size_t   *sample_sizes;  /* Array of tested buffer sizes */
    double   *sample_latencies; /* Corresponding latencies in ns */
} membench_cache_info_t;

/* ── Benchmark functions ──────────────────────────────────────────────────── */

/**
 * Measure read latency via pointer-chase over `buffer_size` bytes.
 * `iterations` controls how many traversals to average over.
 */
int membench_cpu_read_latency(size_t buffer_size, uint64_t iterations,
                              membench_latency_result_t *result);

/**
 * Measure write latency over `buffer_size` bytes.
 */
int membench_cpu_write_latency(size_t buffer_size, uint64_t iterations,
                               membench_latency_result_t *result);

/**
 * Measure sequential read bandwidth over `buffer_size` bytes.
 */
int membench_cpu_read_bandwidth(size_t buffer_size, uint64_t iterations,
                                membench_bandwidth_result_t *result);

/**
 * Measure sequential write bandwidth over `buffer_size` bytes.
 */
int membench_cpu_write_bandwidth(size_t buffer_size, uint64_t iterations,
                                 membench_bandwidth_result_t *result);

/**
 * Auto-detect cache hierarchy by sweeping buffer sizes.
 * Caller must call membench_cache_info_free() on the result.
 */
int membench_cpu_detect_cache(membench_cache_info_t *info);

/**
 * Free arrays inside a cache_info_t.
 */
void membench_cache_info_free(membench_cache_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* MEMBENCH_BENCH_CPU_H */
