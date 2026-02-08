/**
 * gpu_cuda.cu — CUDA GPU memory benchmarks.
 *
 * Pointer-chase for latency, streaming for bandwidth.
 */
#include "membench/bench_gpu.h"

#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Error handling ───────────────────────────────────────────────────────── */

#define CUDA_CHECK(call) do { \
    cudaError_t err = (call); \
    if (err != cudaSuccess) { \
        fprintf(stderr, "CUDA error at %s:%d: %s\n", \
                __FILE__, __LINE__, cudaGetErrorString(err)); \
        return -1; \
    } \
} while(0)

/* ── Device info ──────────────────────────────────────────────────────────── */

int membench_gpu_get_info(int device_id, membench_gpu_info_t *info) {
    if (!info) return -1;

    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, device_id));

    memset(info, 0, sizeof(*info));
    snprintf(info->name, sizeof(info->name), "%s", prop.name);
    info->total_mem_bytes = prop.totalGlobalMem;
    info->memory_bus_width = prop.memoryBusWidth;

    /* memoryClockRate removed from cudaDeviceProp in CUDA 13+;
       query it via cudaDeviceGetAttribute instead. */
    {
        int clock_khz = 0;
        cudaDeviceGetAttribute(&clock_khz, cudaDevAttrMemoryClockRate, device_id);
        info->memory_clock_mhz = clock_khz / 1000;
    }

    /* Theoretical BW: clock(MHz) * bus_width(bits) * 2 (DDR) / 8 (bits→bytes) / 1024^3 (→GB) */
    double clock_ghz = info->memory_clock_mhz / 1000.0;
    info->theoretical_bw_gbps = clock_ghz * (info->memory_bus_width / 8.0) * 2.0;

    return 0;
}

/* ── Latency: GPU pointer-chase kernel ────────────────────────────────────── */

__global__ void pointer_chase_kernel(unsigned int *buf, unsigned int count,
                                     unsigned int iterations, unsigned int *out) {
    unsigned int idx = 0;
    for (unsigned int iter = 0; iter < iterations; iter++) {
        for (unsigned int i = 0; i < count; i++) {
            idx = buf[idx];
        }
    }
    /* Write final value to prevent optimization */
    *out = idx;
}

static void build_gpu_chase(unsigned int *host_buf, unsigned int count) {
    /* Fisher-Yates to create random Hamiltonian cycle */
    unsigned int *indices = (unsigned int *)malloc(count * sizeof(unsigned int));
    for (unsigned int i = 0; i < count; i++) indices[i] = i;

    srand(42);
    for (unsigned int i = count - 1; i > 0; i--) {
        unsigned int j = (unsigned int)(rand() % (i + 1));
        unsigned int tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }

    for (unsigned int i = 0; i < count - 1; i++) {
        host_buf[indices[i]] = indices[i + 1];
    }
    host_buf[indices[count - 1]] = indices[0];

    free(indices);
}

int membench_gpu_read_latency(int device_id, size_t buffer_size,
                              uint64_t iterations,
                              membench_gpu_latency_result_t *result) {
    if (!result || buffer_size == 0) return -1;

    CUDA_CHECK(cudaSetDevice(device_id));

    unsigned int count = (unsigned int)(buffer_size / sizeof(unsigned int));
    if (count < 2) return -1;

    /* Build chase on host */
    unsigned int *h_buf = (unsigned int *)malloc(count * sizeof(unsigned int));
    if (!h_buf) return -1;
    build_gpu_chase(h_buf, count);

    /* Allocate on device */
    unsigned int *d_buf = NULL, *d_out = NULL;
    CUDA_CHECK(cudaMalloc(&d_buf, count * sizeof(unsigned int)));
    CUDA_CHECK(cudaMalloc(&d_out, sizeof(unsigned int)));
    CUDA_CHECK(cudaMemcpy(d_buf, h_buf, count * sizeof(unsigned int),
                          cudaMemcpyHostToDevice));

    /* Warmup */
    pointer_chase_kernel<<<1, 1>>>(d_buf, count, 1, d_out);
    CUDA_CHECK(cudaDeviceSynchronize());

    /* Timed run */
    cudaEvent_t start, stop;
    CUDA_CHECK(cudaEventCreate(&start));
    CUDA_CHECK(cudaEventCreate(&stop));

    CUDA_CHECK(cudaEventRecord(start));
    pointer_chase_kernel<<<1, 1>>>(d_buf, count, (unsigned int)iterations, d_out);
    CUDA_CHECK(cudaEventRecord(stop));
    CUDA_CHECK(cudaEventSynchronize(stop));

    float ms = 0;
    CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));

    uint64_t total_accesses = (uint64_t)iterations * count;
    result->buffer_size = buffer_size;
    result->accesses = total_accesses;
    result->avg_latency_ns = (double)ms * 1e6 / (double)total_accesses;

    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    cudaFree(d_buf);
    cudaFree(d_out);
    free(h_buf);
    return 0;
}

