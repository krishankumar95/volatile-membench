/**
 * membench/alloc.h — Page-aligned memory allocation abstraction.
 *
 * Uses VirtualAlloc (Windows) or mmap (POSIX) for large page-aligned buffers
 * to ensure consistent benchmark behavior.
 */
#ifndef MEMBENCH_ALLOC_H
#define MEMBENCH_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocate `size` bytes of page-aligned memory.
 * Returns NULL on failure.
 */
void *membench_alloc(size_t size);

/**
 * Free memory previously allocated with membench_alloc.
 */
void membench_free(void *ptr, size_t size);

/**
 * Return the system page size in bytes.
 */
size_t membench_page_size(void);

#ifdef __cplusplus
}
#endif

#endif /* MEMBENCH_ALLOC_H */
