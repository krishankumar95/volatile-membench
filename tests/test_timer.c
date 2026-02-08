/**
 * test_timer.c — Verify timer subsystem works correctly.
 */
#include "membench/timer.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Test: Timer\n");

    if (membench_timer_init() != 0) {
        fprintf(stderr, "FAIL: timer init failed\n");
        return 1;
    }

    /* Resolution should be > 0 and < 1000 ns (reasonable for modern systems) */
    double res = membench_timer_resolution_ns();
    printf("  Resolution: %.4f ns\n", res);
    if (res <= 0 || res > 1000) {
        fprintf(stderr, "FAIL: suspicious timer resolution: %.4f ns\n", res);
        return 1;
    }

    /* Two timestamps should be monotonically increasing */
    uint64_t t1 = membench_timer_ns();
    /* Busy-wait a tiny bit */
    volatile int x = 0;
    for (int i = 0; i < 100000; i++) x += i;
    (void)x;
    uint64_t t2 = membench_timer_ns();

    printf("  t1: %llu ns\n", (unsigned long long)t1);
    printf("  t2: %llu ns\n", (unsigned long long)t2);
    printf("  delta: %llu ns\n", (unsigned long long)(t2 - t1));

    if (t2 <= t1) {
        fprintf(stderr, "FAIL: timer not monotonic (t2 <= t1)\n");
        return 1;
    }

    /* Delta should be positive and < 1 second for such a small loop */
    uint64_t delta = t2 - t1;
    if (delta > 1000000000ULL) {
        fprintf(stderr, "FAIL: delta too large (>1s), timer may be broken\n");
        return 1;
    }

    printf("  PASS\n");
    return 0;
}
