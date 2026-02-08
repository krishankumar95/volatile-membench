# ── Architecture Detection ───────────────────────────────────────────────────
# Sets: MEMBENCH_ARCH = X86_64 | ARM64 | UNKNOWN

# Use CMAKE_SYSTEM_PROCESSOR which CMake populates per-platform
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _arch)

if(_arch MATCHES "amd64|x86_64|x64")
    set(MEMBENCH_ARCH "X86_64")
elseif(_arch MATCHES "aarch64|arm64")
    set(MEMBENCH_ARCH "ARM64")
elseif(_arch MATCHES "x86|i[3-6]86")
    set(MEMBENCH_ARCH "X86")
else()
    set(MEMBENCH_ARCH "UNKNOWN")
    message(WARNING "Unknown architecture '${CMAKE_SYSTEM_PROCESSOR}' – arch-specific optimizations disabled")
endif()

message(STATUS "Detected architecture: ${MEMBENCH_ARCH}")
