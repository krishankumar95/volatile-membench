/**
 * gpu_stub.c — Stub GPU implementation when no GPU SDK is available.
 *
 * Returns error codes so the CLI can gracefully skip GPU tests.
 */
#include "membench/bench_gpu.h"
#include <stdio.h>
#include <string.h>

int membench_gpu_get_info(int device_id, membench_gpu_info_t *info) {
    (void)device_id;
    if (info) {
        memset(info, 0, sizeof(*info));
        snprintf(info->name, sizeof(info->name), "No GPU support compiled");
    }
    return -1;
}

int membench_gpu_read_latency(int device_id, size_t buffer_size,
                              uint64_t iterations,
                              membench_gpu_latency_result_t *result) {
    (void)device_id; (void)buffer_size; (void)iterations; (void)result;
    return -1;
}

int membench_gpu_read_bandwidth(int device_id, size_t buffer_size,
                                uint64_t iterations,
                                membench_gpu_bandwidth_result_t *result) {
    (void)device_id; (void)buffer_size; (void)iterations; (void)result;
    return -1;
}

int membench_gpu_write_bandwidth(int device_id, size_t buffer_size,
                                 uint64_t iterations,
                                 membench_gpu_bandwidth_result_t *result) {
    (void)device_id; (void)buffer_size; (void)iterations; (void)result;
    return -1;
}
