/**
 * membench/cli.h — Command-line argument parsing.
 */
#ifndef MEMBENCH_CLI_H
#define MEMBENCH_CLI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MEMBENCH_TARGET_CPU = 0,
    MEMBENCH_TARGET_GPU,
    MEMBENCH_TARGET_ALL
} membench_target_t;

typedef enum {
    MEMBENCH_TEST_LATENCY     = (1 << 0),
    MEMBENCH_TEST_BANDWIDTH   = (1 << 1),
    MEMBENCH_TEST_CACHE_DETECT = (1 << 2),
    MEMBENCH_TEST_ALL         = 0x7
} membench_test_flags_t;

typedef enum {
    MEMBENCH_FMT_TABLE = 0,
    MEMBENCH_FMT_CSV,
    MEMBENCH_FMT_JSON
} membench_output_fmt_t;

typedef struct {
    membench_target_t     target;
    membench_test_flags_t tests;
    membench_output_fmt_t format;
    size_t                buffer_size;  /* 0 = use defaults */
    uint64_t              iterations;   /* 0 = auto */
    int                   gpu_device;   /* -1 = auto-detect first */
    bool                  verbose;
    bool                  show_help;
} membench_options_t;

/**
 * Parse argc/argv into options. Returns 0 on success, -1 on error.
 */
int membench_cli_parse(int argc, char **argv, membench_options_t *opts);

/**
 * Print usage information.
 */
void membench_cli_usage(const char *progname);

/**
 * Interactive mode: present arrow-key navigable menus for all options.
 * Only works when stdin is a TTY. Returns 0 on success, -1 on cancel/error.
 */
int membench_cli_interactive(membench_options_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* MEMBENCH_CLI_H */
