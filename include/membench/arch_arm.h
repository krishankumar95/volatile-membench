/**
 * arch_arm.h — ARM64-specific cache flush and fence intrinsics.
 *
 * Used on Apple Silicon, Raspberry Pi, and other AArch64 platforms.
 */
#ifndef MEMBENCH_ARCH_ARM_H
#define MEMBENCH_ARCH_ARM_H

#include "membench/platform.h"

#if defined(MEMBENCH_ARCH_ARM64)

#include <stddef.h>
#include <stdint.h>

/**
 * Data memory barrier — ensures all prior memory accesses complete.
 */
MEMBENCH_INLINE void membench_dmb(void) {
#ifdef _MSC_VER
    __dmb(_ARM64_BARRIER_SY);
#else
    __asm__ __volatile__("dmb sy" ::: "memory");
#endif
}

/**
 * Data synchronization barrier — stronger than DMB.
 */
MEMBENCH_INLINE void membench_dsb(void) {
#ifdef _MSC_VER
    __dsb(_ARM64_BARRIER_SY);
#else
    __asm__ __volatile__("dsb sy" ::: "memory");
#endif
}

/**
 * Instruction synchronization barrier.
 */
MEMBENCH_INLINE void membench_isb(void) {
#ifdef _MSC_VER
    __isb(_ARM64_BARRIER_SY);
#else
    __asm__ __volatile__("isb" ::: "memory");
#endif
}

/**
 * Clean and invalidate data cache line by virtual address.
 * Flushes the cache line containing `addr` to main memory.
 */
MEMBENCH_INLINE void membench_dc_civac(const void *addr) {
#ifdef _MSC_VER
    /* MSVC ARM64 doesn't expose DC CIVAC directly; use intrinsic workaround */
    (void)addr;
    __dsb(_ARM64_BARRIER_SY);
#else
    __asm__ __volatile__("dc civac, %0" :: "r"(addr) : "memory");
#endif
}

/**
 * Flush entire buffer from cache (ARM64).
 */
static inline void membench_flush_buffer(const void *buf, size_t size) {
    const char *p = (const char *)buf;
    /* ARM64 typically uses 64-byte cache lines (Apple M1 uses 128) */
    size_t line_size = 64;
    for (size_t i = 0; i < size; i += line_size) {
        membench_dc_civac(p + i);
    }
    membench_dsb();
    membench_isb();
}

/**
 * Read cycle counter (CNTVCT_EL0) — not always available in user mode.
 * Fallback: use membench_timer_ns() instead.
 */
MEMBENCH_INLINE uint64_t membench_read_cntvct(void) {
#ifdef _MSC_VER
    return 0; /* Not easily accessible on Windows ARM64 */
#else
    uint64_t val;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
    return val;
#endif
}

#endif /* MEMBENCH_ARCH_ARM64 */

#endif /* MEMBENCH_ARCH_ARM_H */
