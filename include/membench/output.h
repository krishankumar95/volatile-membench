/**
 * output.h — Output formatting helpers (table, CSV, JSON).
 */
#ifndef MEMBENCH_OUTPUT_H
#define MEMBENCH_OUTPUT_H

#include "membench/bench_cpu.h"
#include "membench/bench_gpu.h"
#include "membench/cli.h"

#ifdef __cplusplus
extern "C" {
#endif

void membench_print_latency(const membench_latency_result_t *r,
                            const char *label, membench_output_fmt_t fmt);

void membench_print_bandwidth(const membench_bandwidth_result_t *r,
                              const char *label, membench_output_fmt_t fmt);

void membench_print_cache_info(const membench_cache_info_t *info,
                               membench_output_fmt_t fmt);

void membench_print_gpu_latency(const membench_gpu_latency_result_t *r,
                                const char *label, membench_output_fmt_t fmt);

void membench_print_gpu_bandwidth(const membench_gpu_bandwidth_result_t *r,
                                  const char *label, membench_output_fmt_t fmt);

void membench_print_gpu_info(const membench_gpu_info_t *info,
                             membench_output_fmt_t fmt);

#ifdef __cplusplus
}
#endif

#endif /* MEMBENCH_OUTPUT_H */
