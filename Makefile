# Volatile MemBench — Convenience Makefile
#
# Wraps CMake commands for common workflows.
# Works on Linux and macOS with GNU Make (or compatible).
# On Windows, use setup.ps1 or CMake commands directly.

.PHONY: all setup setup-minimal build build-release build-cpu test clean help

# Default target
all: build

# ── Full setup (deps + build + test) ─────────────────────────────────────────

setup:
	@echo "=== Full Setup ==="
	@if [ -f scripts/setup.sh ]; then \
		chmod +x scripts/setup.sh && ./scripts/setup.sh; \
	else \
		echo "Error: scripts/setup.sh not found"; exit 1; \
	fi

setup-minimal:
	@echo "=== Minimal Setup (deps only, no build) ==="
	@if [ -f scripts/setup.sh ]; then \
		chmod +x scripts/setup.sh && ./scripts/setup.sh --skip-optional --no-build; \
	else \
		echo "Error: scripts/setup.sh not found"; exit 1; \
	fi

# ── Build targets ────────────────────────────────────────────────────────────

build:
	cmake --preset default
	cmake --build build

build-release:
	cmake --preset release
	cmake --build build-release

build-debug:
	cmake --preset debug
	cmake --build build-debug

build-cpu:
	cmake --preset no-gpu
	cmake --build build-cpu

# ── Test ─────────────────────────────────────────────────────────────────────

test: build
	cd build && ctest --output-on-failure

# ── Clean ────────────────────────────────────────────────────────────────────

clean:
	rm -rf build build-release build-debug build-cpu

# ── Help ─────────────────────────────────────────────────────────────────────

help:
	@echo "Volatile MemBench — Makefile targets"
	@echo ""
	@echo "  make setup          Full setup: install deps + configure + build + test"
	@echo "  make setup-minimal  Install required deps only, skip GPU/Ninja"
	@echo "  make build          Configure (default preset) + build"
	@echo "  make build-release  Configure (release preset) + build"
	@echo "  make build-debug    Configure (debug preset) + build"
	@echo "  make build-cpu      Configure (no-gpu preset) + build"
	@echo "  make test           Build + run CTest"
	@echo "  make clean          Remove all build directories"
	@echo "  make help           Show this message"
	@echo ""
	@echo "Windows: use scripts\\setup.ps1 or CMake commands directly."
