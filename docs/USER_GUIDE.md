# Volatile MemBench — User Guide

A comprehensive guide to using Volatile MemBench for CPU and GPU memory performance evaluation.

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Interactive Mode](#interactive-mode)
4. [Command-Line Reference](#command-line-reference)
5. [Tests](#tests)
   - [Latency](#latency)
   - [Bandwidth](#bandwidth)
   - [Cache Detection](#cache-detection)
6. [Targets](#targets)
7. [Output Formats](#output-formats)
8. [Default Sweep Sizes](#default-sweep-sizes)
9. [Examples](#examples)
10. [Interpreting Results](#interpreting-results)
11. [Platform Notes](#platform-notes)
12. [Troubleshooting](#troubleshooting)

---

## Overview

Volatile MemBench measures the performance characteristics of volatile memory (CPU caches, system DRAM, and GPU VRAM):

- **Latency** — How long it takes to read/write a single random cache line (nanoseconds). Uses a pointer-chase technique with randomized Hamiltonian cycles that defeat hardware prefetchers.
- **Bandwidth** — Streaming throughput when reading/writing sequentially (GB/s). RAM-aware — automatically skips sizes that would trigger swap on low-memory systems.
- **Cache Detection** — Automatically identifies L1, L2, and L3 cache sizes by sweeping latency across buffer sizes and applying signal processing (derivative analysis + Gaussian smoothing) to find boundaries.

Supported hardware:
- **CPU**: x86_64 and ARM64 (including Apple Silicon)
- **GPU**: NVIDIA (CUDA), AMD (HIP/ROCm — Linux only)
- **Platforms**: Linux, macOS, Windows

---

## Quick Start

```bash
# Build
cmake --preset default && cmake --build build

# Run interactively (no arguments — guided menu)
./build/src/membench

# Run with arguments (non-interactive)
./build/src/membench --test latency
./build/src/membench --target all --format csv

# See help
./build/src/membench --help
```

---

## Interactive Mode

When you run `membench` with **no arguments**, it launches an interactive terminal UI with cursor-based navigation. If stdin is not a TTY (piped input, CI, scripts), it falls back to showing the help message.

### Walkthrough

```
  ╭──────────────────────────────────╮
  │      Volatile MemBench           │
  │   Memory Performance Benchmark   │
  ╰──────────────────────────────────╯

? Select target
  ● CPU            ← highlighted, use ↑↓ to move
    GPU
    Both (CPU + GPU)
```

The interactive flow guides you through 6 steps:

| Step | Widget | Options |
|------|--------|---------|
| 1. Target | Radio select | CPU, GPU, Both |
| 2. Tests | Checkbox | Latency, Bandwidth, Cache Detection |
| 3. Buffer size | Radio select | Auto (sweep all tiers), Custom size |
| 4. Output format | Radio select | Table, CSV, JSON |
| 5. Detail level | Radio select | Normal, Verbose |
| 6. Confirm | Yes/No | Run benchmarks? |

### Controls

| Key | Action |
|-----|--------|
| `↑` / `↓` | Move selection |
| `Enter` | Confirm selection |
| `Space` | Toggle checkbox item |
| `q` or `Esc` | Cancel and exit |

### Custom Size Input

If you select "Custom size" in step 3, a text input appears:

```
? Enter size (e.g. 32K, 4M, 1G) [32K]: _
```

Type a size with optional suffix (`K`, `M`, `G`) and press Enter. The suffix is case-insensitive.

After confirming, benchmarks run with the selected options — identical to running with equivalent command-line flags.

---

## Command-Line Reference

```
Usage: membench [options]

Options:
  --target <cpu|gpu|all>       Target device (default: cpu)
  --test <tests>               Comma-separated: latency,bandwidth,cache-detect,all
                               (default: all)
  --size <bytes>               Buffer size for single-size tests (e.g. 1K, 32K, 4M, 1G)
                               (default: auto-selected per test)
  --iterations <n>             Number of iterations (default: auto)
  --gpu-device <id>            GPU device index (default: 0)
  --format <table|csv|json>    Output format (default: table)
  --verbose                    Enable verbose output (timer resolution, latency curves)
  --help, -h                   Show help message
```

When no arguments are provided and stdin is a TTY, interactive mode launches instead.

### Size Suffixes

The `--size` flag accepts human-readable size strings:

| Suffix | Multiplier | Example |
|--------|-----------|---------|
| `K`    | × 1,024   | `32K` = 32,768 bytes |
| `M`    | × 1,048,576 | `4M` = 4,194,304 bytes |
| `G`    | × 1,073,741,824 | `1G` = 1,073,741,824 bytes |
| _(none)_ | × 1 | `65536` = 65,536 bytes |

Suffixes are case-insensitive (`32k` and `32K` are equivalent).

---

## Tests

### Latency

```bash
membench --test latency
```

Measures **random-access latency** using a pointer-chase technique:
- A linked list is built within the buffer with **cache-line stride** (128 bytes on Apple Silicon, 64 bytes on x86). The stride is detected at runtime.
- Each node points to a random (but unique) location, forming a **Hamiltonian cycle** via Fisher-Yates shuffle.
- The benchmark walks the chain, timing each dereference through a `volatile` cast.
- The random traversal defeats both spatial and streaming hardware prefetchers.

Results include both **read latency** and **write latency**:
- **Read**: Follows the pointer chain, reading each node via a `volatile` load.
- **Write**: Follows the pointer chain, writing a scratch word at each node before following the pointer (dependent read-write chain).

When no `--size` is specified, latency sweeps across 8 buffer sizes covering all memory tiers (L1 → L2 → L3 → DRAM). Sizes that exceed 50% of physical RAM are automatically skipped.

### Bandwidth

```bash
membench --test bandwidth
```

Measures **sequential streaming throughput** (GB/s):
- **Read bandwidth**: Sequential 64-bit loads across the entire buffer.
- **Write bandwidth**: Sequential 64-bit stores across the entire buffer.

When no `--size` is specified, bandwidth sweeps across 8 buffer sizes from L1 through DRAM for CPU, and 3 sizes for GPU. Buffer sizes are capped at 50% of physical RAM to avoid measuring swap performance instead of DRAM.

### Cache Detection

```bash
membench --test cache-detect
```

Automatically detects L1, L2, and L3 cache sizes by:
1. Sweeping pointer-chase latency across **77 buffer sizes** (1 KB to 512 MB, logarithmic spacing — 4 samples per octave).
2. Computing the derivative of the log-latency vs. log-size curve.
3. Applying Gaussian smoothing to reduce noise.
4. Finding derivative peaks (transition points between cache levels).
5. Using geometric-mean crossing with log-interpolation for sub-sample precision.

Use `--verbose` to see the full latency curve with all 77 data points.

> **Note**: On Linux and Windows, the benchmark thread is pinned to core 0 for stable measurements (per-core L1/L2 caches). On macOS, a QoS hint (`USER_INTERACTIVE`) is used instead since thread affinity APIs are not available.

---

## Targets

| Target | Flag | What it benchmarks |
|--------|------|--------------------|
| CPU | `--target cpu` (default) | CPU caches + system DRAM |
| GPU | `--target gpu` | GPU VRAM (requires CUDA or HIP) |
| All | `--target all` | Both CPU and GPU |

### GPU Benchmarks

```bash
# Use a specific GPU (multi-GPU systems)
membench --target gpu --gpu-device 1

# GPU with custom size
membench --target gpu --test bandwidth --size 512M
```

GPU benchmarks display device info (name, VRAM, bus width, memory clock, theoretical bandwidth) before running tests.

When no GPU SDK is available (e.g., macOS, or Linux without CUDA/ROCm), GPU benchmarks are **gracefully skipped** with an informational message — no errors, exit code 0.

---

## Output Formats

### Table (default)

Human-readable, aligned columns:

```
=== System Information ===
  CPU:          Apple M1
  Cores:        8 physical, 8 logical
  L1 Data:      128.0 KB
  L2:           12.0 MB
  Total RAM:    8.0 GB

=== CPU Read Latency ===
  Read Latency          size=16.0 KB     latency=    0.95 ns  (20000000 accesses)
  Read Latency          size=256.0 MB    latency=  104.25 ns  (8388608 accesses)
```

### CSV

Machine-readable, for piping to scripts or spreadsheets:

```bash
membench --test latency --format csv > results.csv
```

### JSON

Structured JSON objects (one per line), for programmatic consumption:

```bash
membench --test bandwidth --format json | python3 analyze.py
```

---

## Default Sweep Sizes

When no `--size` is specified, each test automatically sweeps these buffer sizes:

### CPU Latency & Bandwidth

| Memory Tier | Sizes |
|-------------|-------|
| L1 Cache | 16 KB, 32 KB |
| L2 Cache | 128 KB, 512 KB |
| L3 Cache | 4 MB, 32 MB |
| DRAM | 64 MB, 256 MB |

Sizes exceeding 50% of physical RAM are skipped automatically (e.g., on an 8 GB machine, 4 GB+ sizes are excluded to avoid swap).

### GPU Bandwidth

| Size |
|------|
| 1 MB |
| 16 MB |
| 256 MB |

### GPU Latency

| Size |
|------|
| 1 MB |
| 4 MB |
| 32 MB |

### Cache Detection

Logarithmic sweep: **77 points** from **1 KB to 512 MB** (4 samples per octave/doubling).

---

## Examples

### Interactive mode

```bash
# Just run it — follow the prompts
./build/src/membench
```

### Run all CPU benchmarks

```bash
membench --target cpu
```

### Latency at a specific buffer size

```bash
# L1 hit (~1 ns)
membench --test latency --size 32K

# DRAM access (~50-100 ns)
membench --test latency --size 256M
```

### Bandwidth across all memory tiers

```bash
membench --test bandwidth
```

### Detect cache hierarchy with full curve

```bash
membench --test cache-detect --verbose
```

### Export results as CSV

```bash
membench --test bandwidth --format csv > bandwidth.csv
```

### Export results as JSON

```bash
membench --test latency --format json > latency.json
```

### Combine multiple tests

```bash
membench --test latency,bandwidth
membench --test latency,cache-detect --verbose
```

### GPU benchmark on specific device

```bash
membench --target gpu --gpu-device 0 --test bandwidth
```

### Custom iterations (for more precise measurements)

```bash
membench --test latency --size 4M --iterations 1000
```

### CPU + GPU with CSV export

```bash
membench --target all --format csv > full_results.csv
```

---

## Interpreting Results

### Latency

| Memory Tier | Typical Latency Range | What to Expect |
|-------------|----------------------|----------------|
| L1 Cache | 0.5 – 1.5 ns | ~3–4 CPU cycles |
| L2 Cache | 2 – 6 ns | ~10–20 cycles |
| L3 Cache | 8 – 15 ns | ~30–50 cycles |
| DRAM (DDR4) | 40 – 80 ns | ~150–250 cycles |
| DRAM (LPDDR4X) | 90 – 110 ns | Higher latency, lower power |
| DRAM (DDR5) | 50 – 100 ns | Higher latency than DDR4, higher bandwidth |
| GPU VRAM (GDDR6/7) | 80 – 200 ns | Flat across sizes (no visible cache hierarchy) |

**Key observations**:
- Latency should step up sharply at each cache boundary.
- L1 cache sizes vary: 128 KB on Apple Silicon P-cores, 32 KB on typical x86. Expect the L1→L2 transition at different sizes.
- GPU VRAM latency is flat regardless of buffer size (no cache tiers visible from pointer-chase).
- Write latency ≈ read latency for in-cache sizes (store buffers absorb writes); may differ at DRAM level.

### Bandwidth

| Memory Tier | What Affects It |
|-------------|----------------|
| L1 Cache | Core execution width, load/store ports |
| L2 Cache | L2 ↔ L1 bandwidth (usually 32–64 B/cycle) |
| L3 Cache | Ring/mesh interconnect bandwidth |
| DRAM | Channel count × frequency × bus width |
| GPU VRAM | Memory bus width × clock × protocol efficiency |

**Key observations**:
- Read bandwidth > write bandwidth (writes may require read-for-ownership).
- Bandwidth drops at each cache level boundary.
- Apple M1 achieves ~56 GB/s read, ~59 GB/s write at DRAM (LPDDR4X unified memory).
- AMD 5800X3D achieves ~40 GB/s read, ~27 GB/s write (DDR4-3200 dual-channel).
- GPU bandwidth typically peaks for mid-sized buffers (GPU L2 cache effect).

### Cache Detection

The estimated sizes should be close to (but not exact matches of) the OS-reported values:
- **L1**: Typically within ±20% of actual.
- **L2**: Typically within ±30% of actual.
- **L3**: May vary more on processors with non-uniform L3 (e.g., AMD 3D V-Cache, chiplet designs).
- **No L3 detected**: Expected on Apple Silicon — there's a System Level Cache (~16 MB) but its behavior doesn't produce a clean L3 latency step.

Use `--verbose` to inspect the raw latency curve and visually confirm where transitions occur.

---

## Platform Notes

### Apple Silicon (M1/M2/M3)

- **Cache line size**: 128 bytes (vs 64 on x86). Detected at runtime.
- **P-core vs E-core**: System info reports P-core cache sizes (`hw.perflevel0.*` sysctls). Default macOS APIs return E-core values which are smaller.
- **Frequency scaling**: M1 P-cores ramp from ~600 MHz to 3.2 GHz dynamically. A 200 ms warmup loop runs before benchmarks to stabilize the clock.
- **No thread affinity**: macOS doesn't expose `pthread_setaffinity`. A `USER_INTERACTIVE` QoS hint encourages scheduling on P-cores.
- **No CUDA/HIP**: GPU benchmarks are gracefully skipped with an informational message.

### x86_64 (Intel/AMD)

- **Cache line size**: 64 bytes (standard).
- **Thread affinity**: Linux uses `pthread_setaffinity_np`; Windows uses `SetThreadAffinityMask` to pin cache detection to core 0.
- **V-Cache**: AMD processors with 3D V-Cache (e.g., 5800X3D with 96 MB L3) show unique latency curves — the L3 is so large that DRAM transitions appear much later than on standard CPUs.

### Low-Memory Systems

- Bandwidth sweep sizes exceeding 50% of physical RAM are automatically skipped.
- On an 8 GB system, this prevents the 4 GB buffer from triggering OS swap (which would measure SSD speed, not DRAM).

---

## Troubleshooting

### "0.00 ns" latency

The compiler may have optimized away the pointer chase. This shouldn't happen with the current codebase (all loads use `volatile` casts), but if you see it after modifying the code, ensure pointer dereferences go through a `volatile` cast:
```c
node = *(void * volatile *)node;  // correct — volatile prevents elimination
```

### GPU benchmarks skipped

```
=== GPU Information ===
  Skipped — no GPU support (compiled without CUDA/HIP)
```

This is normal on macOS or systems without CUDA/ROCm installed. To enable GPU benchmarks:
- **NVIDIA**: Install [CUDA Toolkit](https://developer.nvidia.com/cuda-downloads) 11.0+
- **AMD**: Install [ROCm](https://rocm.docs.amd.com/) 5.0+ (Linux only)

Then rebuild: `cmake --preset default && cmake --build build`

### Inflated latency numbers

If L1 latency is 2–3× higher than expected (~2.8 ns instead of ~0.9 ns):
- **CPU frequency scaling**: The CPU may be running at a lower clock. The built-in warmup should handle this, but on heavily throttled systems (thermal limits, power saving), results may still be affected.
- **Running on E-cores**: On Apple Silicon, the OS may schedule the benchmark on efficiency cores. Close other apps to encourage P-core scheduling.

### Very slow GPU latency tests

GPU pointer-chase latency is inherently slow for large buffers. Default sizes are capped at 32 MB with 10 iterations. If you specify a large `--size` (e.g., 256M), expect the test to take several minutes.

### Inconsistent results between runs

- Close background applications.
- On Linux: set the CPU governor to performance (`cpupower frequency-set -g performance`).
- The benchmark pins to core 0 during cache detection for stability; other tests may benefit from manual affinity.
- Run multiple times and compare.

### Build issues

See the [Build Guide](BUILD_GUIDE.md) for detailed platform-specific instructions, or run the setup scripts:

```bash
# Linux / macOS
./scripts/setup.sh

# Windows PowerShell
.\scripts\setup.ps1

# Or use the Makefile
make setup
```
