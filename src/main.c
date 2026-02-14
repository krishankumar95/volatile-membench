/**
 * main.c — Volatile MemBench entry point.
 */
#include "membench/cli.h"
#include "membench/timer.h"
#include "membench/sysinfo.h"
#include "membench/bench_cpu.h"
#include "membench/bench_gpu.h"
#include "membench/output.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(MEMBENCH_PLATFORM_MACOS)
#include <sys/sysctl.h>
#endif

/* ── CPU frequency warmup ─────────────────────────────────────────────────── */

/**
 * Busy-loop for ~200 ms to force the CPU out of low-power idle states.
 * Without this, the first benchmark runs at a reduced clock frequency
 * (e.g. ~1 GHz on Apple M1 instead of 3.2 GHz), inflating results ~3×.
 */
static void cpu_freq_warmup(void) {
    uint64_t start = membench_timer_ns();
    volatile uint64_t sink = 0;
    while (membench_timer_ns() - start < 200000000ULL) { /* 200 ms */
        for (int i = 0; i < 10000; i++) {
            sink += (uint64_t)i * 37;
        }
    }
    (void)sink;
}

/* ── Default test parameters ──────────────────────────────────────────────── */

/* Buffer sizes for latency sweep (when no --size given) */
static const size_t DEFAULT_LATENCY_SIZES[] = {
    16 * 1024,          /*  16 KB (L1)  */
    32 * 1024,          /*  32 KB (L1)  */
    128 * 1024,         /* 128 KB (L2)  */
    512 * 1024,         /* 512 KB (L2)  */
    4 * 1024 * 1024,    /*   4 MB (L3)  */
    32 * 1024 * 1024,   /*  32 MB (L3)  */
    64 * 1024 * 1024,   /*  64 MB (DRAM) */
    256 * 1024 * 1024,  /* 256 MB (DRAM) */
};
#define NUM_DEFAULT_LATENCY_SIZES \
    (sizeof(DEFAULT_LATENCY_SIZES) / sizeof(DEFAULT_LATENCY_SIZES[0]))

static const size_t DEFAULT_BW_SIZES[] = {
    16 * 1024,                /*  16 KB (L1)  */
    32 * 1024,                /*  32 KB (L1)  */
    128 * 1024,               /* 128 KB (L2)  */
    512 * 1024,               /* 512 KB (L2)  */
    4 * 1024 * 1024,          /*   4 MB (L3)  */
    32 * 1024 * 1024,         /*  32 MB (L3)  */
    64 * 1024 * 1024,         /*  64 MB (DRAM) */
    256 * 1024 * 1024,        /* 256 MB (DRAM) */
    (size_t)1024 * 1024 * 1024,       /*   1 GB (DRAM) */
    (size_t)4 * 1024 * 1024 * 1024,   /*   4 GB (DRAM) */
    (size_t)8 * 1024 * 1024 * 1024,   /*   8 GB (DRAM) */
    (size_t)10 * 1024 * 1024 * 1024,  /*  10 GB (DRAM) */
};
#define NUM_DEFAULT_BW_SIZES \
    (sizeof(DEFAULT_BW_SIZES) / sizeof(DEFAULT_BW_SIZES[0]))

static const size_t DEFAULT_GPU_BW_SIZES[] = {
    1  * 1024 * 1024,                  /*   1 MB  */
    16 * 1024 * 1024,                  /*  16 MB  */
    256 * 1024 * 1024,                 /* 256 MB  */
    (size_t)1024 * 1024 * 1024,        /*   1 GB  */
    (size_t)4 * 1024 * 1024 * 1024,    /*   4 GB  */
    (size_t)8 * 1024 * 1024 * 1024,    /*   8 GB  */
    (size_t)10 * 1024 * 1024 * 1024,   /*  10 GB  */
};
#define NUM_DEFAULT_GPU_BW_SIZES \
    (sizeof(DEFAULT_GPU_BW_SIZES) / sizeof(DEFAULT_GPU_BW_SIZES[0]))
