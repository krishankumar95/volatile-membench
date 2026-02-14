/**
 * cache_detect.c — Automatic cache hierarchy detection.
 *
 * Sweeps pointer-chase latency across a range of buffer sizes.
 * Cache level boundaries appear as sharp increases in latency
 * when the working set exceeds the cache size.
 *
 * Detection uses a peak-finding algorithm on the derivative:
 *   1. Smooth the log-latency curve with a heavy median filter (R=3).
 *   2. Compute the derivative d(log_lat)/d(log_size) with W=2.
 *   3. Smooth the derivative again (median filter R=2).
 *   4. Find peaks in the smoothed derivative — these mark transitions.
 *   5. For each transition, determine the lower and upper plateau
 *      latency levels.
 *   6. Find where latency crosses the geometric mean of the two plateau
 *      levels, refined with log-interpolation for sub-sample accuracy.
 *
 * This approach is more robust than plateau-end detection because:
 *   - Derivative peaks are positive signals, not absence-of-signal.
 *   - The geometric-mean crossing naturally adapts to each transition's
 *     magnitude, giving good estimates for both sharp (L1->L2) and
 *     gradual (L3->DRAM) transitions.
 *
 * The benchmark thread is pinned to a single core to avoid migration
 * noise, since L1/L2 caches are per-core and migration would create
 * inconsistent measurements.
 */
#include "membench/bench_cpu.h"
#include "membench/alloc.h"
#include "membench/timer.h"
#include "membench/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Thread affinity: pin benchmark to one core for stable measurements */
#if defined(MEMBENCH_PLATFORM_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(MEMBENCH_PLATFORM_LINUX)
    #define _GNU_SOURCE
    #include <sched.h>
#elif defined(MEMBENCH_PLATFORM_MACOS)
    #include <pthread.h>
    #include <sys/sysctl.h>
#endif

/* ── Test sizes: logarithmic sweep from 1 KB to 512 MB ───────────────────── */

#define MIN_SIZE_KB    1
#define MAX_SIZE_KB    (512 * 1024)  /* 512 MB */
#define STEPS_PER_OCTAVE 4           /* 4 points per doubling */

static size_t generate_sizes(size_t **out_sizes) {
    size_t count = 0;
    double sz = MIN_SIZE_KB;
    double factor = pow(2.0, 1.0 / STEPS_PER_OCTAVE);

    while (sz <= MAX_SIZE_KB) {
        count++;
        sz *= factor;
    }

    *out_sizes = (size_t *)malloc(count * sizeof(size_t));
    if (!*out_sizes) return 0;

    sz = MIN_SIZE_KB;
    size_t prev = 0;
    size_t actual = 0;
    for (size_t i = 0; i < count; i++) {
        size_t bytes = (size_t)(sz * 1024.0);  /* multiply BEFORE cast */
        sz *= factor;
        if (bytes == prev) continue;           /* skip duplicate sizes */
        prev = bytes;
        (*out_sizes)[actual++] = bytes;
    }
    return actual;
}

/**
 * Auto-iteration count: ensure enough total accesses so wall-clock time
 * is well above timer granularity. For cache detection we use cache-line
 * stride nodes, so count = buffer_size / cache_line_size.
 */
static size_t get_cache_line_size_cd(void) {
#if defined(MEMBENCH_PLATFORM_MACOS)
    size_t line = 0, sz = sizeof(line);
    if (sysctlbyname("hw.cachelinesize", &line, &sz, NULL, 0) == 0 && line > 0)
        return line;
#endif
    return 64;
}

