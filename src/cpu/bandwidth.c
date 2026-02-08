/**
 * bandwidth.c — Sequential read/write bandwidth benchmarks.
 *
 * Streams through a buffer sequentially to measure sustained memory bandwidth.
 * Uses volatile pointers to prevent the compiler from optimizing away accesses.
 */
#include "membench/bench_cpu.h"
#include "membench/alloc.h"
#include "membench/timer.h"
#include "membench/platform.h"

#include <stdint.h>
#include <string.h>

/* ── Sequential read bandwidth ────────────────────────────────────────────── */

int membench_cpu_read_bandwidth(size_t buffer_size, uint64_t iterations,
                                membench_bandwidth_result_t *result) {
    if (!result || buffer_size == 0) return -1;

    /* Work with 64-bit words for throughput */
    size_t count = buffer_size / sizeof(uint64_t);
    if (count == 0) return -1;

    uint64_t *buf = (uint64_t *)membench_alloc(count * sizeof(uint64_t));
    if (!buf) return -1;

    /* Initialize with non-zero pattern */
    for (size_t i = 0; i < count; i++) {
        buf[i] = (uint64_t)i;
    }

    /* Warmup pass */
    {
        volatile uint64_t sink = 0;
        for (size_t i = 0; i < count; i++) {
            sink += buf[i];
        }
        (void)sink;
    }

    uint64_t total_bytes = iterations * count * sizeof(uint64_t);
    volatile uint64_t sink = 0;

    uint64_t start = membench_timer_ns();

    for (uint64_t iter = 0; iter < iterations; iter++) {
        uint64_t local_sum = 0;
        for (size_t i = 0; i < count; i++) {
            local_sum += buf[i];
        }
        sink += local_sum; /* prevent dead-code elimination */
    }

    uint64_t end = membench_timer_ns();
    (void)sink;

    double elapsed_s = (double)(end - start) / 1e9;
    result->buffer_size = buffer_size;
    result->bandwidth_gbps = ((double)total_bytes / (1024.0 * 1024.0 * 1024.0)) / elapsed_s;
    result->bytes_moved = total_bytes;
    result->avg_latency_ns = (double)(end - start) / (double)(iterations * count);

    membench_free(buf, count * sizeof(uint64_t));
    return 0;
}

/* ── Sequential write bandwidth ───────────────────────────────────────────── */

int membench_cpu_write_bandwidth(size_t buffer_size, uint64_t iterations,
                                 membench_bandwidth_result_t *result) {
    if (!result || buffer_size == 0) return -1;

    size_t count = buffer_size / sizeof(uint64_t);
    if (count == 0) return -1;

    uint64_t *buf = (uint64_t *)membench_alloc(count * sizeof(uint64_t));
    if (!buf) return -1;

    /* Warmup pass */
    for (size_t i = 0; i < count; i++) {
        buf[i] = 0;
    }

    uint64_t total_bytes = iterations * count * sizeof(uint64_t);

    uint64_t start = membench_timer_ns();

    for (uint64_t iter = 0; iter < iterations; iter++) {
        for (size_t i = 0; i < count; i++) {
            buf[i] = (uint64_t)(iter + i);
        }
    }

    uint64_t end = membench_timer_ns();

    /* Read one value back to prevent compiler from removing writes entirely */
    volatile uint64_t check = buf[count / 2];
    (void)check;

    double elapsed_s = (double)(end - start) / 1e9;
    result->buffer_size = buffer_size;
    result->bandwidth_gbps = ((double)total_bytes / (1024.0 * 1024.0 * 1024.0)) / elapsed_s;
    result->bytes_moved = total_bytes;
    result->avg_latency_ns = (double)(end - start) / (double)(iterations * count);

    membench_free(buf, count * sizeof(uint64_t));
    return 0;
}
