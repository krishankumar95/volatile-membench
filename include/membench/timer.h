/**
 * membench/timer.h — High-resolution timer abstraction.
 *
 * Provides nanosecond-resolution timing across Windows, Linux, and macOS.
 */
#ifndef MEMBENCH_TIMER_H
#define MEMBENCH_TIMER_H

#include "platform.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the timer subsystem. Must be called once before any timing calls.
 * Returns 0 on success, -1 on failure.
 */
int membench_timer_init(void);

/**
 * Return a monotonic timestamp in nanoseconds.
 * The absolute value is meaningless — only differences matter.
 */
uint64_t membench_timer_ns(void);

/**
 * Return the resolution (smallest measurable increment) in nanoseconds.
 */
double membench_timer_resolution_ns(void);

#ifdef __cplusplus
}
#endif

#endif /* MEMBENCH_TIMER_H */
