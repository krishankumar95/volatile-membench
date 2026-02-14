# Copilot Instructions — Volatile MemBench

## Project Overview

Cross-platform C11 memory benchmarking tool measuring cache/DRAM latency, bandwidth, and cache hierarchy detection. Targets CPU (x86_64, ARM64) and GPU (CUDA, HIP). Pure C — no C++, no external dependencies.

## Language & Standard

- **C11** (`-std=c11`). No C++ features, no GCC extensions.
- All source in `src/`, public headers in `include/membench/`, tests in `tests/`.

## Naming Conventions

| Category | Pattern | Example |
|----------|---------|---------|
| Types (structs, enums) | `membench_*_t` | `membench_latency_result_t` |
| Public functions | `membench_*()` | `membench_cpu_read_latency()` |
| Static/private functions | `static` keyword, no prefix | `static void build_pointer_chase_cl()` |
| Macros/constants | `UPPERCASE_SNAKE_CASE` | `CACHE_LINE_BYTES_DEFAULT` |
| Platform macros | `MEMBENCH_PLATFORM_*` | `MEMBENCH_PLATFORM_MACOS` |
| Architecture macros | `MEMBENCH_ARCH_*` | `MEMBENCH_ARCH_ARM64` |

## Code Style

- 4-space indentation, K&R brace style (opening brace on same line).
- Include guards: `#ifndef MEMBENCH_*_H` / `#define MEMBENCH_*_H` / `#endif`.
- All public headers wrap declarations in `#ifdef __cplusplus extern "C" { ... }`.
- Minimal comments — only for non-obvious logic.

## Error Handling

- Functions return `int`: `0` = success, `-1` = error.
- Allocators return `NULL` on failure.
- **No exceptions, no `abort()`, no `assert()` in production code.** Always propagate errors up.
- Validate inputs early: `if (!result || buffer_size == 0) return -1;`
- GPU stub functions return `-1` silently (no stderr spam).

## Memory Management

- **Always use `membench_alloc(size)` / `membench_free(ptr, size)`** — never raw `malloc`/`free`.
- Allocations are page-aligned (`mmap` on POSIX, `VirtualAlloc` on Windows).
- `membench_free()` requires both pointer and size.
- All allocated memory is zeroed (`memset` in `membench_alloc`).

## Platform Abstraction

- Use `#if defined(MEMBENCH_PLATFORM_WINDOWS)` / `MEMBENCH_PLATFORM_LINUX` / `MEMBENCH_PLATFORM_MACOS` — defined in `platform.h`.
- **Never use `#ifdef` alone** — always `#if defined(...)` for consistency.
- Platform macros are auto-detected at compile time; don't hardcode.
- Every platform-specific block must have a fallback or all three platforms covered.

### Platform-specific patterns:
```c
#if defined(MEMBENCH_PLATFORM_WINDOWS)
    /* Windows implementation */
#elif defined(MEMBENCH_PLATFORM_MACOS)
    /* macOS implementation */
#else
    /* Linux / POSIX fallback */
#endif
```

## Critical: Volatile Semantics

**All pointer-chase loads MUST use `volatile` casts.** Without this, the compiler eliminates the entire benchmark loop (measured 0.00 ns on MSVC).

```c
// CORRECT — volatile prevents optimization
p = *(void * volatile *)p;

// WRONG — compiler may hoist or eliminate
p = *(void **)p;
```

**Sink pattern** — prevent dead-code elimination of benchmark results:
```c
volatile void *sink = p;
(void)sink;
```

## Cache Line Stride

- Pointer-chase nodes must be **exactly one cache line apart** (not `sizeof(void*)`).
- Cache line size is detected at **runtime**: 128 bytes on Apple Silicon, 64 bytes on x86.
- Use `membench_get_cache_line_size()` or `sysctl`/`sysconf` — never hardcode 64.
- Dividing buffer size by `sizeof(void*)` instead of cache line size produces 8-16× wrong iteration counts.

## Apple Silicon Considerations

- Cache line: 128 bytes (not 64).
- Page size: 16 KB (not 4 KB).
- Default `hw.l1dcachesize` returns **E-core** values. Use `hw.perflevel0.*` for P-core.
- No `pthread_setaffinity` — use `pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE)` for P-core hints.
- CPU frequency scales from ~600 MHz to 3.2 GHz dynamically — benchmarks need a warmup phase.
- No CUDA/HIP — GPU tests are gracefully skipped.

## Build System

- CMake 3.20+, presets in `CMakePresets.json` (default, release, debug, no-gpu).
- Three static libraries: `membench_core`, `membench_cpu`, `membench_gpu`.
- GPU: auto-detected; `gpu_stub.c` linked when CUDA/HIP absent.
- Build: `cmake --preset default && cmake --build build`
- Test: `cd build && ctest --output-on-failure`
- Convenience: `make build`, `make test`, `make clean`

### Adding new source files:
Add to the appropriate library in `src/CMakeLists.txt`:
- Core utilities → `membench_core` static library
- CPU benchmarks → `membench_cpu` static library
- GPU kernels → `membench_gpu` (CUDA/HIP or stub)

## Testing

- Tests are standalone C executables returning 0 (pass) or non-zero (fail).
- Linked against `membench_core`. Registered with CTest via `add_test()`.
- Pattern: print "Test: ...", validate, print "PASS"/"FAIL", return 0/1.
- Three existing tests: `test_timer`, `test_alloc`, `test_sysinfo`.

## Benchmarking Patterns

- **Latency**: Pointer-chase through randomized Hamiltonian cycle (Fisher-Yates shuffle). Defeats prefetchers.
- **Bandwidth**: Sequential streaming with `volatile` sink to prevent elision.
- **Always fence**: Use `memory_fence()` before `membench_timer_ns()` calls.
- **Warmup**: CPU frequency warmup (200ms busy-loop) runs before any benchmarks.
- **RAM-aware**: Skip buffer sizes ≥ 50% of physical RAM to avoid measuring swap.

## Interactive CLI

- `src/core/cli_interactive.c` — terminal raw mode with ANSI escape codes.
- POSIX: `termios` for raw mode. Windows: `SetConsoleMode` with virtual terminal.
- **Must restore terminal state on all exit paths** (confirm, cancel, error).
- Widgets: radio select, checkbox, text input, confirm.
- Interactive mode triggers when `argc == 1` and `isatty(STDIN_FILENO)`.

## Output

- Three formats: table (default), CSV, JSON.
- Format-specific printing via `membench_print_*()` functions in `output.c`.
- All output to `stdout`; errors to `stderr`.

## Documentation

| File | Purpose |
|------|---------|
| `README.md` | Overview, quick start, feature highlights |
| `docs/USER_GUIDE.md` | Full CLI reference, interpreting results |
| `docs/BUILD_TROUBLESHOOTING.md` | Platform-specific build issues, GPU SDK setup |
| `docs/KEY_LEARNINGS.md` | Technical deep-dives (18 sections) |
| `docs/RESULTS.md` | Multi-platform benchmark data |
