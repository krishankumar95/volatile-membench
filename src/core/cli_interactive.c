/**
 * cli_interactive.c — Interactive terminal UI for membench.
 *
 * Provides arrow-key navigable menus when run without arguments.
 * Uses ANSI escape codes and raw terminal mode (termios on Unix,
 * virtual terminal on Windows). Pure C, no external dependencies.
 */
#include "membench/cli.h"
#include "membench/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Platform-specific terminal control ──────────────────────────────────── */

#if defined(MEMBENCH_PLATFORM_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <conio.h>

    static HANDLE  g_hStdin;
    static HANDLE  g_hStdout;
    static DWORD   g_old_mode_in;
    static DWORD   g_old_mode_out;

    static void term_raw_enter(void) {
        g_hStdin  = GetStdHandle(STD_INPUT_HANDLE);
        g_hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleMode(g_hStdin, &g_old_mode_in);
        GetConsoleMode(g_hStdout, &g_old_mode_out);
        /* Enable virtual terminal processing for ANSI on Windows 10+ */
        SetConsoleMode(g_hStdin, ENABLE_VIRTUAL_TERMINAL_INPUT);
        SetConsoleMode(g_hStdout, g_old_mode_out | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    static void term_raw_leave(void) {
        SetConsoleMode(g_hStdin, g_old_mode_in);
        SetConsoleMode(g_hStdout, g_old_mode_out);
    }

    static int term_read_key(void) {
        int ch = _getch();
        if (ch == 0 || ch == 0xE0) {
            ch = _getch();
            switch (ch) {
                case 72: return 'A'; /* up */
                case 80: return 'B'; /* down */
                default: return 0;
            }
        }
        return ch;
    }

    static int term_is_tty(void) {
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode;
        return GetConsoleMode(h, &mode);
    }

#else /* POSIX (Linux, macOS) */
    #include <termios.h>
    #include <unistd.h>
    #include <sys/ioctl.h>

    static struct termios g_old_termios;

    static void term_raw_enter(void) {
        struct termios raw;
        tcgetattr(STDIN_FILENO, &g_old_termios);
        raw = g_old_termios;
        raw.c_lflag &= ~(ECHO | ICANON | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    static void term_raw_leave(void) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_old_termios);
    }

    static int term_read_key(void) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) != 1) return -1;
        if (c == 27) { /* ESC — could be arrow key sequence */
            unsigned char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return 27;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return 27;
            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': return 'A'; /* up */
                    case 'B': return 'B'; /* down */
                    case 'C': return 'C'; /* right */
                    case 'D': return 'D'; /* left */
                }
            }
            return 27;
        }
        return (int)c;
    }

    static int term_is_tty(void) {
        return isatty(STDIN_FILENO);
    }
#endif

/* ── ANSI helpers ────────────────────────────────────────────────────────── */

#define CLR_RESET    "\033[0m"
#define CLR_BOLD     "\033[1m"
#define CLR_DIM      "\033[2m"
#define CLR_CYAN     "\033[36m"
#define CLR_GREEN    "\033[32m"
#define CLR_YELLOW   "\033[33m"
#define CLR_MAGENTA  "\033[35m"

/* Move cursor up N lines and clear each */
static void ansi_clear_lines(int n) {
    for (int i = 0; i < n; i++) {
        printf("\033[A\033[2K");
    }
    printf("\r");
    fflush(stdout);
}

/* ── Widget: radio select (single choice) ────────────────────────────────── */

/**
 * Display a list of options with arrow-key navigation.
 * Returns the index of the selected option.
 *
 *  ◆ prompt
 *  │  ● selected option
 *  │  ○ other option
 */
static int widget_radio(const char *prompt, const char **options, int count,
                        int initial) {
    int sel = initial;
    int total_lines = count + 1; /* prompt + options */

    for (;;) {
        /* Draw */
        printf(CLR_CYAN CLR_BOLD "◆ %s" CLR_RESET "\n", prompt);
        for (int i = 0; i < count; i++) {
            if (i == sel)
                printf(CLR_GREEN "│  ● %s" CLR_RESET "\n", options[i]);
            else
                printf(CLR_DIM "│  ○ %s" CLR_RESET "\n", options[i]);
        }
        fflush(stdout);

        /* Read key */
        int key = term_read_key();
        if (key == 'A' && sel > 0)          sel--;       /* up */
        else if (key == 'B' && sel < count - 1) sel++;   /* down */
        else if (key == '\r' || key == '\n') break;      /* enter */
        else if (key == 'q' || key == 27)   { sel = -1; break; } /* quit */

        ansi_clear_lines(total_lines);
    }

    /* Redraw as confirmed */
    ansi_clear_lines(total_lines);
    printf(CLR_GREEN "✓ %s: " CLR_BOLD "%s" CLR_RESET "\n",
           prompt, sel >= 0 ? options[sel] : "(cancelled)");
    fflush(stdout);

    return sel;
}

