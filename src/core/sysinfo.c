/**
 * sysinfo.c — System information detection.
 */
#include "membench/sysinfo.h"
#include "membench/platform.h"

#include <stdio.h>
#include <string.h>

#if defined(MEMBENCH_PLATFORM_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(MEMBENCH_PLATFORM_LINUX)
    #include <unistd.h>
#elif defined(MEMBENCH_PLATFORM_MACOS)
    #include <sys/sysctl.h>
    #include <unistd.h>
#endif

/* ── x86 CPUID helper ────────────────────────────────────────────────────── */
#if defined(MEMBENCH_ARCH_X86_64) || defined(MEMBENCH_ARCH_X86)
    #ifdef _MSC_VER
        #include <intrin.h>
        static void cpuid(int info[4], int leaf) { __cpuid(info, leaf); }
    #else
        #include <cpuid.h>
        static void cpuid(int info[4], int leaf) {
            __cpuid(leaf, info[0], info[1], info[2], info[3]);
        }
    #endif
#endif

static void detect_cpu_model(char *buf, size_t len) {
#if defined(MEMBENCH_ARCH_X86_64) || defined(MEMBENCH_ARCH_X86)
    int regs[4];
    char brand[49] = {0};

    for (int i = 0; i < 3; i++) {
        cpuid(regs, 0x80000002 + i);
        memcpy(brand + i * 16,      &regs[0], 4);
        memcpy(brand + i * 16 + 4,  &regs[1], 4);
        memcpy(brand + i * 16 + 8,  &regs[2], 4);
        memcpy(brand + i * 16 + 12, &regs[3], 4);
    }
    /* Trim leading spaces */
    const char *p = brand;
    while (*p == ' ') p++;
    snprintf(buf, len, "%s", p);
#elif defined(MEMBENCH_PLATFORM_MACOS)
    size_t sz = len;
    if (sysctlbyname("machdep.cpu.brand_string", buf, &sz, NULL, 0) != 0) {
        snprintf(buf, len, "Unknown (ARM)");
    }
#else
    /* Linux ARM fallback: read /proc/cpuinfo */
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0 ||
                strncmp(line, "Model", 5) == 0) {
                char *colon = strchr(line, ':');
                if (colon) {
                    colon++;
                    while (*colon == ' ') colon++;
                    /* Remove trailing newline */
                    char *nl = strchr(colon, '\n');
                    if (nl) *nl = '\0';
                    snprintf(buf, len, "%s", colon);
                    fclose(f);
                    return;
                }
            }
        }
        fclose(f);
    }
    snprintf(buf, len, "Unknown");
#endif
}

