/**
 * alloc.c — Page-aligned memory allocation implementation.
 */
#include "membench/alloc.h"
#include "membench/platform.h"

#include <string.h>

#if defined(MEMBENCH_PLATFORM_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif

void *membench_alloc(size_t size) {
    if (size == 0) return NULL;

#if defined(MEMBENCH_PLATFORM_WINDOWS)
    void *ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) ptr = NULL;
#endif

    /* Touch every page to ensure physical backing (avoid lazy allocation noise) */
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void membench_free(void *ptr, size_t size) {
    if (!ptr) return;

#if defined(MEMBENCH_PLATFORM_WINDOWS)
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}

size_t membench_page_size(void) {
#if defined(MEMBENCH_PLATFORM_WINDOWS)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
#else
    return (size_t)sysconf(_SC_PAGESIZE);
#endif
}
