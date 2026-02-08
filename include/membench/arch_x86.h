/**
 * arch_x86.h — x86-specific cache flush and fence intrinsics.
 *
 * Used to defeat hardware prefetching and ensure accurate latency measurements.
 * Only included on x86/x86_64 builds.
 */
#ifndef MEMBENCH_ARCH_X86_H
#define MEMBENCH_ARCH_X86_H

#include "membench/platform.h"

#if defined(MEMBENCH_ARCH_X86_64) || defined(MEMBENCH_ARCH_X86)

#ifdef _MSC_VER
    #include <intrin.h>
#else
    #include <x86intrin.h>
#endif

#include <stddef.h>

/**
 * Flush a cache line containing address `addr` from all cache levels.
 */
MEMBENCH_INLINE void membench_clflush(const void *addr) {
    _mm_clflush(addr);
}

/**
 * Full memory fence — ensures all prior loads and stores are globally visible.
 */
MEMBENCH_INLINE void membench_mfence(void) {
    _mm_mfence();
}

/**
 * Load fence — ensures all prior loads are completed.
 */
MEMBENCH_INLINE void membench_lfence(void) {
    _mm_lfence();
}

/**
 * Store fence — ensures all prior stores are completed.
 */
MEMBENCH_INLINE void membench_sfence(void) {
    _mm_sfence();
}

/**
 * Serializing instruction — waits for all prior instructions to complete.
 * More expensive than fences but guarantees pipeline drain.
 */
MEMBENCH_INLINE void membench_serialize(void) {
#ifdef _MSC_VER
    int info[4];
    __cpuid(info, 0);
#else
    __asm__ __volatile__("cpuid" ::: "eax", "ebx", "ecx", "edx", "memory");
#endif
}

/**
 * Read Time Stamp Counter (RDTSC) — cycle-level timing.
 */
MEMBENCH_INLINE uint64_t membench_rdtsc(void) {
#ifdef _MSC_VER
    return __rdtsc();
#else
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#endif
}

/**
 * Flush entire buffer from cache.
 */
static inline void membench_flush_buffer(const void *buf, size_t size) {
    const char *p = (const char *)buf;
    for (size_t i = 0; i < size; i += 64) { /* 64-byte cache line assumed */
        _mm_clflush(p + i);
    }
    _mm_mfence();
}

#endif /* MEMBENCH_ARCH_X86_64 || MEMBENCH_ARCH_X86 */

#endif /* MEMBENCH_ARCH_X86_H */