static uint64_t auto_iterations(size_t buffer_size) {
    size_t cl = get_cache_line_size_cd();
    size_t nodes = buffer_size / cl;
    if (nodes == 0) nodes = 1;
    /*
     * Target ~100 million node-visits per measurement.
     * Tiny buffers (L1) need many iterations; large buffers (DRAM) need few.
     */
    uint64_t iters = 100000000ULL / nodes;
    if (iters < 4) iters = 4;
    return iters;
}

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static int dbl_cmp(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

/** Median of a small array (copies & sorts). */
static double median_of(const double *arr, size_t n) {
    double *tmp = (double *)malloc(n * sizeof(double));
    if (!tmp) return arr[n / 2];
    for (size_t i = 0; i < n; i++) tmp[i] = arr[i];
    qsort(tmp, n, sizeof(double), dbl_cmp);
    double m = (n % 2) ? tmp[n / 2] : (tmp[n / 2 - 1] + tmp[n / 2]) / 2.0;
    free(tmp);
    return m;
}

/**
 * Find cache boundaries by locating transition peaks in the derivative
 * and then crossing the geometric mean of adjacent plateau levels.
 *
 * This replaces the older "plateau-end" approach which systematically
 * underestimated cache sizes because the forward-looking derivative
 * window detected transitions W samples too early.
 */
static void detect_boundaries(const size_t *sizes, const double *latencies,
                              size_t n, membench_cache_info_t *info) {
    info->l1_size_bytes = 0;
    info->l2_size_bytes = 0;
    info->l3_size_bytes = 0;
    if (n < 10) return;

    double *log_lat  = (double *)malloc(n * sizeof(double));
    double *log_size = (double *)malloc(n * sizeof(double));
    double *smooth   = (double *)malloc(n * sizeof(double));
    double *deriv    = (double *)malloc(n * sizeof(double));
    double *sderiv   = (double *)malloc(n * sizeof(double));
    if (!log_lat || !log_size || !smooth || !deriv || !sderiv) goto cleanup;

    /* Step 1: log-transform latency and size */
    for (size_t i = 0; i < n; i++) {
        log_lat[i]  = (latencies[i] > 0.0) ? log(latencies[i]) : 0.0;
        log_size[i] = log((double)sizes[i]);
    }

    /* Step 2: heavy median filter on log-latency (radius 3 = 7-point window).
     * This removes outliers while preserving step edges. */
    {
        const int R = 3;
        for (size_t i = 0; i < n; i++) {
            size_t lo = (i >= (size_t)R) ? i - R : 0;
            size_t hi = (i + R < n) ? i + R : n - 1;
            smooth[i] = median_of(&log_lat[lo], hi - lo + 1);
        }
    }

    /* Step 3: derivative d(smooth)/d(log_size) with window W=2.
     * W=2 is narrow enough to localise peaks well while the heavy
     * median smoothing above has already removed noise. */
    {
        const int W = 2;
        for (size_t i = 0; i < n; i++) {
            size_t lo = (i >= (size_t)W) ? i - W : 0;
            size_t hi = (i + W < n) ? i + W : n - 1;
            double denom = log_size[hi] - log_size[lo];
            if (hi == lo || denom < 1e-12) { deriv[i] = 0.0; continue; }
            deriv[i] = (smooth[hi] - smooth[lo]) / denom;
        }
    }

    /* Step 4: smooth derivative with median filter (radius 2) to clean
     * any remaining noise in the derivative signal. */
    {
        const int R = 2;
        for (size_t i = 0; i < n; i++) {
            size_t lo = (i >= (size_t)R) ? i - R : 0;
            size_t hi = (i + R < n) ? i + R : n - 1;
            sderiv[i] = median_of(&deriv[lo], hi - lo + 1);
        }
    }

    /* Step 5: find local maxima in smoothed derivative */
    #define MAX_PEAKS      20
    #define MIN_PEAK_HEIGHT 0.10  /* ignore tiny bumps */

    size_t peak_pos[MAX_PEAKS];
    double peak_mag[MAX_PEAKS];
    int npeaks = 0;

    for (size_t i = 1; i + 1 < n && npeaks < MAX_PEAKS; i++) {
        double v = sderiv[i];
        /* Filter NaN/Inf that could arise from degenerate data */
        if (!(v == v) || v > 1e30) continue;   /* isnan / isinf guard */
        if (v >= sderiv[i - 1] && v >= sderiv[i + 1]
            && v > MIN_PEAK_HEIGHT) {
            peak_pos[npeaks] = i;
            peak_mag[npeaks] = v;
            npeaks++;
        }
    }

    /* Step 6: merge nearby peaks (within 5 indices) — keep the tallest */
    for (int i = 0; i < npeaks; i++) {
        if (peak_mag[i] < 0) continue;
        for (int j = i + 1; j < npeaks; j++) {
            if (peak_mag[j] < 0) continue;
            if ((int)(peak_pos[j] - peak_pos[i]) <= 5) {
                if (peak_mag[j] > peak_mag[i]) {
                    peak_mag[i] = -1.0;  /* mark dead */
                    break;
                } else {
                    peak_mag[j] = -1.0;
                }
            }
        }
    }
    { /* compact */
        int m = 0;
        for (int i = 0; i < npeaks; i++)
            if (peak_mag[i] >= 0) {
                peak_pos[m] = peak_pos[i];
                peak_mag[m] = peak_mag[i];
                m++;
            }
        npeaks = m;
    }

    /* Step 7: select top 3 peaks by magnitude, then sort by index */
    for (int i = 0; i < npeaks - 1; i++)
        for (int j = i + 1; j < npeaks; j++)
            if (peak_mag[j] > peak_mag[i]) {
                double tv = peak_mag[i]; peak_mag[i] = peak_mag[j]; peak_mag[j] = tv;
                size_t ti = peak_pos[i]; peak_pos[i] = peak_pos[j]; peak_pos[j] = ti;
            }
    {
        int top = (npeaks > 3) ? 3 : npeaks;
        /* sort the top entries by index (ascending size) */
        for (int i = 0; i < top - 1; i++)
            for (int j = i + 1; j < top; j++)
                if (peak_pos[j] < peak_pos[i]) {
                    double tv = peak_mag[i]; peak_mag[i] = peak_mag[j]; peak_mag[j] = tv;
                    size_t ti = peak_pos[i]; peak_pos[i] = peak_pos[j]; peak_pos[j] = ti;
                }

        /* Step 8: for each transition, find the geometric-mean crossing.
         *
         * "Lower plateau" = median latency of flat samples before peak.
         * "Upper plateau" = median latency of flat samples after peak.
         * Threshold       = sqrt(lower * upper)   (geometric mean)
         *
         * Scan for the first raw-latency sample >= threshold, then
         * log-interpolate between that sample and its predecessor for
         * sub-sample accuracy. */
        for (int t = 0; t < top; t++) {
            size_t pk = peak_pos[t];

            /* Collect lower-plateau latencies (between previous peak and this one) */
            size_t lo_start = (t > 0) ? peak_pos[t - 1] + 1 : 0;
            double lo_vals[40];
            int lo_n = 0;
            for (size_t i = lo_start; i < pk && lo_n < 40; i++)
                if (sderiv[i] < MIN_PEAK_HEIGHT)
                    lo_vals[lo_n++] = latencies[i];
            if (lo_n < 1) continue;

            /* Collect upper-plateau latencies (between this peak and next) */
            size_t up_end = (t + 1 < top) ? peak_pos[t + 1] : n;
            double up_vals[40];
            int up_n = 0;
            for (size_t i = pk + 1; i < up_end && up_n < 40; i++)
                if (sderiv[i] < MIN_PEAK_HEIGHT)
                    up_vals[up_n++] = latencies[i];
            if (up_n < 1) continue;

            double lo_med = median_of(lo_vals, (size_t)lo_n);
            double up_med = median_of(up_vals, (size_t)up_n);
            double threshold = sqrt(lo_med * up_med);

            /* Scan forward for first sample >= threshold (in transition region) */
            size_t ci = pk;  /* fallback */
            for (size_t i = lo_start; i < up_end; i++) {
                if (latencies[i] >= threshold) { ci = i; break; }
            }

            /* Log-interpolate between ci-1 and ci for sub-sample precision */
            size_t boundary;
            if (ci > 0 && latencies[ci - 1] < threshold
                       && latencies[ci] >= threshold) {
                double f = (log(threshold) - log(latencies[ci - 1])) /
                           (log(latencies[ci]) - log(latencies[ci - 1]));
                double ls = log((double)sizes[ci - 1]) +
                            f * (log((double)sizes[ci]) -
                                 log((double)sizes[ci - 1]));
                boundary = (size_t)exp(ls);
            } else {
                boundary = sizes[ci];
            }

            switch (t) {
                case 0: info->l1_size_bytes = boundary; break;
                case 1: info->l2_size_bytes = boundary; break;
                case 2: info->l3_size_bytes = boundary; break;
            }
        }
    }

cleanup:
    free(log_lat);
    free(log_size);
    free(smooth);
    free(deriv);
    free(sderiv);
}

/* ── Public API ───────────────────────────────────────────────────────────── */

int membench_cpu_detect_cache(membench_cache_info_t *info) {
    if (!info) return -1;

    size_t *sizes = NULL;
    size_t num = generate_sizes(&sizes);
    if (num == 0 || !sizes) return -1;

    double *latencies = (double *)malloc(num * sizeof(double));
    if (!latencies) { free(sizes); return -1; }

    /* Pin to core 0 so all measurements use the same L1/L2 (per-core caches)
     * and we avoid migration-induced noise. */
#if defined(MEMBENCH_PLATFORM_WINDOWS)
    DWORD_PTR old_affinity = SetThreadAffinityMask(GetCurrentThread(), 1);
#elif defined(MEMBENCH_PLATFORM_LINUX)
    cpu_set_t old_mask, new_mask;
    CPU_ZERO(&new_mask);
    CPU_SET(0, &new_mask);
    sched_getaffinity(0, sizeof(old_mask), &old_mask);
    sched_setaffinity(0, sizeof(new_mask), &new_mask);
#elif defined(MEMBENCH_PLATFORM_MACOS)
    /* macOS has no POSIX thread affinity.  Request high-priority QoS class
     * to encourage scheduling on P-cores rather than E-cores. */
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif

    printf("  Sweeping %zu buffer sizes from %zu KB to %zu MB...\n",
           num, sizes[0] / 1024, sizes[num - 1] / (1024 * 1024));

    for (size_t i = 0; i < num; i++) {
        membench_latency_result_t lat = {0};
        uint64_t iters = auto_iterations(sizes[i]);

        int ret = membench_cpu_read_latency(sizes[i], iters, &lat);
        if (ret != 0) {
            latencies[i] = -1.0;
            continue;
        }
        latencies[i] = lat.avg_latency_ns;
    }

    /* Restore original thread affinity / QoS */
#if defined(MEMBENCH_PLATFORM_WINDOWS)
    if (old_affinity) SetThreadAffinityMask(GetCurrentThread(), old_affinity);
#elif defined(MEMBENCH_PLATFORM_LINUX)
    sched_setaffinity(0, sizeof(old_mask), &old_mask);
#elif defined(MEMBENCH_PLATFORM_MACOS)
    pthread_set_qos_class_self_np(QOS_CLASS_DEFAULT, 0);
#endif

    detect_boundaries(sizes, latencies, num, info);

    info->num_samples = num;
    info->sample_sizes = sizes;
    info->sample_latencies = latencies;

    return 0;
}

void membench_cache_info_free(membench_cache_info_t *info) {
    if (!info) return;
    free(info->sample_sizes);
    free(info->sample_latencies);
    info->sample_sizes = NULL;
    info->sample_latencies = NULL;
    info->num_samples = 0;
}
