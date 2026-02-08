/**
 * membench/platform.h — Platform and architecture detection at compile time.
 *
 * Defines:
 *   MEMBENCH_PLATFORM_WINDOWS / LINUX / MACOS
 *   MEMBENCH_ARCH_X86_64 / ARM64
 *   MEMBENCH_INLINE, MEMBENCH_NOINLINE, MEMBENCH_ALIGN(n)
 */
#ifndef MEMBENCH_PLATFORM_H
#define MEMBENCH_PLATFORM_H

/* ── Platform ─────────────────────────────────────────────────────────────── */
#if defined(_WIN32) || defined(_WIN64)
    #define MEMBENCH_PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #define MEMBENCH_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define MEMBENCH_PLATFORM_LINUX 1
#else
    #error "Unsupported platform"
#endif

/* ── Architecture ─────────────────────────────────────────────────────────── */
#if defined(__x86_64__) || defined(_M_X64)
    #define MEMBENCH_ARCH_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define MEMBENCH_ARCH_ARM64  1
#else
    #define MEMBENCH_ARCH_UNKNOWN 1
#endif

/* ── Compiler helpers ─────────────────────────────────────────────────────── */
#ifdef _MSC_VER
    #define MEMBENCH_INLINE       __forceinline
    #define MEMBENCH_NOINLINE     __declspec(noinline)
    #define MEMBENCH_ALIGN(n)     __declspec(align(n))
#else
    #define MEMBENCH_INLINE       static inline __attribute__((always_inline))
    #define MEMBENCH_NOINLINE     __attribute__((noinline))
    #define MEMBENCH_ALIGN(n)     __attribute__((aligned(n)))
#endif

#endif /* MEMBENCH_PLATFORM_H */
