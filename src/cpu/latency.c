/**
 * latency.c — Read/write latency benchmarks via pointer-chase.
 *
 * The pointer-chase technique creates a linked list of pointers within
 * the buffer, where each pointer points to a random (but unique) location.
 * Following this chain defeats hardware prefetchers and measures true
 * random-access latency for the given buffer size.
 *
 * IMPORTANT: Each node in the chain is spaced one cache line apart (64 B)
 * so that every dereference guarantees a fresh cache-line fetch.  Without
 * this, multiple nodes share the same 64-byte line and the apparent
 * latency is diluted by ~8× (8 pointers per line).
 */
#include "membench/bench_cpu.h"
#include "membench/alloc.h"
#include "membench/timer.h"
#include "membench/platform.h"

#include <stdlib.h>
#include <string.h>

/* ── Constants ────────────────────────────────────────────────────────────── */

/**
 * Default cache line size (x86_64). On Apple Silicon this is 128 bytes;
 * membench_get_cache_line_size() detects at runtime on macOS.
 */
#define CACHE_LINE_BYTES_DEFAULT 64

#if defined(MEMBENCH_PLATFORM_MACOS)
#include <sys/sysctl.h>
#endif

static size_t membench_get_cache_line_size(void) {
#if defined(MEMBENCH_PLATFORM_MACOS)
    size_t line = 0, sz = sizeof(line);
    if (sysctlbyname("hw.cachelinesize", &line, &sz, NULL, 0) == 0 && line > 0)
        return line;
#endif
    return CACHE_LINE_BYTES_DEFAULT;
}

/** How many void-pointers fit in one cache line */
#define PTRS_PER_LINE_DEFAULT (CACHE_LINE_BYTES_DEFAULT / sizeof(void *))   /* 8 on 64-bit */

/* ── Pointer-chase setup (cache-line stride) ──────────────────────────────── */

/**
 * Build a random cyclic pointer-chase within buf. 
 * `node_count` nodes are each CACHE_LINE_BYTES apart.
 * Node i lives at buf[i * PTRS_PER_LINE].
 * The chain visits every node exactly once.
 */
static void build_pointer_chase_cl(void **buf, size_t node_count, size_t ptrs_per_line) {
    /* Fisher-Yates shuffle of node indices → random Hamiltonian cycle */
    size_t *idx = (size_t *)malloc(node_count * sizeof(size_t));
    if (!idx) return;

    for (size_t i = 0; i < node_count; i++) idx[i] = i;

    for (size_t i = node_count - 1; i > 0; i--) {
        size_t j = (size_t)rand() % (i + 1);
        size_t tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
    }

    for (size_t i = 0; i < node_count - 1; i++) {
        buf[idx[i] * ptrs_per_line] = (void *)&buf[idx[i + 1] * ptrs_per_line];
    }
    buf[idx[node_count - 1] * ptrs_per_line] = (void *)&buf[idx[0] * ptrs_per_line];

    free(idx);
}

/* ── Memory fence helpers ─────────────────────────────────────────────────── */

MEMBENCH_INLINE void memory_fence(void) {
#if defined(MEMBENCH_ARCH_X86_64) || defined(MEMBENCH_ARCH_X86)
    #ifdef _MSC_VER
        _ReadWriteBarrier();
    #else
        __asm__ __volatile__("mfence" ::: "memory");
    #endif
#elif defined(MEMBENCH_ARCH_ARM64)
    #ifdef _MSC_VER
        __dmb(_ARM64_BARRIER_SY);
    #else
        __asm__ __volatile__("dmb sy" ::: "memory");
    #endif
#else
    /* Generic C11 fallback — compiler fence only */
    __asm__ __volatile__("" ::: "memory");
#endif
}

/**
 * Force-read a pointer through volatile to prevent the compiler
 * from optimizing out the dereference chain.  The cast to
 * (void * volatile *) makes each load non-eliminable.
 */
MEMBENCH_INLINE void **chase_load(void **p) {
    return (void **)(*(void * volatile *)p);
}

/* ── Read latency (pointer-chase, cache-line stride) ──────────────────────── */

