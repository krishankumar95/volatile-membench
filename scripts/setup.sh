#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# Development environment setup for volatile-membench (Linux / macOS).
#
# Checks for and installs all required (and optional) dependencies.
#
# Required : CMake 3.20+, C11 compiler (GCC 9+ / Clang 11+ / Apple Clang)
# Optional : CUDA Toolkit (NVIDIA GPU benchmarks)
#            ROCm / HIP  (AMD GPU benchmarks)
#            Ninja       (faster builds)
#            Git         (source control)
#
# Usage:
#   ./scripts/setup.sh                 # full setup + build
#   ./scripts/setup.sh --skip-optional # skip optional deps
#   ./scripts/setup.sh --no-build      # install deps only
#   ./scripts/setup.sh --help
# ─────────────────────────────────────────────────────────────────────────────

set -euo pipefail

# ── Flags ────────────────────────────────────────────────────────────────────

SKIP_OPTIONAL=false
NO_BUILD=false

for arg in "$@"; do
    case "$arg" in
        --skip-optional) SKIP_OPTIONAL=true ;;
        --no-build)      NO_BUILD=true ;;
        --help|-h)
            echo "Usage: $0 [--skip-optional] [--no-build]"
            exit 0
            ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

# ── Helpers ──────────────────────────────────────────────────────────────────

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; CYAN='\033[0;36m'; GRAY='\033[0;37m'; NC='\033[0m'
banner() { printf "\n${CYAN}═══ %s ═══${NC}\n" "$1"; }
ok()     { printf "  ${GREEN}✓ %s${NC}\n" "$1"; }
warn()   { printf "  ${YELLOW}⚠ %s${NC}\n" "$1"; }
err()    { printf "  ${RED}✗ %s${NC}\n" "$1"; }
info()   { printf "  ${GRAY}ℹ %s${NC}\n" "$1"; }

has_cmd() { command -v "$1" &>/dev/null; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ── Detect OS ────────────────────────────────────────────────────────────────

OS="unknown"
case "$(uname -s)" in
    Linux*)  OS="linux" ;;
    Darwin*) OS="macos" ;;
    *)       OS="unknown" ;;
esac

ARCH="$(uname -m)"

banner "Platform"
info "OS: $OS  |  Architecture: $ARCH"

# ── Package manager helpers ──────────────────────────────────────────────────

pkg_install() {
    # $1 = human label, remaining = per-PM package names handled below
    local label="$1"; shift

    if [[ "$OS" == "macos" ]]; then
        if has_cmd brew; then
            info "Installing $label via Homebrew …"
            brew install "$@"
        else
            err "Homebrew not found. Install it from https://brew.sh and re-run."
            return 1
        fi
    elif [[ "$OS" == "linux" ]]; then
        if has_cmd apt-get; then
            info "Installing $label via apt …"
            sudo apt-get update -qq
            sudo apt-get install -y "$@"
        elif has_cmd dnf; then
            info "Installing $label via dnf …"
            sudo dnf install -y "$@"
        elif has_cmd pacman; then
            info "Installing $label via pacman …"
            sudo pacman -S --noconfirm "$@"
        elif has_cmd zypper; then
            info "Installing $label via zypper …"
            sudo zypper install -y "$@"
        else
            err "No supported package manager found (apt/dnf/pacman/zypper). Install $label manually."
            return 1
        fi
    fi
}

# Distro-aware package names for common tools
pkg_cmake() {
    if [[ "$OS" == "macos" ]]; then pkg_install "CMake" cmake
    elif has_cmd apt-get;       then pkg_install "CMake" cmake
    elif has_cmd dnf;           then pkg_install "CMake" cmake
    elif has_cmd pacman;        then pkg_install "CMake" cmake
    elif has_cmd zypper;        then pkg_install "CMake" cmake
    fi
}