static const size_t DEFAULT_GPU_LAT_SIZES[] = {
    1  * 1024 * 1024,   /*   1 MB */
    4  * 1024 * 1024,   /*   4 MB */
    32 * 1024 * 1024,   /*  32 MB (VRAM) */
};
#define NUM_DEFAULT_GPU_LAT_SIZES \
    (sizeof(DEFAULT_GPU_LAT_SIZES) / sizeof(DEFAULT_GPU_LAT_SIZES[0]))

/* Auto-pick iterations: target ~200ms per measurement.
 * For latency tests, element count must match the pointer-chase node count
 * (buffer_size / cache_line_size), not buffer_size / sizeof(void*). */
static size_t get_cache_line_size_main(void) {
#if defined(MEMBENCH_PLATFORM_MACOS)
    size_t line = 0, sz = sizeof(line);
    if (sysctlbyname("hw.cachelinesize", &line, &sz, NULL, 0) == 0 && line > 0)
        return line;
#endif
    return 64;
}

static uint64_t auto_iter(size_t buffer_size, int is_latency) {
    size_t elems = is_latency
                   ? buffer_size / get_cache_line_size_main()
                   : buffer_size / sizeof(void *);
    if (elems == 0) elems = 1;
    uint64_t target = is_latency ? 20000000ULL : 5000000ULL;
    uint64_t iters = target / elems;
    if (iters < 2) iters = 2;
    return iters;
}

/* ── Run CPU benchmarks ───────────────────────────────────────────────────── */