/* ── Widget: checkbox select (multi-toggle) ──────────────────────────────── */

/**
 * Display a list with space-to-toggle, enter-to-confirm.
 * `selected` is an array of bools, modified in place.
 *
 *  ◆ prompt  (space=toggle, enter=confirm)
 *  │  ✓ checked option
 *  │  · unchecked option
 */
static void widget_checkbox(const char *prompt, const char **options, int count,
                            int *selected) {
    int cur = 0;
    int total_lines = count + 1;

    for (;;) {
        printf(CLR_CYAN CLR_BOLD "◆ %s" CLR_RESET
               CLR_DIM "  (↑↓ move, space toggle, enter confirm)" CLR_RESET "\n",
               prompt);
        for (int i = 0; i < count; i++) {
            const char *mark = selected[i] ? (CLR_GREEN "✓") : (CLR_DIM "·");
            if (i == cur)
                printf("%s " CLR_BOLD "%s" CLR_RESET "\n", mark, options[i]);
            else
                printf("%s %s" CLR_RESET "\n", mark, options[i]);
        }
        fflush(stdout);

        int key = term_read_key();
        if (key == 'A' && cur > 0)            cur--;
        else if (key == 'B' && cur < count-1) cur++;
        else if (key == ' ')                  selected[cur] = !selected[cur];
        else if (key == '\r' || key == '\n')  break;
        else if (key == 'q' || key == 27)     break;

        ansi_clear_lines(total_lines);
    }

    /* Redraw as confirmed */
    ansi_clear_lines(total_lines);
    printf(CLR_GREEN "✓ %s: " CLR_BOLD, prompt);
    int first = 1;
    for (int i = 0; i < count; i++) {
        if (selected[i]) {
            if (!first) printf(", ");
            printf("%s", options[i]);
            first = 0;
        }
    }
    if (first) printf("(none)");
    printf(CLR_RESET "\n");
    fflush(stdout);
}

/* ── Widget: text input ──────────────────────────────────────────────────── */

/**
 * Single-line text input with prompt.
 * Returns the entered string in `buf` (max `buflen` chars).
 */
static void widget_text_input(const char *prompt, const char *hint,
                              char *buf, int buflen) {
    int len = 0;
    buf[0] = '\0';

    for (;;) {
        printf(CLR_CYAN CLR_BOLD "◆ %s: " CLR_RESET, prompt);
        if (len > 0)
            printf("%s", buf);
        else
            printf(CLR_DIM "%s" CLR_RESET, hint);
        fflush(stdout);

        int key = term_read_key();
        if (key == '\r' || key == '\n') {
            printf("\n");
            break;
        }
        else if ((key == 127 || key == 8) && len > 0) { /* backspace */
            buf[--len] = '\0';
        }
        else if (key >= 32 && key < 127 && len < buflen - 1) {
            buf[len++] = (char)key;
            buf[len] = '\0';
        }
        else if (key == 27) { /* ESC — cancel */
            buf[0] = '\0';
            printf("\n");
            break;
        }

        /* Clear and redraw */
        printf("\r\033[2K");
    }

    /* Confirmed line */
    printf("\033[A\033[2K"); /* clear the input line */
    printf(CLR_GREEN "✓ %s: " CLR_BOLD "%s" CLR_RESET "\n",
           prompt, len > 0 ? buf : "(auto)");
    fflush(stdout);
}

/* ── Widget: confirm ─────────────────────────────────────────────────────── */

static int widget_confirm(const char *prompt) {
    printf(CLR_YELLOW CLR_BOLD "◇ %s " CLR_RESET CLR_DIM "[Y/n] " CLR_RESET,
           prompt);
    fflush(stdout);

    int key = term_read_key();
    int yes = (key == 'y' || key == 'Y' || key == '\r' || key == '\n');

    printf("\r\033[2K");
    printf(CLR_GREEN "✓ %s: %s" CLR_RESET "\n",
           prompt, yes ? "Yes" : "No");
    fflush(stdout);

    return yes;
}