/* ── Bandwidth: streaming read kernel ─────────────────────────────────────── */

__global__ void read_bw_kernel(const uint64_t *buf, size_t count,
                               uint64_t *scratch) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;
    uint64_t sum = 0;
    for (size_t i = idx; i < count; i += stride) {
        sum += buf[i];
    }
    scratch[idx] = sum;
}

int membench_gpu_read_bandwidth(int device_id, size_t buffer_size,
                                uint64_t iterations,
                                membench_gpu_bandwidth_result_t *result) {
    if (!result || buffer_size == 0) return -1;

    CUDA_CHECK(cudaSetDevice(device_id));

    size_t count = buffer_size / sizeof(uint64_t);
    if (count == 0) return -1;

    uint64_t *d_buf = NULL;
    CUDA_CHECK(cudaMalloc(&d_buf, count * sizeof(uint64_t)));
    CUDA_CHECK(cudaMemset(d_buf, 0x42, count * sizeof(uint64_t)));

    int threads = 256;
    int blocks = (int)((count + threads - 1) / threads);
    if (blocks > 65535) blocks = 65535;

    uint64_t *d_scratch = NULL;
    CUDA_CHECK(cudaMalloc(&d_scratch, (size_t)threads * blocks * sizeof(uint64_t)));

    /* Warmup */
    read_bw_kernel<<<blocks, threads>>>(d_buf, count, d_scratch);
    CUDA_CHECK(cudaDeviceSynchronize());

    /* Timed run */
    cudaEvent_t start, stop;
    CUDA_CHECK(cudaEventCreate(&start));
    CUDA_CHECK(cudaEventCreate(&stop));

    CUDA_CHECK(cudaEventRecord(start));
    for (uint64_t iter = 0; iter < iterations; iter++) {
        read_bw_kernel<<<blocks, threads>>>(d_buf, count, d_scratch);
    }
    CUDA_CHECK(cudaEventRecord(stop));
    CUDA_CHECK(cudaEventSynchronize(stop));

    float ms = 0;
    CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));

    uint64_t total_bytes = iterations * count * sizeof(uint64_t);
    double elapsed_s = (double)ms / 1000.0;
    result->buffer_size = buffer_size;
    result->bandwidth_gbps = ((double)total_bytes / (1024.0 * 1024.0 * 1024.0)) / elapsed_s;
    result->bytes_moved = total_bytes;

    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    cudaFree(d_buf);
    cudaFree(d_scratch);
    return 0;
}

/* ── Bandwidth: streaming write kernel ────────────────────────────────────── */

__global__ void write_bw_kernel(uint64_t *buf, size_t count, uint64_t val) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;
    for (size_t i = idx; i < count; i += stride) {
        buf[i] = val + i;
    }
}

int membench_gpu_write_bandwidth(int device_id, size_t buffer_size,
                                 uint64_t iterations,
                                 membench_gpu_bandwidth_result_t *result) {
    if (!result || buffer_size == 0) return -1;

    CUDA_CHECK(cudaSetDevice(device_id));

    size_t count = buffer_size / sizeof(uint64_t);
    if (count == 0) return -1;

    uint64_t *d_buf = NULL;
    CUDA_CHECK(cudaMalloc(&d_buf, count * sizeof(uint64_t)));

    int threads = 256;
    int blocks = (int)((count + threads - 1) / threads);
    if (blocks > 65535) blocks = 65535;

    /* Warmup */
    write_bw_kernel<<<blocks, threads>>>(d_buf, count, 0);
    CUDA_CHECK(cudaDeviceSynchronize());

    /* Timed run */
    cudaEvent_t start, stop;
    CUDA_CHECK(cudaEventCreate(&start));
    CUDA_CHECK(cudaEventCreate(&stop));

    CUDA_CHECK(cudaEventRecord(start));
    for (uint64_t iter = 0; iter < iterations; iter++) {
        write_bw_kernel<<<blocks, threads>>>(d_buf, count, (uint64_t)iter);
    }
    CUDA_CHECK(cudaEventRecord(stop));
    CUDA_CHECK(cudaEventSynchronize(stop));

    float ms = 0;
    CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));

    uint64_t total_bytes = iterations * count * sizeof(uint64_t);
    double elapsed_s = (double)ms / 1000.0;
    result->buffer_size = buffer_size;
    result->bandwidth_gbps = ((double)total_bytes / (1024.0 * 1024.0 * 1024.0)) / elapsed_s;
    result->bytes_moved = total_bytes;

    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    cudaFree(d_buf);
    return 0;
}
