/**
 * output.c — Output formatting for benchmark results.
 */
#include "membench/output.h"

#include <stdio.h>
#include <inttypes.h>

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static const char *fmt_size(size_t bytes, char *buf, size_t len) {
    if (bytes >= 1024ULL * 1024 * 1024)
        snprintf(buf, len, "%.1f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024ULL * 1024)
        snprintf(buf, len, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        snprintf(buf, len, "%.1f KB", (double)bytes / 1024.0);
    else
        snprintf(buf, len, "%zu B", bytes);
    return buf;
}

/* ── CPU latency ──────────────────────────────────────────────────────────── */

void membench_print_latency(const membench_latency_result_t *r,
                            const char *label, membench_output_fmt_t fmt) {
    char sb[64];
    fmt_size(r->buffer_size, sb, sizeof(sb));

    switch (fmt) {
    case MEMBENCH_FMT_TABLE:
        printf("  %-20s  size=%-10s  latency=%8.2f ns  (%" PRIu64 " accesses)\n",
               label, sb, r->avg_latency_ns, r->accesses);
        break;
    case MEMBENCH_FMT_CSV:
        printf("%s,%zu,%.4f,%" PRIu64 "\n",
               label, r->buffer_size, r->avg_latency_ns, r->accesses);
        break;
    case MEMBENCH_FMT_JSON:
        printf("{\"test\":\"%s\",\"buffer_size\":%zu,"
               "\"avg_latency_ns\":%.4f,\"accesses\":%" PRIu64 "}\n",
               label, r->buffer_size, r->avg_latency_ns, r->accesses);
        break;
    }
}

/* ── CPU bandwidth ────────────────────────────────────────────────────────── */

void membench_print_bandwidth(const membench_bandwidth_result_t *r,
                              const char *label, membench_output_fmt_t fmt) {
    char sb[64];
    fmt_size(r->buffer_size, sb, sizeof(sb));

    switch (fmt) {
    case MEMBENCH_FMT_TABLE:
        printf("  %-20s  size=%-10s  bandwidth=%8.2f GB/s\n",
               label, sb, r->bandwidth_gbps);
        break;
    case MEMBENCH_FMT_CSV:
        printf("%s,%zu,%.4f,%" PRIu64 "\n",
               label, r->buffer_size, r->bandwidth_gbps, r->bytes_moved);
        break;
    case MEMBENCH_FMT_JSON:
        printf("{\"test\":\"%s\",\"buffer_size\":%zu,"
               "\"bandwidth_gbps\":%.4f,\"bytes_moved\":%" PRIu64 "}\n",
               label, r->buffer_size, r->bandwidth_gbps, r->bytes_moved);
        break;
    }
}

/* ── Cache detection ──────────────────────────────────────────────────────── */

void membench_print_cache_info(const membench_cache_info_t *info,
                               membench_output_fmt_t fmt) {
    char sb[64];

    if (fmt == MEMBENCH_FMT_TABLE) {
        printf("  --- Cache Detection Results ---\n");
        if (info->l1_size_bytes) {
            fmt_size(info->l1_size_bytes, sb, sizeof(sb));
            printf("  Estimated L1 Data Cache:  %s\n", sb);
        }
        if (info->l2_size_bytes) {
            fmt_size(info->l2_size_bytes, sb, sizeof(sb));
            printf("  Estimated L2 Cache:       %s\n", sb);
        }
        if (info->l3_size_bytes) {
            fmt_size(info->l3_size_bytes, sb, sizeof(sb));
            printf("  Estimated L3 Cache:       %s\n", sb);
        }
        printf("\n  Latency curve (%zu samples):\n", info->num_samples);
        printf("  %-12s  %s\n", "Size", "Latency (ns)");
        printf("  %-12s  %s\n", "----", "------------");
        for (size_t i = 0; i < info->num_samples; i++) {
            if (info->sample_latencies[i] < 0) continue;
            fmt_size(info->sample_sizes[i], sb, sizeof(sb));
            printf("  %-12s  %8.2f\n", sb, info->sample_latencies[i]);
        }
    }
    else if (fmt == MEMBENCH_FMT_CSV) {
        printf("cache_level,size_bytes\n");
        printf("L1,%zu\n", info->l1_size_bytes);
        printf("L2,%zu\n", info->l2_size_bytes);
        printf("L3,%zu\n", info->l3_size_bytes);
        printf("\ncache_curve_size,latency_ns\n");
        for (size_t i = 0; i < info->num_samples; i++) {
            if (info->sample_latencies[i] < 0) continue;
            printf("%zu,%.4f\n", info->sample_sizes[i], info->sample_latencies[i]);
        }
    }
    else if (fmt == MEMBENCH_FMT_JSON) {
        printf("{\"cache\":{\"l1\":%zu,\"l2\":%zu,\"l3\":%zu},\"curve\":[",
               info->l1_size_bytes, info->l2_size_bytes, info->l3_size_bytes);
        int first = 1;
        for (size_t i = 0; i < info->num_samples; i++) {
            if (info->sample_latencies[i] < 0) continue;
            if (!first) printf(",");
            printf("{\"size\":%zu,\"ns\":%.4f}", info->sample_sizes[i],
                   info->sample_latencies[i]);
            first = 0;
        }
        printf("]}\n");
    }
}

/* ── GPU output ───────────────────────────────────────────────────────────── */

void membench_print_gpu_info(const membench_gpu_info_t *info,
                             membench_output_fmt_t fmt) {
    char sb[64];
    fmt_size(info->total_mem_bytes, sb, sizeof(sb));

    if (fmt == MEMBENCH_FMT_TABLE) {
        printf("  GPU:          %s\n", info->name);
        printf("  VRAM:         %s\n", sb);
        printf("  Bus Width:    %d bits\n", info->memory_bus_width);
        printf("  Memory Clock: %d MHz\n", info->memory_clock_mhz);
        printf("  Theoretical:  %.1f GB/s\n", info->theoretical_bw_gbps);
    } else if (fmt == MEMBENCH_FMT_JSON) {
        printf("{\"gpu\":\"%s\",\"vram\":%zu,\"bus_width\":%d,"
               "\"mem_clock_mhz\":%d,\"theoretical_bw_gbps\":%.1f}\n",
               info->name, info->total_mem_bytes, info->memory_bus_width,
               info->memory_clock_mhz, info->theoretical_bw_gbps);
    }
}

void membench_print_gpu_latency(const membench_gpu_latency_result_t *r,
                                const char *label, membench_output_fmt_t fmt) {
    char sb[64];
    fmt_size(r->buffer_size, sb, sizeof(sb));

    if (fmt == MEMBENCH_FMT_TABLE) {
        printf("  %-20s  size=%-10s  latency=%8.2f ns\n",
               label, sb, r->avg_latency_ns);
    } else if (fmt == MEMBENCH_FMT_CSV) {
        printf("%s,%zu,%.4f\n", label, r->buffer_size, r->avg_latency_ns);
    } else if (fmt == MEMBENCH_FMT_JSON) {
        printf("{\"test\":\"%s\",\"buffer_size\":%zu,\"avg_latency_ns\":%.4f}\n",
               label, r->buffer_size, r->avg_latency_ns);
    }
}

void membench_print_gpu_bandwidth(const membench_gpu_bandwidth_result_t *r,
                                  const char *label, membench_output_fmt_t fmt) {
    char sb[64];
    fmt_size(r->buffer_size, sb, sizeof(sb));

    if (fmt == MEMBENCH_FMT_TABLE) {
        printf("  %-20s  size=%-10s  bandwidth=%8.2f GB/s\n",
               label, sb, r->bandwidth_gbps);
    } else if (fmt == MEMBENCH_FMT_CSV) {
        printf("%s,%zu,%.4f\n", label, r->buffer_size, r->bandwidth_gbps);
    } else if (fmt == MEMBENCH_FMT_JSON) {
        printf("{\"test\":\"%s\",\"buffer_size\":%zu,\"bandwidth_gbps\":%.4f}\n",
               label, r->buffer_size, r->bandwidth_gbps);
    }
}