/* ── Main interactive flow ───────────────────────────────────────────────── */

int membench_cli_interactive(membench_options_t *opts) {
    if (!opts) return -1;
    if (!term_is_tty()) return -1;

    term_raw_enter();

    /* Header */
    printf("\n");
    printf(CLR_MAGENTA CLR_BOLD
           "  ╭──────────────────────────────────╮\n"
           "  │      Volatile MemBench           │\n"
           "  │   Memory Performance Benchmark   │\n"
           "  ╰──────────────────────────────────╯"
           CLR_RESET "\n\n");
    fflush(stdout);

    /* Defaults */
    opts->target = MEMBENCH_TARGET_CPU;
    opts->tests = MEMBENCH_TEST_ALL;
    opts->format = MEMBENCH_FMT_TABLE;
    opts->buffer_size = 0;
    opts->iterations = 0;
    opts->gpu_device = 0;
    opts->verbose = false;
    opts->show_help = false;

    /* 1. Target selection */
    {
        const char *target_opts[] = { "CPU", "GPU", "Both (CPU + GPU)" };
        int sel = widget_radio("Select target", target_opts, 3, 0);
        if (sel < 0) goto cancelled;
        const membench_target_t map[] = {
            MEMBENCH_TARGET_CPU, MEMBENCH_TARGET_GPU, MEMBENCH_TARGET_ALL
        };
        opts->target = map[sel];
    }

    printf("\n");

    /* 2. Test selection (checkbox) */
    {
        const char *test_opts[] = { "Latency", "Bandwidth", "Cache Detection" };
        int selected[] = { 1, 1, 1 };
        widget_checkbox("Select tests", test_opts, 3, selected);
        opts->tests = 0;
        if (selected[0]) opts->tests |= MEMBENCH_TEST_LATENCY;
        if (selected[1]) opts->tests |= MEMBENCH_TEST_BANDWIDTH;
        if (selected[2]) opts->tests |= MEMBENCH_TEST_CACHE_DETECT;
        if (opts->tests == 0) opts->tests = MEMBENCH_TEST_ALL;
    }

    printf("\n");

    /* 3. Buffer size */
    {
        const char *size_opts[] = {
            "Auto (sweep all tiers)",
            "Custom size"
        };
        int sel = widget_radio("Buffer size", size_opts, 2, 0);
        if (sel < 0) goto cancelled;
        if (sel == 1) {
            char buf[64];
            term_raw_leave();
            /* Briefly leave raw mode for text input readability */
            term_raw_enter();
            widget_text_input("Enter size (e.g. 32K, 4M, 1G)", "32K", buf, sizeof(buf));
            if (buf[0]) {
                /* Parse the size string */
                char *end = NULL;
                double val = strtod(buf, &end);
                if (end && *end) {
                    switch (*end) {
                        case 'k': case 'K': val *= 1024; break;
                        case 'm': case 'M': val *= 1024 * 1024; break;
                        case 'g': case 'G': val *= 1024.0 * 1024 * 1024; break;
                        default: break;
                    }
                }
                opts->buffer_size = (size_t)val;
            }
        }
    }

    printf("\n");

    /* 4. Output format */
    {
        const char *fmt_opts[] = { "Table (human-readable)", "CSV", "JSON" };
        int sel = widget_radio("Output format", fmt_opts, 3, 0);
        if (sel < 0) goto cancelled;
        const membench_output_fmt_t map[] = {
            MEMBENCH_FMT_TABLE, MEMBENCH_FMT_CSV, MEMBENCH_FMT_JSON
        };
        opts->format = map[sel];
    }

    printf("\n");

    /* 5. Verbose */
    {
        const char *v_opts[] = { "Normal", "Verbose (show latency curves)" };
        int sel = widget_radio("Detail level", v_opts, 2, 0);
        if (sel < 0) goto cancelled;
        opts->verbose = (sel == 1);
    }

    printf("\n");

    /* 6. Confirm */
    {
        int yes = widget_confirm("Run benchmarks?");
        if (!yes) goto cancelled;
    }

    printf("\n");
    term_raw_leave();
    return 0;

cancelled:
    printf("\n" CLR_DIM "Cancelled." CLR_RESET "\n");
    term_raw_leave();
    return -1;
}
