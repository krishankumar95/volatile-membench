# ── tests/ CMakeLists.txt ────────────────────────────────────────────────────

# Simple test runner — each test is a standalone C file that returns 0 on success.

# ── Timer test ──
add_executable(test_timer test_timer.c)
target_link_libraries(test_timer PRIVATE membench_core)
add_test(NAME timer COMMAND test_timer)

# ── Allocator test ──
add_executable(test_alloc test_alloc.c)
target_link_libraries(test_alloc PRIVATE membench_core)
add_test(NAME alloc COMMAND test_alloc)

# ── Sysinfo test ──
add_executable(test_sysinfo test_sysinfo.c)
target_link_libraries(test_sysinfo PRIVATE membench_core)
add_test(NAME sysinfo COMMAND test_sysinfo)