pkg_compiler() {
    if [[ "$OS" == "macos" ]]; then
        info "Installing Xcode Command Line Tools (provides Apple Clang) …"
        xcode-select --install 2>/dev/null || true
    elif has_cmd apt-get; then pkg_install "GCC" build-essential
    elif has_cmd dnf;     then pkg_install "GCC" gcc gcc-c++ make
    elif has_cmd pacman;  then pkg_install "GCC" base-devel
    elif has_cmd zypper;  then pkg_install "GCC" gcc gcc-c++ make
    fi
}

pkg_git() {
    pkg_install "Git" git
}

pkg_ninja() {
    if [[ "$OS" == "macos" ]]; then pkg_install "Ninja" ninja
    elif has_cmd apt-get;       then pkg_install "Ninja" ninja-build
    elif has_cmd dnf;           then pkg_install "Ninja" ninja-build
    elif has_cmd pacman;        then pkg_install "Ninja" ninja
    elif has_cmd zypper;        then pkg_install "Ninja" ninja
    fi
}

# ── Version comparison helper ────────────────────────────────────────────────

version_ge() {
    # Returns 0 (true) if $1 >= $2  (dot-separated versions)
    printf '%s\n%s' "$2" "$1" | sort -V -C
}

get_cmake_version() {
    if has_cmd cmake; then
        cmake --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?'
    fi
}

# ── 1. Git ───────────────────────────────────────────────────────────────────

banner "Git"
if has_cmd git; then
    ok "Git $(git --version | sed 's/git version //') found"
elif ! $SKIP_OPTIONAL; then
    warn "Git not found — installing …"
    pkg_git
    if has_cmd git; then ok "Git installed"; else warn "Git install may require a new shell"; fi
else
    info "Git not found (skipped — optional)"
fi

# ── 2. C Compiler ───────────────────────────────────────────────────────────

banner "C Compiler"

HAS_COMPILER=false

if has_cmd gcc; then
    GCC_VER="$(gcc --version | head -1)"
    ok "GCC found — $GCC_VER"
    HAS_COMPILER=true
fi

if has_cmd clang; then
    CLANG_VER="$(clang --version | head -1)"
    ok "Clang found — $CLANG_VER"
    HAS_COMPILER=true
fi

if ! $HAS_COMPILER; then
    warn "No C compiler found — installing …"
    pkg_compiler
    if has_cmd gcc || has_cmd clang; then
        ok "Compiler installed"
        HAS_COMPILER=true
    else
        err "Compiler installation failed. Please install GCC or Clang manually."
    fi
fi

# ── 3. CMake ─────────────────────────────────────────────────────────────────

banner "CMake"

CMAKE_MIN="3.20"
CMAKE_VER="$(get_cmake_version)"

if [[ -n "$CMAKE_VER" ]] && version_ge "$CMAKE_VER" "$CMAKE_MIN"; then
    ok "CMake $CMAKE_VER found (>= $CMAKE_MIN required)"
elif [[ -n "$CMAKE_VER" ]]; then
    warn "CMake $CMAKE_VER found but >= $CMAKE_MIN required — upgrading …"
    pkg_cmake
    CMAKE_VER="$(get_cmake_version)"
else
    warn "CMake not found — installing …"
    pkg_cmake
    CMAKE_VER="$(get_cmake_version)"
fi

# Verify
if [[ -n "$CMAKE_VER" ]] && version_ge "$CMAKE_VER" "$CMAKE_MIN"; then
    ok "CMake $CMAKE_VER ready"
else
    err "CMake >= $CMAKE_MIN still not available."
    info "Install manually: https://cmake.org/download/"
    info "  or on macOS: brew install cmake"
fi

# ── 4. CUDA Toolkit (optional) ──────────────────────────────────────────────

banner "CUDA Toolkit (optional)"

if $SKIP_OPTIONAL; then
    info "Skipped (--skip-optional)"
