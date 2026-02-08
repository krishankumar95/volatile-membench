# Volatile MemBench — User Guide

A comprehensive guide to using Volatile MemBench for CPU and GPU memory performance evaluation.

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Command-Line Reference](#command-line-reference)
4. [Tests](#tests)
   - [Latency](#latency)
   - [Bandwidth](#bandwidth)
   - [Cache Detection](#cache-detection)
5. [Targets](#targets)
6. [Output Formats](#output-formats)
7. [Default Sweep Sizes](#default-sweep-sizes)
8. [Examples](#examples)
9. [Interpreting Results](#interpreting-results)
10. [Troubleshooting](#troubleshooting)

---

## Overview

Volatile MemBench measures the performance characteristics of volatile memory (CPU caches, system DRAM, and GPU VRAM):

- **Latency** — How long it takes to read/write a single random cache line (nanoseconds). Uses a pointer-chase technique that defeats hardware prefetchers.
- **Bandwidth** — Streaming throughput when reading/writing sequentially (GB/s).
- **Cache Detection** — Automatically identifies L1, L2, and L3 cache sizes by sweeping latency across buffer sizes.

Supported hardware:
- **CPU**: x86_64 and ARM64
- **GPU**: NVIDIA (CUDA), AMD (HIP/ROCm — Linux only)

---

## Quick Start

```bash
# Run all CPU benchmarks with defaults
membench

# Run everything (CPU + GPU)
membench --target all

# Just bandwidth across all memory tiers
membench --target all --test bandwidth

# See help
membench --help
```

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
- A linked list is built within the buffer with cache-line stride (64 bytes between nodes).
- Each node points to a random (but unique) location, forming a Hamiltonian cycle.
- The benchmark walks the chain, timing each dereference.
- This defeats hardware prefetchers, measuring true **random-access latency**.

Results include both **read latency** and **write latency**:
- **Read**: Follows the pointer chain, reading each node via a `volatile` load.
- **Write**: Follows the pointer chain, writing a scratch word at each node before following the pointer (dependent read-write chain).

When no `--size` is specified, latency sweeps across 8 buffer sizes covering all memory tiers (L1 → L2 → L3 → DRAM).

### Bandwidth

```bash
membench --test bandwidth
```

Measures **sequential streaming throughput** (GB/s):
- **Read bandwidth**: Sequential 64-bit loads across the entire buffer.
- **Write bandwidth**: Sequential 64-bit stores across the entire buffer.

When no `--size` is specified, bandwidth sweeps across 8 buffer sizes from L1 through DRAM for CPU, and 3 sizes for GPU.

### Cache Detection

```bash
membench --test cache-detect
```

Automatically detects L1, L2, and L3 cache sizes by:
1. Sweeping pointer-chase latency across 77 buffer sizes (1 KB to 512 MB, logarithmic spacing).
2. Computing the derivative of the log-latency vs. log-size curve.
3. Finding derivative peaks (transition points between cache levels).
4. Using geometric-mean crossing with log-interpolation for sub-sample precision.

Use `--verbose` to see the full latency curve with all 77 data points.

> **Note**: The benchmark thread is pinned to core 0 for stable measurements, since L1 and L2 caches are per-core.

---

## Targets

| Target | Flag | What it benchmarks |
|--------|------|--------------------|
| CPU | `--target cpu` (default) | CPU caches + system DRAM |
| GPU | `--target gpu` | GPU VRAM (requires CUDA or HIP) |
| All | `--target all` | Both CPU and GPU |

### GPU Options

```bash
# Use a specific GPU (multi-GPU systems)
membench --target gpu --gpu-device 1

# GPU with custom size
membench --target gpu --test bandwidth --size 512M
```

GPU benchmarks display device info (name, VRAM, bus width, memory clock, theoretical bandwidth) before running tests.

---

## Output Formats

### Table (default)

Human-readable, aligned columns:

```
=== CPU Read Latency ===
  Read Latency          size=16.0 KB     latency=    0.91 ns  (2499840 accesses)
  Read Latency          size=256.0 MB    latency=   51.49 ns  (8388608 accesses)
```

### CSV

Machine-readable, for piping to scripts or spreadsheets:

```bash
membench --test latency --format csv > results.csv
```

### JSON

Structured JSON objects (one per line), for programmatic consumption:

```bash
membench --test bandwidth --format json | python analyze.py
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

### Run all benchmarks on CPU and GPU

```bash
membench --target all
```

### Bandwidth across all memory tiers

```bash
membench --target all --test bandwidth
```

### Latency at a specific buffer size

```bash
# Measure latency when working set fits in L1 (32 KB)
membench --test latency --size 32K

# Measure latency at DRAM level (512 MB)
membench --test latency --size 512M
```

### Detect cache hierarchy with full curve

```bash
membench --test cache-detect --verbose
```

### Export results as CSV

```bash
membench --target all --test bandwidth --format csv > bandwidth.csv
```

### Export results as JSON

```bash
membench --test latency --format json > latency.json
```

### GPU benchmark on specific device

```bash
membench --target gpu --gpu-device 0 --test bandwidth
```

### Combine multiple tests

```bash
membench --test latency,bandwidth
membench --test latency,cache-detect --verbose
```

### Custom iterations (for more precise measurements)

```bash
membench --test latency --size 4M --iterations 1000
```

---

## Interpreting Results

### Latency

| Memory Tier | Typical Latency Range | What to Expect |
|-------------|----------------------|----------------|
| L1 Cache | 0.5 – 1.5 ns | ~4 CPU cycles |
| L2 Cache | 2 – 5 ns | ~10–15 cycles |
| L3 Cache | 8 – 15 ns | ~30–50 cycles |
| DRAM (DDR4) | 40 – 80 ns | ~150–250 cycles |
| DRAM (DDR5) | 50 – 100 ns | Higher latency, higher bandwidth |
| GPU VRAM (GDDR6/6X) | 100 – 200 ns | Flat across sizes (no cache hierarchy) |
| GPU VRAM (GDDR7) | 80 – 150 ns | Similar to GDDR6X |

**Key observations**:
- Latency should step up sharply at each cache boundary.
- GPU VRAM latency is flat regardless of buffer size (no cache tiers visible from pointer-chase).
- Write latency ≈ read latency for in-cache sizes; may differ at DRAM level.

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
- GPU bandwidth peaks for mid-sized buffers (GPU L2 cache effect) and decreases for VRAM-sized buffers.

### Cache Detection

The estimated sizes should be close to (but not exact matches of) the OS-reported values:
- **L1**: Typically within ±20% of actual.
- **L2**: Typically within ±30% of actual.
- **L3**: May vary more on processors with non-uniform L3 (e.g., AMD 3D V-Cache, chiplet designs).

Use `--verbose` to inspect the raw latency curve and visually confirm where transitions occur.

---

## Troubleshooting

### "0.00 ns" latency

The compiler may have optimized away the pointer chase. This shouldn't happen with the current codebase (volatile loads prevent it), but if you see it after modifying the code, ensure pointer dereferences go through a `volatile` cast.

### GPU benchmarks not available

```
Failed to get GPU info for device 0
```

- Ensure CUDA Toolkit or ROCm/HIP is installed.
- Verify the GPU driver is loaded (`nvidia-smi` or `rocm-smi`).
- Check `--gpu-device` index is valid.

### Very slow GPU latency tests

GPU pointer-chase latency is inherently slow for large buffers. Default sizes are capped at 32 MB with 10 iterations. If you specify a large `--size` (e.g., 256M), expect the test to take several minutes.

### Inconsistent results between runs

- Close background applications.
- Disable CPU frequency scaling (set performance governor on Linux).
- The benchmark pins to core 0 during cache detection for stability; other tests may benefit from manual affinity.
- Run multiple times and compare.

### Build issues

See the [setup scripts](../scripts/) or run:

```bash
# Linux/macOS
./scripts/setup.sh

# Windows PowerShell
.\scripts\setup.ps1
```
