/**
 * timer.c — High-resolution timer implementation.
 */
#include "membench/timer.h"
#include "membench/platform.h"

#if defined(MEMBENCH_PLATFORM_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    static LARGE_INTEGER g_freq;
    static int g_timer_ready = 0;
#elif defined(MEMBENCH_PLATFORM_LINUX)
    #include <time.h>
#elif defined(MEMBENCH_PLATFORM_MACOS)
    #include <mach/mach_time.h>
    static mach_timebase_info_data_t g_timebase;
    static int g_timer_ready = 0;
#endif

int membench_timer_init(void) {
#if defined(MEMBENCH_PLATFORM_WINDOWS)
    if (!QueryPerformanceFrequency(&g_freq)) {
        return -1;
    }
    g_timer_ready = 1;
    return 0;
#elif defined(MEMBENCH_PLATFORM_MACOS)
    if (mach_timebase_info(&g_timebase) != KERN_SUCCESS) {
        return -1;
    }
    g_timer_ready = 1;
    return 0;
#else
    /* Linux: clock_gettime is always available */
    return 0;
#endif
}

uint64_t membench_timer_ns(void) {
#if defined(MEMBENCH_PLATFORM_WINDOWS)
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    /* Convert to nanoseconds: ticks * 1e9 / freq */
    return (uint64_t)((double)now.QuadPart * 1e9 / (double)g_freq.QuadPart);
#elif defined(MEMBENCH_PLATFORM_LINUX)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#elif defined(MEMBENCH_PLATFORM_MACOS)
    uint64_t ticks = mach_absolute_time();
    return ticks * g_timebase.numer / g_timebase.denom;
#else
    return 0;
#endif
}

double membench_timer_resolution_ns(void) {
#if defined(MEMBENCH_PLATFORM_WINDOWS)
    return 1e9 / (double)g_freq.QuadPart;
#elif defined(MEMBENCH_PLATFORM_LINUX)
    struct timespec res;
    clock_gettime(CLOCK_MONOTONIC, &res);
    return (double)res.tv_sec * 1e9 + (double)res.tv_nsec;
#elif defined(MEMBENCH_PLATFORM_MACOS)
    return (double)g_timebase.numer / (double)g_timebase.denom;
#else
    return 0.0;
#endif
}
