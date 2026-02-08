/**
 * test_alloc.c — Verify page-aligned allocation.
 */
#include "membench/alloc.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    printf("Test: Allocator\n");

    size_t page_sz = membench_page_size();
    printf("  Page size: %zu\n", page_sz);
    if (page_sz == 0 || (page_sz & (page_sz - 1)) != 0) {
        fprintf(stderr, "FAIL: page size not a power of 2\n");
        return 1;
    }

    /* Allocate 1 MB */
    size_t alloc_size = 1024 * 1024;
    void *p = membench_alloc(alloc_size);
    if (!p) {
        fprintf(stderr, "FAIL: allocation returned NULL\n");
        return 1;
    }

    /* Check page alignment */
    if (((uintptr_t)p % page_sz) != 0) {
        fprintf(stderr, "FAIL: allocation not page-aligned\n");
        membench_free(p, alloc_size);
        return 1;
    }
    printf("  Allocated %zu bytes at %p (page-aligned)\n", alloc_size, p);

    /* Verify we can read/write the full buffer (already memset to 0 in alloc) */
    unsigned char *bp = (unsigned char *)p;
    for (size_t i = 0; i < alloc_size; i++) {
        if (bp[i] != 0) {
            fprintf(stderr, "FAIL: buffer not zeroed at offset %zu\n", i);
            membench_free(p, alloc_size);
            return 1;
        }
    }

    /* Write pattern and verify */
    memset(p, 0xAB, alloc_size);
    for (size_t i = 0; i < alloc_size; i++) {
        if (bp[i] != 0xAB) {
            fprintf(stderr, "FAIL: pattern mismatch at offset %zu\n", i);
            membench_free(p, alloc_size);
            return 1;
        }
    }

    membench_free(p, alloc_size);

    /* Allocate zero should return NULL */
    if (membench_alloc(0) != NULL) {
        fprintf(stderr, "FAIL: alloc(0) should return NULL\n");
        return 1;
    }

    printf("  PASS\n");
    return 0;
}
