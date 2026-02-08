# Volatile MemBench

A cross-platform benchmarking tool for volatile memory (SRAM caches & DRAM) performance evaluation.

## Features

- **Read/Write Latency** — Pointer-chase based random access latency measurement
- **Read/Write Bandwidth** — Sequential streaming throughput measurement
- **Cache Hierarchy Detection** — Automatic L1/L2/L3 cache size detection via latency curve analysis
- **GPU VRAM Benchmarks** — CUDA (NVIDIA) and HIP (AMD) global memory latency & bandwidth
- **Cross-platform** — Windows, Linux, macOS
- **Multi-architecture** — x86_64, ARM64

## Quick Start

The fastest way to get a working development environment:

```bash
# Linux / macOS
chmod +x scripts/setup.sh
./scripts/setup.sh
```

```powershell
# Windows (PowerShell)
.\scripts\setup.ps1
```

These scripts will **detect your platform**, install missing dependencies, and build the project automatically.

| Flag | PowerShell | Bash | Effect |
|------|-----------|------|--------|
| Skip optional deps | `-SkipOptional` | `--skip-optional` | Skip GPU SDK, Ninja, Git |
| No build | `-NoBuild` | `--no-build` | Install deps only |

There's also a convenience Makefile:

```bash
make setup            # Full setup (deps + build + test)
make setup-minimal    # Required deps only, no build
```

## Building

### Prerequisites

| Tool | Required | Version | Notes |
|------|----------|---------|-------|
| CMake | **Yes** | 3.20+ | Build system generator |
| C compiler | **Yes** | C11 support | MSVC 2019+, GCC 9+, Clang 11+ |
| Git | Recommended | Any | Source control |
| CUDA Toolkit | Optional | 11.0+ | NVIDIA GPU benchmarks |
| ROCm / HIP | Optional | 5.0+ | AMD GPU benchmarks (Linux only) |
| Ninja | Optional | Any | Faster parallel builds |

### Manual Build

```bash
cmake --preset default
cmake --build build
```

For CPU-only builds (skip GPU detection):
```bash
cmake --preset no-gpu
cmake --build build-cpu
```

### Run Tests

```bash
cd build
ctest --output-on-failure
```

## Usage

```bash
# Run all CPU benchmarks
./membench

# Run specific tests
./membench --test latency
./membench --test bandwidth
./membench --test cache-detect

# GPU benchmarks
./membench --target gpu --test bandwidth

# Custom buffer size and output format
./membench --test latency --size 32K --format csv

# See all options
./membench --help
```

## Output Formats

- `table` (default) — Human-readable table
- `csv` — Machine-readable CSV
- `json` — JSON objects (one per line)

## Architecture

```
volatile-membench/
├── include/membench/   # Public API headers
├── src/
│   ├── core/           # Timer, allocator, CLI, sysinfo, output
│   ├── cpu/            # Latency, bandwidth, cache-detect benchmarks
│   └── gpu/            # CUDA/HIP kernels (or stub when unavailable)
├── tests/              # Unit tests
├── cmake/              # Platform/arch/GPU detection modules
└── CMakeLists.txt
```

## TBD
- Validate extensively by building and running on Linux,Mac as well
- Layer a cross platform UI


## License

Open source — see LICENSE file. 

Built with Opus 4.6 ; pitfalls have been catalogued in key learnings and where patched by corrective nudging. 