static int run_cpu(const membench_options_t *opts) {
    int rc = 0;

    if (opts->tests & MEMBENCH_TEST_LATENCY) {
        printf("\n=== CPU Read Latency ===\n");
        if (opts->buffer_size) {
            membench_latency_result_t r = {0};
            uint64_t iters = opts->iterations ? opts->iterations
                             : auto_iter(opts->buffer_size, 1);
            rc = membench_cpu_read_latency(opts->buffer_size, iters, &r);
            if (rc == 0) membench_print_latency(&r, "Read Latency", opts->format);
        } else {
            for (size_t i = 0; i < NUM_DEFAULT_LATENCY_SIZES; i++) {
                membench_latency_result_t r = {0};
                uint64_t iters = auto_iter(DEFAULT_LATENCY_SIZES[i], 1);
                rc = membench_cpu_read_latency(DEFAULT_LATENCY_SIZES[i], iters, &r);
                if (rc == 0) membench_print_latency(&r, "Read Latency", opts->format);
            }
        }

        printf("\n=== CPU Write Latency ===\n");
        if (opts->buffer_size) {
            membench_latency_result_t r = {0};
            uint64_t iters = opts->iterations ? opts->iterations
                             : auto_iter(opts->buffer_size, 1);
            rc = membench_cpu_write_latency(opts->buffer_size, iters, &r);
            if (rc == 0) membench_print_latency(&r, "Write Latency", opts->format);
        } else {
            for (size_t i = 0; i < NUM_DEFAULT_LATENCY_SIZES; i++) {
                membench_latency_result_t r = {0};
                uint64_t iters = auto_iter(DEFAULT_LATENCY_SIZES[i], 1);
                rc = membench_cpu_write_latency(DEFAULT_LATENCY_SIZES[i], iters, &r);
                if (rc == 0) membench_print_latency(&r, "Write Latency", opts->format);
            }
        }
    }

    if (opts->tests & MEMBENCH_TEST_BANDWIDTH) {
        /* Determine RAM limit: skip sizes >= 50% of physical RAM to avoid
         * measuring swap performance instead of DRAM. */
        membench_sysinfo_t si = {0};
        membench_sysinfo_get(&si);
        size_t ram_limit = si.total_ram > 0 ? si.total_ram / 2 : (size_t)-1;

        printf("\n=== CPU Read Bandwidth ===\n");
        if (opts->buffer_size) {
            membench_bandwidth_result_t r = {0};
            uint64_t iters = opts->iterations ? opts->iterations
                             : auto_iter(opts->buffer_size, 0);
            rc = membench_cpu_read_bandwidth(opts->buffer_size, iters, &r);
            if (rc == 0) membench_print_bandwidth(&r, "Read BW", opts->format);
        } else {
            for (size_t i = 0; i < NUM_DEFAULT_BW_SIZES; i++) {
                if (DEFAULT_BW_SIZES[i] >= ram_limit) {
                    printf("  (skipping %.1f GB+ — exceeds 50%% of %.1f GB RAM)\n",
                           (double)DEFAULT_BW_SIZES[i] / (1024.0*1024.0*1024.0),
                           (double)si.total_ram / (1024.0*1024.0*1024.0));
                    break;
                }
                membench_bandwidth_result_t r = {0};
                uint64_t iters = auto_iter(DEFAULT_BW_SIZES[i], 0);
                rc = membench_cpu_read_bandwidth(DEFAULT_BW_SIZES[i], iters, &r);
                if (rc == 0) membench_print_bandwidth(&r, "Read BW", opts->format);
            }
        }

        printf("\n=== CPU Write Bandwidth ===\n");
        if (opts->buffer_size) {
            membench_bandwidth_result_t r = {0};
            uint64_t iters = opts->iterations ? opts->iterations
                             : auto_iter(opts->buffer_size, 0);
            rc = membench_cpu_write_bandwidth(opts->buffer_size, iters, &r);
            if (rc == 0) membench_print_bandwidth(&r, "Write BW", opts->format);
        } else {
            for (size_t i = 0; i < NUM_DEFAULT_BW_SIZES; i++) {
                if (DEFAULT_BW_SIZES[i] >= ram_limit) {
                    printf("  (skipping %.1f GB+ — exceeds 50%% of %.1f GB RAM)\n",
                           (double)DEFAULT_BW_SIZES[i] / (1024.0*1024.0*1024.0),
                           (double)si.total_ram / (1024.0*1024.0*1024.0));
                    break;
                }
                membench_bandwidth_result_t r = {0};
                uint64_t iters = auto_iter(DEFAULT_BW_SIZES[i], 0);
                rc = membench_cpu_write_bandwidth(DEFAULT_BW_SIZES[i], iters, &r);
                if (rc == 0) membench_print_bandwidth(&r, "Write BW", opts->format);
            }
        }
    }

    if (opts->tests & MEMBENCH_TEST_CACHE_DETECT) {
        printf("\n=== Cache Hierarchy Detection ===\n");
        membench_cache_info_t cinfo = {0};
        rc = membench_cpu_detect_cache(&cinfo);
        if (rc == 0) {
            membench_print_cache_info(&cinfo, opts->format);
            membench_cache_info_free(&cinfo);
        }
    }

    return rc;
}

/* ── Run GPU benchmarks ───────────────────────────────────────────────────── */

