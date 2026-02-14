# Volatile MemBench

A cross-platform C11 tool that measures what your hardware actually does — cache latency, DRAM bandwidth, and memory hierarchy boundaries — down to the nanosecond.

Built to answer questions like: *How fast is my L1 cache? Where does L2 end and L3 begin? How much real bandwidth can my CPU push?*

---

## Why This Exists

Most memory benchmarks are black boxes that spit out a single number. Volatile MemBench gives you the full picture:

- **Sub-nanosecond precision** — pointer-chase latency that defeats hardware prefetchers using randomized Hamiltonian cycles
- **Automatic cache boundary detection** — sweeps 77 buffer sizes and uses signal processing (derivative analysis + smoothing) to find where L1 ends and L2 begins
- **Validated across architectures** — tested on Apple M1 (ARM64, 128-byte cache lines) and AMD 5800X3D (x86_64, 96 MB V-Cache), with runtime adaptation for each
- **No dependencies** — pure C11, no external libraries, builds with any C11 compiler
- **GPU support** — CUDA (NVIDIA) and HIP (AMD) VRAM latency & bandwidth when available; gracefully skipped when not

## Key Results

| Metric | Apple M1 | AMD 5800X3D |
|--------|:--------:|:-----------:|
| L1 read latency | 0.95 ns | 0.9 ns |
| L2 read latency | 5.5 ns | 3.0 ns |
| DRAM latency | 104 ns | 52 ns |
| Read bandwidth | 56 GB/s | 40 GB/s |
| L1 size (detected) | 128 KB | 32 KB |
| L3 size (detected) | — | 96 MB |

Full results with analysis → [`docs/RESULTS.md`](docs/RESULTS.md)

---

## Quick Start

```bash
# Build
cmake --preset default && cmake --build build

# Run interactively (arrow keys to navigate)
./build/src/membench

# Or with arguments
./build/src/membench --test latency --size 32K
./build/src/membench --target cpu --format csv
./build/src/membench --help
```

### Interactive Mode

Run with no arguments for a guided terminal UI:

```
  ╭──────────────────────────────────╮
  │      Volatile MemBench           │
  │   Memory Performance Benchmark   │
  ╰──────────────────────────────────╯

? Select target
  ● CPU
    GPU
    Both (CPU + GPU)

? Select tests (space to toggle)
  [✓] Latency
  [✓] Bandwidth
  [ ] Cache Detection
```

Arrow keys navigate, Space toggles, Enter confirms, `q` cancels.

### Setup Scripts

```bash
# Linux / macOS — installs deps, builds, runs tests
chmod +x scripts/setup.sh && ./scripts/setup.sh

# Windows (PowerShell)
.\scripts\setup.ps1
```

Or use the convenience Makefile:

```bash
make setup          # Full setup (deps + build + test)
make build          # Build only
make test           # Run unit tests
make help           # See all targets
```

---

## What It Measures

### Latency (Read & Write)

Pointer-chase through a randomized linked list where each node is one cache line apart. Every load uses a `volatile` cast so the compiler can't optimize anything away. The random traversal order (Fisher-Yates shuffle → Hamiltonian cycle) defeats spatial and streaming prefetchers.

### Bandwidth (Read & Write)

Sequential streaming throughput using `memcpy`-style loops over aligned buffers. Measures real sustained throughput at each memory tier. RAM-aware — automatically skips buffer sizes that would trigger swap on low-memory systems.

### Cache Detection

Sweeps latency across 77 geometrically-spaced buffer sizes (2 KB → 512 MB), then applies signal processing to the latency curve: finite-difference derivatives, Gaussian smoothing, and peak detection to identify L1/L2/L3 boundaries. Reports detected sizes with the latency inflection points as evidence.

---

## Platform Support

| | x86_64 | ARM64 |
|---|:---:|:---:|
| **Linux** | ✓ | ✓ |
| **macOS** | ✓ | ✓ (Apple Silicon) |
| **Windows** | ✓ | — |

Runtime adaptations:
- **Cache line size** — detected via `sysctl` (macOS) or `sysconf` (Linux); 128 bytes on Apple Silicon, 64 bytes on x86
- **CPU frequency warmup** — 200 ms busy-loop before benchmarks to defeat dynamic frequency scaling
- **P-core targeting** — uses `hw.perflevel0.*` sysctls on Apple Silicon for accurate cache sizes (default sysctls report E-core values)
- **GPU graceful skip** — prints a one-line notice instead of errors when CUDA/HIP isn't available

## Building

### Prerequisites

| Tool | Required | Version | Notes |
|------|----------|---------|-------|
| CMake | **Yes** | 3.20+ | Build system generator |
| C compiler | **Yes** | C11 | MSVC 2019+, GCC 9+, Clang 11+ |
| CUDA Toolkit | Optional | 11.0+ | NVIDIA GPU benchmarks |
| ROCm / HIP | Optional | 5.0+ | AMD GPU benchmarks (Linux) |

### Build Presets

```bash
cmake --preset default     # RelWithDebInfo, auto-detect GPU
cmake --preset release     # Optimized release
cmake --preset debug       # Debug with sanitizers
cmake --preset no-gpu      # Skip GPU entirely
```

Then: `cmake --build build`

### Tests

```bash
cd build && ctest --output-on-failure
```

Three unit tests: timer precision, aligned allocation, system info detection.

## Output Formats

| Format | Flag | Use Case |
|--------|------|----------|
| Table | `--format table` (default) | Human-readable terminal output |
| CSV | `--format csv` | Spreadsheet import, scripts |
| JSON | `--format json` | Programmatic consumption |

---

## Project Structure

```
volatile-membench/
├── include/membench/    # Public API headers
├── src/
│   ├── core/            # Timer, allocator, CLI, interactive UI, sysinfo
│   ├── cpu/             # Latency, bandwidth, cache detection benchmarks
│   └── gpu/             # CUDA/HIP implementations (or stub)
├── tests/               # Unit tests (timer, alloc, sysinfo)
├── cmake/               # Platform, architecture, GPU detection modules
├── docs/
│   ├── BUILD_GUIDE.md   # Detailed build instructions
│   ├── USER_GUIDE.md    # Full CLI reference and examples
│   ├── KEY_LEARNINGS.md # 18 technical deep-dives on memory hierarchies
│   └── RESULTS.md       # Multi-platform benchmark data with analysis
├── scripts/             # Cross-platform setup scripts
├── Makefile             # Convenience wrapper for CMake
└── CMakeLists.txt
```

## Documentation

| Document | What's in it |
|----------|-------------|
| [User Guide](docs/USER_GUIDE.md) | CLI reference, all flags, examples, troubleshooting |
| [Build Troubleshooting](docs/BUILD_TROUBLESHOOTING.md) | Platform-specific build instructions, GPU SDK setup, CMake options |
| [Results](docs/RESULTS.md) | M1 vs 5800X3D benchmark data with analysis |
| [Key Learnings](docs/KEY_LEARNINGS.md) | 18 sections on memory walls, volatile semantics, prefetcher defeat, cache detection signal processing |

---

## License

Open source — see LICENSE file.

## Acknowledgements

Initial codebase generated with Claude Opus 4.6. Platform-specific bugs (frequency scaling, cache line size, sysinfo accuracy) diagnosed and fixed through iterative benchmarking. Pitfalls catalogued in [Key Learnings](docs/KEY_LEARNINGS.md).
