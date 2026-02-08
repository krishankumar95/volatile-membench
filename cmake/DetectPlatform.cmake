# ── Platform Detection ───────────────────────────────────────────────────────
# Sets: MEMBENCH_PLATFORM = WINDOWS | LINUX | MACOS | UNKNOWN

if(WIN32)
    set(MEMBENCH_PLATFORM "WINDOWS")
elseif(APPLE)
    set(MEMBENCH_PLATFORM "MACOS")
elseif(UNIX)
    set(MEMBENCH_PLATFORM "LINUX")
else()
    set(MEMBENCH_PLATFORM "UNKNOWN")
    message(WARNING "Unknown platform – some features may not work")
endif()

message(STATUS "Detected platform: ${MEMBENCH_PLATFORM}")

# Platform-specific link libraries
if(MEMBENCH_PLATFORM STREQUAL "WINDOWS")
    # Nothing extra needed for VirtualAlloc / QPC
elseif(MEMBENCH_PLATFORM STREQUAL "LINUX")
    # Need -lrt for clock_gettime on older glibc, -lpthread for barriers
    find_library(RT_LIB rt)
    find_library(PTHREAD_LIB pthread)
elseif(MEMBENCH_PLATFORM STREQUAL "MACOS")
    # mach_absolute_time is in system libs, nothing extra needed
endif()
