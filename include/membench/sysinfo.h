/**
 * membench/sysinfo.h — System information detection.
 *
 * CPU model, cache sizes, RAM info, etc.
 */
#ifndef MEMBENCH_SYSINFO_H
#define MEMBENCH_SYSINFO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char   cpu_model[256];
    int    num_cores_physical;
    int    num_cores_logical;
    size_t l1_data_cache;    /* bytes, 0 if unknown */
    size_t l2_cache;
    size_t l3_cache;
    size_t total_ram;        /* bytes */
} membench_sysinfo_t;

/**
 * Populate system info struct. Returns 0 on success.
 */
int membench_sysinfo_get(membench_sysinfo_t *info);

/**
 * Print system info to stdout.
 */
void membench_sysinfo_print(const membench_sysinfo_t *info);

#ifdef __cplusplus
}
#endif

#endif /* MEMBENCH_SYSINFO_H */
