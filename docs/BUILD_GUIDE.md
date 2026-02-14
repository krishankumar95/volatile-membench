# Building Volatile MemBench

Step-by-step instructions for building on Windows, Linux, and macOS.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Automated Setup (Recommended)](#automated-setup-recommended)
3. [Manual Build](#manual-build)
   - [Windows (MSVC)](#windows-msvc)
   - [Linux (GCC/Clang)](#linux-gccclang)
   - [macOS (Apple Clang)](#macos-apple-clang)
4. [Build Presets](#build-presets)
5. [CMake Options](#cmake-options)
6. [GPU Support](#gpu-support)
   - [NVIDIA (CUDA)](#nvidia-cuda)
   - [AMD (ROCm/HIP)](#amd-rocmhip)
   - [CPU-Only Build](#cpu-only-build)
7. [Running Tests](#running-tests)
8. [Build Configurations](#build-configurations)
9. [Makefile Shortcuts](#makefile-shortcuts)
10. [Troubleshooting](#troubleshooting)

---

## Prerequisites

| Tool | Required | Minimum Version | Notes |
|------|----------|----------------|-------|
| CMake | **Yes** | 3.20 | Build system generator |
| C Compiler | **Yes** | C11 support | MSVC 2019+, GCC 9+, Clang 11+, Apple Clang |
| Git | Recommended | Any | Cloning the repo |
| CUDA Toolkit | Optional | 11.0+ | NVIDIA GPU benchmarks |
| ROCm / HIP | Optional | 5.0+ | AMD GPU benchmarks (Linux only) |
| Ninja | Optional | Any | Faster parallel builds |

**Supported architectures**: x86_64 and ARM64 (Apple Silicon, Linux aarch64). Architecture is auto-detected by CMake.

### Checking Your Environment

```bash
cmake --version        # Need 3.20+
git --version          # Recommended
nvcc --version         # Optional, for NVIDIA GPU support
```

**Windows (MSVC)**: Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with the **"Desktop development with C++"** workload. This provides MSVC, the Windows SDK, and CMake.

**Linux**: Install build essentials:
```bash
# Ubuntu / Debian
sudo apt install build-essential cmake

# Fedora
sudo dnf install gcc cmake make

# Arch
sudo pacman -S base-devel cmake
```

**macOS**: Install Xcode Command Line Tools:
```bash
xcode-select --install
brew install cmake    # if not bundled with Xcode
```

---

## Automated Setup (Recommended)

The easiest path — scripts detect your platform, install missing dependencies, and build the project:

### Windows (PowerShell)

```powershell
.\scripts\setup.ps1
```

| Flag | Effect |
|------|--------|
| `-SkipOptional` | Skip CUDA, Git, Ninja |
| `-NoBuild` | Install dependencies only |

### Linux / macOS (Bash)

```bash
chmod +x scripts/setup.sh
./scripts/setup.sh
```

| Flag | Effect |
|------|--------|
| `--skip-optional` | Skip CUDA, ROCm, Ninja, Git |
| `--no-build` | Install dependencies only |

### Makefile Shortcuts

```bash
make setup            # Full: deps + configure + build + test
make setup-minimal    # Required deps only, no build
```

---

## Manual Build

### Windows (MSVC)

**Option A — Using VS Developer Command Prompt** (recommended):

Open "x64 Native Tools Command Prompt for VS 2022" from the Start menu, then:

```cmd
cd path\to\volatile-membench
cmake --preset default
cmake --build build --config RelWithDebInfo
```

The executable is at `build\src\RelWithDebInfo\membench.exe`.

**Option B — Using PowerShell** (requires VS on PATH):

```powershell
# Auto-detect and activate MSVC environment
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath  = & $vsWhere -latest -property installationPath
$vcvars  = "$vsPath\VC\Auxiliary\Build\vcvarsall.bat"

# Import MSVC environment into PowerShell
cmd /c "`"$vcvars`" x64 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2])
    }
}

cmake --preset default
cmake --build build --config RelWithDebInfo
```

**Option C — Using Visual Studio IDE**:

1. Open Visual Studio 2022.
2. File → Open → CMake → select `CMakeLists.txt`.
3. VS auto-detects presets. Select **"default"** from the preset dropdown.
4. Build → Build All (Ctrl+Shift+B).

### Linux (GCC/Clang)

```bash
git clone <repo-url> volatile-membench
cd volatile-membench
cmake --preset default
cmake --build build
```

To use Clang instead of GCC:

```bash
CC=clang cmake --preset default
cmake --build build
```

### macOS (Apple Clang)

```bash
git clone <repo-url> volatile-membench
cd volatile-membench
cmake --preset default
cmake --build build
```

> **Note**: macOS has no CUDA support. GPU benchmarks are automatically disabled and a stub library is linked instead.

> **Apple Silicon (M1/M2/M3/M4)**: ARM64 architecture is auto-detected. The benchmark automatically detects the 128-byte cache line size (vs 64 bytes on x86) via `sysctl` and adjusts pointer-chase stride accordingly. System information reports P-core (high-performance) cache sizes by default.

---

## Build Presets

The project includes CMake presets in `CMakePresets.json`:

| Preset | Build Dir | Type | GPU |
|--------|-----------|------|-----|
| `default` | `build/` | RelWithDebInfo | Auto-detected |
| `release` | `build-release/` | Release | Auto-detected |
| `debug` | `build-debug/` | Debug | Auto-detected |
| `no-gpu` | `build-cpu/` | RelWithDebInfo | Disabled |

```bash
# List available presets
cmake --list-presets

# Use a specific preset
cmake --preset release
cmake --build build-release

# CPU-only (skip GPU detection entirely)
cmake --preset no-gpu
cmake --build build-cpu
```

### When to Use Each Preset

| Preset | Use Case |
|--------|----------|
| `default` | Day-to-day development and testing |
| `release` | Final benchmarks (full optimization, no debug symbols) |
| `debug` | Debugging with symbols, assertions enabled |
| `no-gpu` | Systems without CUDA/ROCm, or to avoid GPU SDK requirement |

---

## CMake Options

These can be set with `-D` during configuration:

| Option | Default | Description |
|--------|---------|-------------|
| `MEMBENCH_ENABLE_GPU` | `ON` | Build GPU benchmarks (auto-detects CUDA/HIP) |
| `MEMBENCH_ENABLE_TESTS` | `ON` | Build unit tests |
| `CMAKE_BUILD_TYPE` | `RelWithDebInfo` | Build configuration |
| `CMAKE_CUDA_COMPILER` | Auto | Explicit path to `nvcc` |
| `CMAKE_CUDA_ARCHITECTURES` | `50;60;70;80;90` | CUDA compute capabilities to target |

### Examples

```bash
# Disable GPU and tests
cmake -S . -B build -DMEMBENCH_ENABLE_GPU=OFF -DMEMBENCH_ENABLE_TESTS=OFF

# Explicit CUDA compiler path
cmake -S . -B build -DCMAKE_CUDA_COMPILER="/usr/local/cuda/bin/nvcc"

# Target only modern GPUs (faster build)
cmake -S . -B build -DCMAKE_CUDA_ARCHITECTURES="80;90"
```

---

## GPU Support

### NVIDIA (CUDA)

**Requirements**: CUDA Toolkit 11.0+ with `nvcc` on PATH or `CUDA_PATH` set.

```bash
# Verify CUDA is installed
nvcc --version

# If CUDA_PATH is not set (Windows)
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.1

# Linux: usually auto-detected from /usr/local/cuda
```

CMake auto-detects CUDA during configuration. You'll see:

```
-- CUDA found: /usr/local/cuda/bin/nvcc
```

If CUDA is installed but not detected, pass it explicitly:

```bash
cmake --preset default -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc
```

**Windows-specific**: MSBuild (used by MSVC) requires `CUDA_PATH` or `CudaToolkitDir` as an environment variable at build time, not just at configure time. If you see:

```
error : The CUDA Toolkit directory '' does not exist.
```

Set the environment variable in your terminal session before building:

```powershell
$env:CUDA_PATH = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.1"
cmake --build build --config RelWithDebInfo
```

### AMD (ROCm/HIP)

Linux only. Requires ROCm 5.0+ and the `hip` CMake package.

```bash
# Ubuntu
sudo apt install rocm-hip-sdk

# Verify
hipcc --version
```

CMake auto-detects HIP. You'll see:

```
-- HIP found: 5.x.x
```

### CPU-Only Build

If you don't have a GPU SDK or don't need GPU benchmarks:

```bash
cmake --preset no-gpu
cmake --build build-cpu
```

The build links a stub GPU library that returns error codes for all GPU functions. The `--target gpu` flag will report "Failed to get GPU info" at runtime.

---

## Running Tests

The project includes three unit tests:

| Test | What It Validates |
|------|-------------------|
| `test_timer` | High-resolution timer initialization and resolution |
| `test_alloc` | Page-aligned memory allocation and freeing |
| `test_sysinfo` | System information detection (CPU model, cores, caches, RAM) |

### Using CTest

```bash
cd build
ctest --output-on-failure
```

Expected output:

```
Test project D:/SEngg/Repos/volatile-membench/build
    Start 1: timer
1/3 Test #1: timer ......................   Passed    0.01 sec
    Start 2: alloc
2/3 Test #2: alloc ......................   Passed    0.01 sec
    Start 3: sysinfo
3/3 Test #3: sysinfo ....................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 3
```

### Running Tests Individually

```bash
# From the build directory
./tests/RelWithDebInfo/test_timer      # Windows
./tests/test_timer                     # Linux/macOS
```

---

## Build Configurations

### RelWithDebInfo (Default)

```bash
cmake --preset default
cmake --build build --config RelWithDebInfo
```

- Optimized code (`/O2` on MSVC, `-O2` on GCC/Clang).
- Debug symbols included — can attach a debugger.
- **Best for benchmarking**: optimizations reflect real-world performance while allowing debugging.

### Release

```bash
cmake --preset release
cmake --build build-release --config Release
```

- Maximum optimization (`/O2` on MSVC, `-O3` on GCC/Clang).
- No debug symbols.
- Slightly smaller binary, marginally faster.
- Use for final published results.

### Debug

```bash
cmake --preset debug
cmake --build build-debug --config Debug
```

- No optimization.
- Full debug symbols and assertions.
- **Not suitable for benchmarking** — unoptimized code doesn't represent real performance.
- Use for debugging crashes or logic errors.

---

## Makefile Shortcuts

For convenience, a `Makefile` in the project root wraps common operations:

| Command | What It Does |
|---------|-------------|
| `make setup` | Full setup: install deps + configure + build + test |
| `make setup-minimal` | Install required deps only, skip GPU/Ninja |
| `make build` | Configure (default preset) + build |
| `make build-release` | Configure (release preset) + build |
| `make build-debug` | Configure (debug preset) + build |
| `make build-cpu` | Configure (no-gpu preset) + build |
| `make test` | Build + run CTest |
| `make clean` | Remove all build directories |
| `make help` | Show available commands |

> **Note**: On Windows, `make` requires GNU Make (via MSYS2, Git Bash, or WSL). Alternatively, use the setup scripts or CMake commands directly.

---

## Troubleshooting

### CMake not found

**Windows**: CMake ships with Visual Studio 2022. It's located at:
```
C:\Program Files\Microsoft Visual Studio\2022\<Edition>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
```
Open the VS Developer Command Prompt (which adds it to PATH), or install CMake system-wide from [cmake.org](https://cmake.org/).

**Linux/macOS**: Install via package manager (`apt install cmake`, `brew install cmake`).

### MSVC not detected

CMake must be run from a VS Developer Command Prompt, or you must run `vcvarsall.bat x64` first. The automated setup script handles this automatically.

### CUDA Toolkit directory '' does not exist

MSBuild needs the CUDA path at build time. Set it as an environment variable:

```powershell
# PowerShell
$env:CUDA_PATH = [System.Environment]::GetEnvironmentVariable('CUDA_PATH', 'Machine')

# cmd
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.1
```

Then rebuild. If the env var was set after VS was opened, restart the terminal.

### Windows SDK headers missing

If `windows.h` is not found, the Windows SDK headers may not be installed. Open the **Visual Studio Installer** → Modify → Individual Components → search for **"Windows SDK"** → install the latest version. Ensure both **"Desktop C++ x86/x64 build tools"** and the SDK are checked.

### Build succeeds but GPU tests fail at runtime

- Verify the GPU driver is installed: `nvidia-smi` (NVIDIA) or `rocm-smi` (AMD).
- Check that the GPU device index is correct: `--gpu-device 0` (default).
- Ensure CUDA runtime DLLs are on PATH (usually automatic if CUDA Toolkit is installed properly).

### Rebuild from scratch

If the build gets into a bad state:

```bash
# Delete the build directory and reconfigure
rm -rf build
cmake --preset default
cmake --build build --config RelWithDebInfo
```

Or use `make clean && make build`.

### Cross-compilation

Not currently supported. The benchmarks are designed to run on the same machine they're built on, since they measure hardware-specific properties.

---

## Output Artifacts

After a successful build:

**Linux / macOS (single-config generators — Make, Ninja):**
```
build/
├── src/
│   └── membench                  # Main executable
└── tests/
    ├── test_timer
    ├── test_alloc
    └── test_sysinfo
```

**Windows (multi-config generators — MSVC):**
```
build/
├── src/
│   └── RelWithDebInfo/
│       └── membench.exe          # Main executable
└── tests/
    └── RelWithDebInfo/
        ├── test_timer.exe
        ├── test_alloc.exe
        └── test_sysinfo.exe
```

The main executable is self-contained. Copy `membench` (or `membench.exe`) to any directory and run it. No additional files or libraries are needed at runtime (CUDA runtime is statically linked).