int membench_sysinfo_get(membench_sysinfo_t *info) {
    if (!info) return -1;
    memset(info, 0, sizeof(*info));

    detect_cpu_model(info->cpu_model, sizeof(info->cpu_model));

#if defined(MEMBENCH_PLATFORM_WINDOWS)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    info->num_cores_logical = (int)si.dwNumberOfProcessors;
    /* Physical cores: approximate as logical / 2 for SMT, but we'll use
       GetLogicalProcessorInformation for accuracy */
    info->num_cores_physical = info->num_cores_logical; /* fallback */

    {
        DWORD len = 0;
        GetLogicalProcessorInformation(NULL, &len);
        if (len > 0) {
            SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buf = malloc(len);
            if (buf && GetLogicalProcessorInformation(buf, &len)) {
                int physical = 0;
                DWORD count = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
                for (DWORD i = 0; i < count; i++) {
                    if (buf[i].Relationship == RelationProcessorCore) {
                        physical++;
                    } else if (buf[i].Relationship == RelationCache) {
                        CACHE_DESCRIPTOR *cd = &buf[i].Cache;
                        if (cd->Level == 1 && cd->Type == CacheData && info->l1_data_cache == 0)
                            info->l1_data_cache = cd->Size;
                        else if (cd->Level == 2 && info->l2_cache == 0)
                            info->l2_cache = cd->Size;
                        else if (cd->Level == 3 && info->l3_cache == 0)
                            info->l3_cache = cd->Size;
                    }
                }
                info->num_cores_physical = physical;
            }
            free(buf);
        }
    }

    {
        MEMORYSTATUSEX mem;
        mem.dwLength = sizeof(mem);
        GlobalMemoryStatusEx(&mem);
        info->total_ram = (size_t)mem.ullTotalPhys;
    }

#elif defined(MEMBENCH_PLATFORM_LINUX)
    info->num_cores_logical = (int)sysconf(_SC_NPROCESSORS_ONLN);
    info->num_cores_physical = info->num_cores_logical; /* fallback */

    /* Try to get physical cores from /sys */
    {
        FILE *f = fopen("/sys/devices/system/cpu/cpu0/topology/core_siblings_list", "r");
        if (f) {
            fclose(f);
            /* Count unique core_ids — simplified: use nproc / 2 for SMT */
        }
    }

    /* Cache sizes from sysfs */
    {
        const char *paths[] = {
            "/sys/devices/system/cpu/cpu0/cache/index0/size", /* L1d */
            "/sys/devices/system/cpu/cpu0/cache/index2/size", /* L2  */
            "/sys/devices/system/cpu/cpu0/cache/index3/size", /* L3  */
        };
        size_t *targets[] = {&info->l1_data_cache, &info->l2_cache, &info->l3_cache};

        for (int i = 0; i < 3; i++) {
            FILE *f = fopen(paths[i], "r");
            if (f) {
                unsigned long val = 0;
                char suffix = 0;
                if (fscanf(f, "%lu%c", &val, &suffix) >= 1) {
                    if (suffix == 'K' || suffix == 'k') val *= 1024;
                    else if (suffix == 'M' || suffix == 'm') val *= 1024 * 1024;
                    *targets[i] = (size_t)val;
                }
                fclose(f);
            }
        }
    }

    {
        long pages = sysconf(_SC_PHYS_PAGES);
        long pagesz = sysconf(_SC_PAGESIZE);
        if (pages > 0 && pagesz > 0)
            info->total_ram = (size_t)pages * (size_t)pagesz;
    }

#elif defined(MEMBENCH_PLATFORM_MACOS)
    {
        int ncpu = 0;
        size_t sz = sizeof(ncpu);
        sysctlbyname("hw.logicalcpu", &ncpu, &sz, NULL, 0);
        info->num_cores_logical = ncpu;
        sysctlbyname("hw.physicalcpu", &ncpu, &sz, NULL, 0);
        info->num_cores_physical = ncpu;
    }
    {
        size_t val = 0, sz = sizeof(val);
        sysctlbyname("hw.l1dcachesize", &val, &sz, NULL, 0);
        info->l1_data_cache = val;
        sysctlbyname("hw.l2cachesize", &val, &sz, NULL, 0);
        info->l2_cache = val;
        sysctlbyname("hw.l3cachesize", &val, &sz, NULL, 0);
        info->l3_cache = val;
    }
    {
        uint64_t memsize = 0;
        size_t sz = sizeof(memsize);
        sysctlbyname("hw.memsize", &memsize, &sz, NULL, 0);
        info->total_ram = (size_t)memsize;
    }
#endif

    return 0;
}

static void format_size(char *buf, size_t len, size_t bytes) {
    if (bytes >= 1024ULL * 1024 * 1024)
        snprintf(buf, len, "%.1f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024ULL * 1024)
        snprintf(buf, len, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        snprintf(buf, len, "%.1f KB", (double)bytes / 1024.0);
    else
        snprintf(buf, len, "%zu B", bytes);
}

void membench_sysinfo_print(const membench_sysinfo_t *info) {
    char buf[64];

    printf("=== System Information ===\n");
    printf("  CPU:          %s\n", info->cpu_model);
    printf("  Cores:        %d physical, %d logical\n",
           info->num_cores_physical, info->num_cores_logical);

    if (info->l1_data_cache) {
        format_size(buf, sizeof(buf), info->l1_data_cache);
        printf("  L1 Data:      %s\n", buf);
    }
    if (info->l2_cache) {
        format_size(buf, sizeof(buf), info->l2_cache);
        printf("  L2:           %s\n", buf);
    }
    if (info->l3_cache) {
        format_size(buf, sizeof(buf), info->l3_cache);
        printf("  L3:           %s\n", buf);
    }
    format_size(buf, sizeof(buf), info->total_ram);
    printf("  Total RAM:    %s\n", buf);
}
