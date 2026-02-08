/**
 * test_sysinfo.c — Verify system info detection.
 */
#include "membench/sysinfo.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("Test: System Info\n");

    membench_sysinfo_t info = {0};
    if (membench_sysinfo_get(&info) != 0) {
        fprintf(stderr, "FAIL: sysinfo_get failed\n");
        return 1;
    }

    /* CPU model should not be empty */
    if (strlen(info.cpu_model) == 0) {
        fprintf(stderr, "FAIL: CPU model string empty\n");
        return 1;
    }

    /* Should have at least 1 core */
    if (info.num_cores_logical < 1) {
        fprintf(stderr, "FAIL: logical cores = %d\n", info.num_cores_logical);
        return 1;
    }

    /* Should have some RAM */
    if (info.total_ram == 0) {
        fprintf(stderr, "FAIL: total RAM = 0\n");
        return 1;
    }

    /* Print it out */
    membench_sysinfo_print(&info);

    printf("  PASS\n");
    return 0;
}
