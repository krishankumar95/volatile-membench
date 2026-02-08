/**
 * cli.c — Command-line argument parsing.
 *
 * Zero-dependency CLI parser for membench options.
 */
#include "membench/cli.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void membench_cli_usage(const char *progname) {
    printf("Volatile MemBench — Volatile Memory Benchmarking Tool\n\n");
    printf("Usage: %s [options]\n\n", progname);
    printf("Options:\n");
    printf("  --target <cpu|gpu|all>   Target device (default: cpu)\n");
    printf("  --test <tests>           Comma-separated: latency,bandwidth,cache-detect,all\n");
    printf("                           (default: all)\n");
    printf("  --size <bytes>           Buffer size for single-size tests (e.g. 1M, 64K, 1G)\n");
    printf("                           (default: auto-selected per test)\n");
    printf("  --iterations <n>         Number of iterations (default: auto)\n");
    printf("  --gpu-device <id>        GPU device index (default: 0)\n");
    printf("  --format <table|csv|json> Output format (default: table)\n");
    printf("  --verbose                Enable verbose output\n");
    printf("  --help                   Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                              # Run all CPU tests\n", progname);
    printf("  %s --target gpu --test bandwidth # GPU bandwidth only\n", progname);
    printf("  %s --test latency --size 32K     # Latency at 32 KB\n", progname);
}

/**
 * Parse a size string like "1K", "32M", "1G" into bytes.
 */
static size_t parse_size(const char *str) {
    char *end = NULL;
    double val = strtod(str, &end);
    if (end && *end) {
        switch (*end) {
            case 'k': case 'K': val *= 1024; break;
            case 'm': case 'M': val *= 1024 * 1024; break;
            case 'g': case 'G': val *= 1024 * 1024 * 1024; break;
            default: break;
        }
    }
    return (size_t)val;
}

static int parse_tests(const char *str, membench_test_flags_t *flags) {
    *flags = 0;
    /* Make a mutable copy */
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", str);

    char *tok = strtok(buf, ",");
    while (tok) {
        if (strcmp(tok, "latency") == 0)
            *flags |= MEMBENCH_TEST_LATENCY;
        else if (strcmp(tok, "bandwidth") == 0)
            *flags |= MEMBENCH_TEST_BANDWIDTH;
        else if (strcmp(tok, "cache-detect") == 0)
            *flags |= MEMBENCH_TEST_CACHE_DETECT;
        else if (strcmp(tok, "all") == 0)
            *flags = MEMBENCH_TEST_ALL;
        else {
            fprintf(stderr, "Unknown test: '%s'\n", tok);
            return -1;
        }
        tok = strtok(NULL, ",");
    }
    return 0;
}

int membench_cli_parse(int argc, char **argv, membench_options_t *opts) {
    if (!opts) return -1;

    /* Defaults */
    opts->target = MEMBENCH_TARGET_CPU;
    opts->tests = MEMBENCH_TEST_ALL;
    opts->format = MEMBENCH_FMT_TABLE;
    opts->buffer_size = 0;
    opts->iterations = 0;
    opts->gpu_device = 0;
    opts->verbose = false;
    opts->show_help = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            opts->show_help = true;
            return 0;
        }
        else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "cpu") == 0)       opts->target = MEMBENCH_TARGET_CPU;
            else if (strcmp(argv[i], "gpu") == 0)   opts->target = MEMBENCH_TARGET_GPU;
            else if (strcmp(argv[i], "all") == 0)   opts->target = MEMBENCH_TARGET_ALL;
            else {
                fprintf(stderr, "Unknown target: '%s'\n", argv[i]);
                return -1;
            }
        }
        else if (strcmp(argv[i], "--test") == 0 && i + 1 < argc) {
            i++;
            if (parse_tests(argv[i], &opts->tests) != 0) return -1;
        }
        else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            i++;
            opts->buffer_size = parse_size(argv[i]);
            if (opts->buffer_size == 0) {
                fprintf(stderr, "Invalid size: '%s'\n", argv[i]);
                return -1;
            }
        }
        else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            i++;
            opts->iterations = (uint64_t)strtoull(argv[i], NULL, 10);
        }
        else if (strcmp(argv[i], "--gpu-device") == 0 && i + 1 < argc) {
            i++;
            opts->gpu_device = (int)strtol(argv[i], NULL, 10);
        }
        else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "table") == 0)      opts->format = MEMBENCH_FMT_TABLE;
            else if (strcmp(argv[i], "csv") == 0)    opts->format = MEMBENCH_FMT_CSV;
            else if (strcmp(argv[i], "json") == 0)   opts->format = MEMBENCH_FMT_JSON;
            else {
                fprintf(stderr, "Unknown format: '%s'\n", argv[i]);
                return -1;
            }
        }
        else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            opts->verbose = true;
        }
        else {
            fprintf(stderr, "Unknown option: '%s'\n", argv[i]);
            return -1;
        }
    }

    return 0;
}