int membench_cpu_read_latency(size_t buffer_size, uint64_t iterations,
                              membench_latency_result_t *result) {
    size_t cl = membench_get_cache_line_size();
    size_t ptrs_per_line = cl / sizeof(void *);

    if (!result || buffer_size < cl) return -1;

    /* Number of cache-line-spaced nodes that fit in the buffer */
    size_t node_count = buffer_size / cl;
    if (node_count < 2) node_count = 2;

    /* Allocate the full array (node_count * ptrs_per_line pointers) */
    size_t alloc_elems = node_count * ptrs_per_line;
    void **buf = (void **)membench_alloc(alloc_elems * sizeof(void *));
    if (!buf) return -1;

    /* Zero-fill, then build the cache-line-stride chase */
    memset(buf, 0, alloc_elems * sizeof(void *));
    srand(42);
    build_pointer_chase_cl(buf, node_count, ptrs_per_line);

    /* Warmup: one full traversal */
    {
        void **p = &buf[0];
        for (size_t i = 0; i < node_count; i++) {
            p = chase_load(p);
        }
        memory_fence();
        (void)p;
    }

    /* Timed traversals */
    uint64_t total_accesses = iterations * node_count;
    void **p = &buf[0];

    memory_fence();
    uint64_t start = membench_timer_ns();

    for (uint64_t iter = 0; iter < iterations; iter++) {
        for (size_t i = 0; i < node_count; i++) {
            p = chase_load(p);
        }
    }

    memory_fence();
    uint64_t end = membench_timer_ns();

    /* Prevent dead-code elimination */
    volatile void *sink = p;
    (void)sink;

    result->buffer_size = buffer_size;
    result->accesses = total_accesses;
    result->avg_latency_ns = (double)(end - start) / (double)total_accesses;

    membench_free(buf, alloc_elems * sizeof(void *));
    return 0;
}

/* ── Write latency (dependent read-write chase, cache-line stride) ─────────── */

/**
 * True write latency requires a data dependency so the store buffer
 * cannot absorb the writes.  We follow the same pointer-chase as
 * the read benchmark, but at each node we perform a write (flip a bit
 * in a secondary word within the same cache line) THEN follow the
 * pointer.  The read-after-write dependency serialises every access.
 *
 * Node layout (cache-line stride, 64 bytes per node on 64-bit):
 *   word[0] = next pointer       (used for the chase)
 *   word[1] = scratch for writes (modified each visit)
 */
int membench_cpu_write_latency(size_t buffer_size, uint64_t iterations,
                               membench_latency_result_t *result) {
    size_t cl = membench_get_cache_line_size();
    size_t ptrs_per_line = cl / sizeof(void *);

    if (!result || buffer_size < cl) return -1;

    size_t node_count = buffer_size / cl;
    if (node_count < 2) node_count = 2;

    size_t alloc_elems = node_count * ptrs_per_line;
    void **buf = (void **)membench_alloc(alloc_elems * sizeof(void *));
    if (!buf) return -1;

    /* Build cache-line-stride chase */
    memset(buf, 0, alloc_elems * sizeof(void *));
    srand(42);
    build_pointer_chase_cl(buf, node_count, ptrs_per_line);

    /* Warmup */
    {
        void **p = &buf[0];
        for (size_t i = 0; i < node_count; i++) {
            p[1] = (void *)((uintptr_t)p[1] ^ 1);  /* write scratch word */
            p = chase_load(p);                        /* follow pointer */
        }
        memory_fence();
    }

    /* Timed traversals: read pointer -> write scratch -> follow -> repeat */
    uint64_t total_accesses = iterations * node_count;
    void **p = &buf[0];

    memory_fence();
    uint64_t start = membench_timer_ns();

    for (uint64_t iter = 0; iter < iterations; iter++) {
        for (size_t i = 0; i < node_count; i++) {
            /* Write to word[1] within this cache line (dependent on arriving here) */
            ((volatile uintptr_t *)p)[1] = iter + i;
            /* Follow the pointer in word[0] -- depends on the line being present */
            p = chase_load(p);
        }
    }

    memory_fence();
    uint64_t end = membench_timer_ns();

    volatile void *sink = p;
    (void)sink;

    result->buffer_size = buffer_size;
    result->accesses = total_accesses;
    result->avg_latency_ns = (double)(end - start) / (double)total_accesses;

    membench_free(buf, alloc_elems * sizeof(void *));
    return 0;
}