static int run_gpu(const membench_options_t *opts) {
    int dev = opts->gpu_device;
    membench_gpu_info_t ginfo = {0};

    printf("\n=== GPU Information ===\n");
    if (membench_gpu_get_info(dev, &ginfo) != 0) {
        fprintf(stderr, "Failed to get GPU info for device %d\n", dev);
        return -1;
    }
    membench_print_gpu_info(&ginfo, opts->format);

    size_t bw_size  = opts->buffer_size ? opts->buffer_size : DEFAULT_GPU_BW_SIZES[NUM_DEFAULT_GPU_BW_SIZES - 1];
    uint64_t iters  = opts->iterations ? opts->iterations : 10;

    if (opts->tests & MEMBENCH_TEST_LATENCY) {
        printf("\n=== GPU Read Latency ===\n");
        if (opts->buffer_size) {
            membench_gpu_latency_result_t r = {0};
            if (membench_gpu_read_latency(dev, opts->buffer_size, iters, &r) == 0) {
                membench_print_gpu_latency(&r, "GPU Read Latency", opts->format);
            }
        } else {
            for (size_t i = 0; i < NUM_DEFAULT_GPU_LAT_SIZES; i++) {
                membench_gpu_latency_result_t r = {0};
                if (membench_gpu_read_latency(dev, DEFAULT_GPU_LAT_SIZES[i], iters, &r) == 0) {
                    membench_print_gpu_latency(&r, "GPU Read Latency", opts->format);
                }
            }
        }
    }

    if (opts->tests & MEMBENCH_TEST_BANDWIDTH) {
        printf("\n=== GPU Read Bandwidth ===\n");
        if (opts->buffer_size) {
            membench_gpu_bandwidth_result_t r = {0};
            if (membench_gpu_read_bandwidth(dev, bw_size, iters, &r) == 0) {
                membench_print_gpu_bandwidth(&r, "GPU Read BW", opts->format);
            }
        } else {
            for (size_t i = 0; i < NUM_DEFAULT_GPU_BW_SIZES; i++) {
                membench_gpu_bandwidth_result_t r = {0};
                if (membench_gpu_read_bandwidth(dev, DEFAULT_GPU_BW_SIZES[i], iters, &r) == 0) {
                    membench_print_gpu_bandwidth(&r, "GPU Read BW", opts->format);
                }
            }
        }

        printf("\n=== GPU Write Bandwidth ===\n");
        if (opts->buffer_size) {
            membench_gpu_bandwidth_result_t r = {0};
            if (membench_gpu_write_bandwidth(dev, bw_size, iters, &r) == 0) {
                membench_print_gpu_bandwidth(&r, "GPU Write BW", opts->format);
            }
        } else {
            for (size_t i = 0; i < NUM_DEFAULT_GPU_BW_SIZES; i++) {
                membench_gpu_bandwidth_result_t r = {0};
                if (membench_gpu_write_bandwidth(dev, DEFAULT_GPU_BW_SIZES[i], iters, &r) == 0) {
                    membench_print_gpu_bandwidth(&r, "GPU Write BW", opts->format);
                }
            }
        }
    }

    return 0;
}

/* ── Entry point ──────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    membench_options_t opts = {0};

    if (argc == 1) {
        /* No arguments — try interactive mode */
        if (membench_cli_interactive(&opts) != 0)
            return 0; /* user cancelled */
    } else {
        if (membench_cli_parse(argc, argv, &opts) != 0) {
            membench_cli_usage(argv[0]);
            return 1;
        }
        if (opts.show_help) {
            membench_cli_usage(argv[0]);
            return 0;
        }
    }

    /* Initialize timer */
    if (membench_timer_init() != 0) {
        fprintf(stderr, "Failed to initialize high-resolution timer\n");
        return 1;
    }

    /* Warm up CPU to stabilize clock frequency before benchmarking */
    cpu_freq_warmup();

    /* Print system info */
    membench_sysinfo_t sinfo = {0};
    membench_sysinfo_get(&sinfo);
    membench_sysinfo_print(&sinfo);

    if (opts.verbose) {
        printf("  Timer resolution: %.2f ns\n", membench_timer_resolution_ns());
    }

    printf("\n");

    int rc = 0;

    if (opts.target == MEMBENCH_TARGET_CPU || opts.target == MEMBENCH_TARGET_ALL) {
        rc = run_cpu(&opts);
    }
    if (opts.target == MEMBENCH_TARGET_GPU || opts.target == MEMBENCH_TARGET_ALL) {
        if (run_gpu(&opts) != 0 && rc == 0) rc = -1;
    }

    printf("\nDone.\n");
    return rc;
}