else
    if has_cmd nvcc; then
        NVCC_VER="$(nvcc --version 2>&1 | grep -i release | head -1)"
        ok "CUDA nvcc found — $NVCC_VER"
    else
        warn "CUDA Toolkit not found."
        info "GPU benchmarks will be disabled (CPU-only is fine)."
        info "To enable NVIDIA GPU benchmarks, install CUDA Toolkit:"
        if [[ "$OS" == "linux" ]]; then
            info "  → https://developer.nvidia.com/cuda-downloads"
            info "  → Ubuntu: sudo apt install nvidia-cuda-toolkit"
            info "  → Fedora: sudo dnf install cuda"
        elif [[ "$OS" == "macos" ]]; then
            info "  → CUDA is not supported on modern macOS (Apple Silicon)."
            info "  → Metal/MPS alternatives may be considered in the future."
        fi
    fi
fi

# ── 5. ROCm / HIP (optional, Linux AMD GPUs) ────────────────────────────────

banner "ROCm / HIP (optional)"

if $SKIP_OPTIONAL; then
    info "Skipped (--skip-optional)"
elif [[ "$OS" != "linux" ]]; then
    info "ROCm is Linux-only — skipping"
else
    if has_cmd hipcc; then
        HIP_VER="$(hipcc --version 2>&1 | head -1)"
        ok "HIP found — $HIP_VER"
    else
        warn "ROCm/HIP not found."
        info "To enable AMD GPU benchmarks, install ROCm:"
        info "  → https://rocm.docs.amd.com/en/latest/deploy/linux/quick_start.html"
    fi
fi

# ── 6. Ninja (optional) ─────────────────────────────────────────────────────

banner "Ninja (optional, faster builds)"

if has_cmd ninja; then
    ok "Ninja $(ninja --version 2>&1) found"
elif ! $SKIP_OPTIONAL; then
    info "Ninja not found — installing …"
    pkg_ninja
    if has_cmd ninja; then ok "Ninja installed"; fi
else
    info "Ninja not found (skipped — optional)"
fi

# ── 7. Summary ──────────────────────────────────────────────────────────────

banner "Summary"

printf "\n"
printf "  %-14s %s\n" "Tool" "Status"
printf "  %-14s %s\n" "--------------" "----------------------------"
printf "  %-14s %s\n" "Git"           "$(has_cmd git   && echo 'OK' || echo 'MISSING (optional)')"
printf "  %-14s %s\n" "CMake"         "$(get_cmake_version 2>/dev/null || echo 'MISSING')"
printf "  %-14s %s\n" "C Compiler"    "$($HAS_COMPILER && echo 'OK' || echo 'MISSING')"
printf "  %-14s %s\n" "CUDA nvcc"     "$(has_cmd nvcc  && echo 'OK' || echo 'Not found (optional)')"
printf "  %-14s %s\n" "ROCm hipcc"    "$(has_cmd hipcc && echo 'OK' || echo 'Not found (optional)')"
printf "  %-14s %s\n" "Ninja"         "$(has_cmd ninja && echo 'OK' || echo 'Not found (optional)')"
printf "\n"

# ── 8. Configure & Build ────────────────────────────────────────────────────

if ! $NO_BUILD; then
    banner "Configuring & Building"

    cd "$REPO_ROOT"

    PRESET="default"
    if ! has_cmd nvcc && ! has_cmd hipcc; then
        PRESET="no-gpu"
        info "Using 'no-gpu' preset (no GPU SDK available)"
    fi

    info "cmake --preset $PRESET"
    cmake --preset "$PRESET"

    BUILD_DIR="build"
    [[ "$PRESET" == "no-gpu" ]] && BUILD_DIR="build-cpu"

    info "cmake --build $BUILD_DIR"
    cmake --build "$BUILD_DIR"

    ok "Build succeeded!"

    info "Running tests …"
    cd "$BUILD_DIR"
    if ctest --output-on-failure; then
        ok "All tests passed!"
    else
        warn "Some tests failed — check output above."
    fi
    cd "$REPO_ROOT"
fi

printf "\n"
ok "Setup complete. Happy hacking!"
printf "\n"
