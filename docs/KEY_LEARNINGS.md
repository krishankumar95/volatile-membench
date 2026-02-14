# Key Learnings — Memory Hierarchies & Benchmarking

Lessons from building a memory benchmarking tool, written for computer scientists and systems engineers.

---

## Table of Contents

1. [The Memory Wall](#1-the-memory-wall)
2. [Cache Hierarchy Is Not What You Think](#2-cache-hierarchy-is-not-what-you-think)
3. [Measuring Latency Is Harder Than It Looks](#3-measuring-latency-is-harder-than-it-looks)
4. [Bandwidth vs. Latency — Two Different Worlds](#4-bandwidth-vs-latency--two-different-worlds)
5. [The Compiler Is Your Adversary](#5-the-compiler-is-your-adversary)
6. [Why Cache Line Stride Matters](#6-why-cache-line-stride-matters)
7. [Store Buffers Hide Write Latency](#7-store-buffers-hide-write-latency)
8. [Hardware Prefetchers Distort Everything](#8-hardware-prefetchers-distort-everything)
9. [Per-Core vs. Shared Caches](#9-per-core-vs-shared-caches)
10. [GPU Memory Is a Different Beast](#10-gpu-memory-is-a-different-beast)
11. [Signal Processing on Hardware Data](#11-signal-processing-on-hardware-data)
12. [Real Hardware Has Gradual Transitions](#12-real-hardware-has-gradual-transitions)
13. [Practical Takeaways for Software Engineers](#13-practical-takeaways-for-software-engineers)
14. [CPU Frequency Scaling Distorts First Measurements](#14-cpu-frequency-scaling-distorts-first-measurements)
15. [Apple Silicon Is a Different Architecture](#15-apple-silicon-is-a-different-architecture)
16. [Cache Line Size Is Not Universal](#16-cache-line-size-is-not-universal)
17. [OS APIs Lie About Cache Sizes](#17-os-apis-lie-about-cache-sizes)
18. [Benchmarking on Low-RAM Systems](#18-benchmarking-on-low-ram-systems)

---

## 1. The Memory Wall

The fundamental tension in modern computing:

```
CPU clock:     ~4 GHz  →  1 cycle = 0.25 ns
DRAM access:   ~50 ns  →  200 cycles idle
```

A single cache miss to DRAM costs **200 CPU cycles** of stalled execution. This gap has been growing since the 1980s and is the primary reason cache hierarchies exist.

**Implication**: Memory access patterns matter more than algorithmic complexity for many real-world workloads. An O(n log n) algorithm with good locality can outperform an O(n) algorithm that thrashes the cache.

---

## 2. Cache Hierarchy Is Not What You Think

What textbooks show:

```
L1 → L2 → L3 → DRAM     (clean steps)
```

What real hardware looks like:

```
L1 (0.9 ns) → L2 (2.7 ns) → transition zone → L3 (12 ns) → gradual slope → DRAM (50 ns)
```

**Key insight**: Transitions between cache levels are not sharp cliffs — they are gradual slopes spanning 2–4× the cache size. This happens because:

- **Set associativity**: A 512 KB 8-way associative L2 doesn't evict everything at 513 KB. Conflict misses start gradually as the working set grows.
- **Replacement policies**: LRU (or pseudo-LRU) means some hot lines survive even when the working set exceeds the cache.
- **TLB effects**: At certain sizes, TLB misses add latency independently of cache misses.

**Measured on AMD Ryzen 7 5800X3D (32 KB L1d, 512 KB L2, 96 MB L3)**:

| Buffer Size | Latency | What's Happening |
|-------------|---------|------------------|
| 16 KB | 0.9 ns | Fully in L1 |
| 32 KB | 0.9 ns | Still fits in L1 (exact boundary) |
| 38 KB | 2.7 ns | Spills to L2 — sharp jump |
| 362 KB | 3.0 ns | Comfortable in L2 |
| 430 KB | 3.5 ns | Starting to spill to L3 |
| 512 KB | 3.6 ns | At L2 boundary — not all misses yet |
| 1 MB | 10 ns | Fully in L3 |
| 32 MB | 13.5 ns | Still in L3 |
| 64 MB | 28 ns | Spilling to DRAM |
| 256 MB | 52 ns | Fully in DRAM |

Notice: latency at 512 KB is **3.6 ns**, not 12 ns. The L2→L3 transition is gradual. The working set must exceed the cache by ~2× before you see the full next-level latency.

---

## 3. Measuring Latency Is Harder Than It Looks

### The Pointer-Chase Technique

The gold-standard method:

```c
// Build a random cyclic linked list in the buffer
// Each node is one cache line apart (64 bytes)
for (iterations) {
    p = *p;    // follow the chain — each load depends on the previous
}
```

**Why this works**:
- Each load depends on the previous result → CPU cannot execute them in parallel (instruction-level parallelism is defeated).
- Random ordering defeats hardware prefetchers (they can only predict sequential or strided patterns).
- Cyclic chain means every node is visited exactly once per traversal.

**Why simpler approaches fail**:
- Sequential reads: prefetcher hides latency (measures bandwidth, not latency).
- Random array indexing with independent indices: out-of-order CPU executes 6+ loads simultaneously (measures 1/6th of true latency).
- `timeGetTime` / `clock()`: resolution too low (milliseconds vs. sub-nanosecond events).

### You Need a High-Resolution Timer

```c
// Windows: QueryPerformanceCounter (~100 ns resolution)
// Linux: clock_gettime(CLOCK_MONOTONIC, ...) (~1 ns resolution)
// x86: rdtsc (cycle-accurate, but needs calibration)
```

Timer overhead (~20-100 ns) must be amortized over millions of accesses. For a 0.9 ns L1 measurement, you need at minimum 200 million accesses to get the timer overhead below 0.01%.

---

## 4. Bandwidth vs. Latency — Two Different Worlds

They measure fundamentally different things:

| | Latency | Bandwidth |
|---|---------|-----------|
| **Pattern** | Random (pointer chase) | Sequential (streaming) |
| **Unit** | Nanoseconds per access | Gigabytes per second |
| **Optimized by** | Larger caches, lower CAS latency | More channels, wider buses, prefetching |
| **Real-world analogy** | Ping time | Download speed |

**Critical insight**: Modern CPUs are bandwidth-optimized, not latency-optimized. DDR5 doubled bandwidth vs. DDR4 but actually *increased* latency. This is a deliberate design tradeoff — most workloads are streaming (benefiting from bandwidth) rather than pointer-chasing (needing low latency).

**Measured**:

```
CPU L1 Read BW:    129 GB/s   (bandwidth is enormous in-cache)
CPU DRAM Read BW:   28 GB/s   (4.6× drop)
CPU DRAM Latency:   52 ns     (58× slower than L1)
```

Bandwidth degrades ~4.6×, but latency degrades ~58×. This asymmetry is why latency-sensitive code (hash tables, tree traversals, linked lists) suffers far more from cache misses than bandwidth-sensitive code (matrix multiplication, image processing).

### Read vs. Write Bandwidth Asymmetry on Zen 3

Our large-buffer (1–10 GB) sweeps reveal a striking read/write gap:

```
CPU DRAM Read BW:   ~28 GB/s   (consistent across 256 MB – 10 GB)
CPU DRAM Write BW:  ~20.8 GB/s (consistent across 1 – 10 GB)
CPU DRAM Write BW:  ~10.8 GB/s (at 64 – 256 MB)
```

This is **not a benchmark artifact** — it reflects how AMD Zen 3's memory controller is architected:

1. **Read-optimized prefetch pipeline**: Zen 3's prefetchers aggressively fill cache lines ahead of sequential reads, keeping the memory bus saturated. Writes don't benefit from this — the CPU must first read-for-ownership (RFO) each cache line before writing to it.

2. **Write-combining and store buffer drain**: Writes pass through the store buffer and must be coalesced into full cache-line writes before going to DRAM. This write-combining process adds overhead that doesn't exist for reads. The memory controller prioritizes reads over writes when both compete for bus time.

3. **Single-CCX, dual-channel DDR4 limits**: The 5800X3D has one CCX connected to a dual-channel DDR4-3200 controller. The theoretical peak is ~51.2 GB/s (2 × 3200 MT/s × 8 bytes). Reads achieve ~55% of theoretical; writes achieve ~40% — a ratio consistent with AMD's published specifications and independent STREAM benchmark results on Zen 3.

4. **Buffer size effect on writes**: The jump from ~10.8 GB/s (64–256 MB) to ~20.8 GB/s (1–10 GB) for writes is due to iteration count scaling. Smaller buffers with more iterations spend proportionally more time in store buffer stalls and cache writeback bursts, while the very large single-pass buffers allow the write-combining logic to reach steady-state throughput.

**Takeaway**: On Zen 3 (and most modern CPUs), expect DRAM write bandwidth to be 25–40% lower than read bandwidth. This is a fundamental property of the memory controller, not something software can overcome.

---

## 5. The Compiler Is Your Adversary

### The 0 ns Latency Bug

The first version of our pointer chase measured **0.00 ns** for read latency. Why?

```c
// Original code
for (uint64_t i = 0; i < iterations; i++) {
    p = *(void **)p;   // MSVC optimized this entire loop away
}
```

MSVC determined that `p` was never used after the loop (its final value was discarded), so it eliminated the entire loop. **The benchmark measured nothing**.

### The Fix: Volatile Semantics

```c
static inline void **chase_load(void **p) {
    return (void **)(*(void * volatile *)p);
}

for (uint64_t i = 0; i < iterations; i++) {
    p = chase_load(p);   // volatile forces each load to actually execute
}
```

The `volatile` cast tells the compiler: "This memory access has observable side effects — do not optimize it away, reorder it, or combine it with other accesses."

### Lesson

**Never trust benchmark results of 0 or near-0**. Always verify with a known-good reference (e.g., L1 latency should be ~1 ns on modern x86). If your benchmark runs "too fast," the compiler probably outsmarted you.

This is not a theoretical concern — it happens with **every major compiler** (GCC, Clang, MSVC) at optimization levels ≥ O1.

---

## 6. Why Cache Line Stride Matters

### The 8× Dilution Bug

Our second attempt measured DRAM latency as **~11 ns** instead of the expected ~50 ns.

**Root cause**: Pointer nodes were 8 bytes apart (one `void*`), but cache lines are 64 bytes:

```
Cache line (64 bytes):
[ptr0][ptr1][ptr2][ptr3][ptr4][ptr5][ptr6][ptr7]
  ↑                                              
  Only this fetch causes a cache miss;
  the next 7 dereferences hit the same line (free)
```

8 pointer dereferences shared one cache line. Only 1 in 8 actually caused a cache miss. Measured latency = (1 miss × 50 ns + 7 hits × 1 ns) / 8 ≈ **7.1 ns**. Our measured 11 ns was consistent with this dilution.

### The Fix: Cache-Line Stride

```c
#define CACHE_LINE_BYTES  64
#define PTRS_PER_LINE     (CACHE_LINE_BYTES / sizeof(void *))  // = 8

// Place each node at buf[i * PTRS_PER_LINE] (64 bytes apart)
buf[idx[i] * PTRS_PER_LINE] = &buf[idx[i+1] * PTRS_PER_LINE];
```

After fixing: DRAM latency = **50 ns** ✓

### Lesson

**The unit of transfer in the memory system is the cache line, not the byte or word**. Any benchmark that doesn't account for cache line size (typically 64 bytes on x86, 128 bytes on Apple Silicon) will produce misleading results.

This applies to data structure design too: two fields accessed together should be in the same cache line (spatial locality); two fields accessed by different threads should be in *different* cache lines (avoiding false sharing).

---

## 7. Store Buffers Hide Write Latency

### The 1.85 ns DRAM Write Bug

Our write latency benchmark initially showed **1.85 ns** even for DRAM-sized buffers. Real DRAM write latency should be ~50 ns.

**Root cause**: Independent stores go through the **store buffer**, a small hardware queue (typically 56-72 entries on modern x86) that decouples store execution from cache/memory updates:

```c
// Original write benchmark (WRONG)
for (uint64_t i = 0; i < iterations; i++) {
    buf[random_index] = value;   // goes into store buffer, returns immediately
}
```

The CPU retired each store in ~2 ns (store buffer throughput), never stalling for the actual DRAM write. The store buffer absorbed all writes and drained them asynchronously.

### The Fix: Dependent Read-Write Chain

```c
// Correct approach: each write depends on the previous read
for (uint64_t i = 0; i < iterations; i++) {
    // Write to scratch word in current cache line
    volatile size_t *scratch = (volatile size_t *)((char *)p + sizeof(void *));
    *scratch = (size_t)p;

    // Follow the pointer chain (read depends on being at this address)
    p = chase_load(p);
}
```

Each write is to the *current* node (determined by the previous read), and the next read is from the address loaded in the previous step. This creates a true dependency chain: read → write → read → write. The store cannot be absorbed because the next read depends on being at the correct address.

After fixing: DRAM write latency = **51 ns** ✓

### Lesson

**Store buffers make independent writes appear nearly free**. To measure true write latency, you must create a data dependency between writes (each write's address depends on a previous read). This is the same principle as the pointer chase for reads, but extended to include writes in the dependency chain.

---

## 8. Hardware Prefetchers Distort Everything

Modern CPUs have 3-4 levels of hardware prefetchers:

1. **Next-line prefetcher**: Fetches the adjacent cache line.
2. **Stride prefetcher**: Detects constant-stride access patterns.
3. **Stream prefetcher**: Detects sequential access and prefetches ahead.
4. **L2 spatial prefetcher**: Fetches the pair cache line in the same 128-byte sector.

These prefetchers can hide latency by fetching data before the CPU requests it. For bandwidth measurement, this is correct behavior — streaming bandwidth *should* include prefetcher benefit. For latency measurement, prefetchers distort the result.

**How the pointer chase defeats prefetchers**:
- Random ordering → no stride to detect.
- Dependent loads → next address unknown until current load completes.
- Non-sequential addresses → stream detector finds no pattern.

**Why bandwidth tests should NOT use pointer chase**:
- Sequential streaming is the intended use case for bandwidth.
- Prefetchers legitimately contribute to real-world streaming throughput.
- Pointer-chase bandwidth would measure something no real program does.

---

## 9. Per-Core vs. Shared Caches

Modern CPU cache hierarchy:

```
Core 0                Core 1               Core N
┌──────────┐         ┌──────────┐         ┌──────────┐
│ L1d: 32KB│         │ L1d: 32KB│         │ L1d: 32KB│  ← Private per-core
│ L1i: 32KB│         │ L1i: 32KB│         │ L1i: 32KB│
│ L2: 512KB│         │ L2: 512KB│         │ L2: 512KB│  ← Private per-core
└────┬─────┘         └────┬─────┘         └────┬─────┘
     └──────────┬─────────┴───────────────────┘
           ┌────┴────┐
           │L3: 96 MB│  ← Shared across all cores
           └────┬────┘
                │
           ┌────┴────┐
           │  DRAM   │  ← Shared, multi-channel
           └─────────┘
```

**Why this matters for benchmarking**:

If the OS migrates your benchmark thread from Core 0 to Core 1 mid-test:
- L1 and L2 are cold on Core 1 → sudden latency spike.
- All "warm" data is in Core 0's private caches → useless.
- L3 data is shared, so L3-sized tests are unaffected.

**Solution**: Pin the benchmark thread to a specific core:
```c
// Windows
SetThreadAffinityMask(GetCurrentThread(), 1);  // pin to core 0

// Linux
cpu_set_t mask;
CPU_ZERO(&mask);
CPU_SET(0, &mask);
sched_setaffinity(0, sizeof(mask), &mask);
```

**Implication for cache size detection**: A single-threaded benchmark should see the full per-core L1d and L2, plus the full shared L3. Multi-threaded benchmarks would see contention on L3 and reduced effective L3 per thread.

---

## 10. GPU Memory Is a Different Beast

### GPU vs CPU Memory Architecture

| Property | CPU | GPU (NVIDIA) |
|----------|-----|-------------|
| Cores | 8-64 | 10,000+ (CUDA cores) |
| L1 Cache | 32-64 KB per core | 128-256 KB per SM (shared mem + L1) |
| L2 Cache | 256 KB - 2 MB per core | 4-96 MB total (shared) |
| Main Memory | DDR4/DDR5 | GDDR6X/GDDR7/HBM |
| Bus Width | 64-bit × 2 channels | 256-384 bits |
| Bandwidth | 30-80 GB/s | 500-3000 GB/s |
| Latency | 50-100 ns (DRAM) | 100-200 ns (VRAM) |

**Key differences**:
1. **Bandwidth over latency**: GPU hides latency through massive parallelism (thousands of threads in flight), not through low-latency caches.
2. **Flat latency curve**: A pointer-chase on GPU shows ~110 ns regardless of buffer size — there's no usable cache hierarchy from a single-thread perspective.
3. **Coalescing matters more than caching**: GPU memory controllers merge adjacent accesses from threads in the same warp. This means access *pattern* (coalesced vs. scattered) affects bandwidth more than working set *size*.

### Measured on NVIDIA RTX 5080 (16 GB GDDR7, 256-bit bus)

```
GPU Latency:     ~108 ns (flat across 1 MB – 32 MB)
GPU Read BW:     806 GB/s @ 10 GB,  798 GB/s @ 4 GB  (VRAM-bound)
                1381 GB/s @ 16 MB                     (L2 cache effect)
GPU Write BW:    767 GB/s @ 10 GB,  801 GB/s @ 1 GB  (VRAM-bound)
                 578 GB/s @ 16 MB                     (write path overhead)
Theoretical:     960 GB/s
```

**Key observations from the 1–10 GB sweep**:

1. **Read BW converges to ~806 GB/s** (84% of theoretical) at multi-GB sizes, where the L2 cache (64 MB) holds a negligible fraction of the working set. This is the true VRAM bandwidth floor.

2. **The 16 MB anomaly**: Read BW at 16 MB (1381 GB/s) exceeds theoretical VRAM bandwidth — the data is served almost entirely from the 64 MB L2 cache rather than VRAM.

3. **Write BW plateaus lower at ~767 GB/s** for large buffers. Similar to CPU, GPU write paths incur extra overhead from read-modify-write cycles at the memory controller level.

4. **All sizes fit in 16 GB VRAM**: The 10 GB test confirms that allocation succeeds and bandwidth remains stable. Sizes exceeding VRAM would fail `cudaMalloc` and be silently skipped.

---

## 11. Signal Processing on Hardware Data

Detecting cache sizes from a latency curve is a **signal processing problem**, not a simple threshold comparison.

### What Doesn't Work

**Naive approach**: "Find the biggest ratio jump between consecutive samples."
- Fails because the transitions span multiple samples.
- Noise from timer quantization creates false jumps.
- Gradual transitions (L3→DRAM) have no single "biggest jump."

**Plateau-end detection**: "Find flat regions and mark where they end."
- Systematically underestimates because the derivative's forward-looking window detects transitions early.
- Works for sharp transitions (L1→L2) but fails for gradual ones (L3→DRAM).

### What Works: Peak-Finding on the Derivative

The approach that produces accurate results:

1. **Log-transform**: Work in log-log space (log-latency vs. log-size). This makes multiplicative changes (2× vs 3× latency) into additive ones, normalizing the signal.

2. **Median filter** (radius 3): Removes outliers while preserving step edges. Mean filtering would blur the transitions; median filtering keeps them sharp.

3. **Derivative**: Compute d(log_latency)/d(log_size). Peaks in this derivative correspond to transitions between cache levels.

4. **Second median filter on derivative**: Cleans noise in the derivative signal.

5. **Peak detection**: Find local maxima — these are the transition centers.

6. **Geometric-mean crossing**: For each transition, compute:
   ```
   threshold = sqrt(lower_plateau_latency × upper_plateau_latency)
   ```
   Then find where the raw latency crosses this threshold. The geometric mean is the right choice (not arithmetic mean) because latencies are log-normally distributed.

7. **Log-interpolation**: Between the two samples straddling the threshold, interpolate in log-space for sub-sample precision.

### Lesson

Hardware measurement data requires the same rigor as any other signal processing task. Smoothing, differentiation, peak detection, and interpolation are not overkill — they're necessary to extract reliable information from noisy physical measurements.

---

## 12. Real Hardware Has Gradual Transitions

### AMD 3D V-Cache (5800X3D) as a Case Study

The Ryzen 7 5800X3D has **96 MB of L3 cache** via 3D V-Cache technology:
- Base die: 32 MB L3 (lower latency, ~12 ns)
- Stacked die: 64 MB L3 (slightly higher latency, ~13-14 ns)

This creates a **two-slope L3 region** in the latency curve:

```
Size:     4 MB    32 MB    45 MB    64 MB    90 MB    128 MB    256 MB
Latency:  13 ns   13.5 ns  15 ns    28 ns    46 ns    53 ns     53 ns
          ^^^^^^^^^^^^       ^^^^^^^^^^^^^^^^^         ^^^^^^^^^^^^^^^^^
          L3 plateau         L3→DRAM transition        DRAM plateau
```

The transition from L3 to DRAM spans **45 MB to 128 MB** — nearly a 3× range. There is no single "cache size boundary." Even professional tools (AIDA64, Intel Memory Latency Checker) show this same gradual slope.

**Implication**: Automated cache detection will always be approximate on modern hardware. Reporting "L3 = 60 MB" for a 96 MB V-Cache isn't wrong — it's where the geometric mean of the transition falls. The exact boundary depends on your definition of "boundary."

### Chiplet Architectures (AMD Zen 3/4/5)

Multi-chiplet designs introduce another wrinkle: L3 is per-CCD (Core Complex Die). A benchmark pinned to one core sees only that CCD's L3. Cross-CCD requests go through the Infinity Fabric, adding ~20 ns latency (appearing as a cache level that doesn't exist in the spec sheet).

---

## 13. Practical Takeaways for Software Engineers

### Data Structure Design

```
Good (cache-friendly):         Bad (cache-hostile):
┌─────────────────┐            struct Node {
│ Array of structs │              int key;
│ [A][B][C][D]... │              Node* left;     // 64 bytes apart from parent
│ Sequential access│              Node* right;    // random memory location
└─────────────────┘            };
   ~1 ns/access (L1)             ~50 ns/access (DRAM) on large trees
```

**Rule of thumb**: Arrays > linked structures. If you must use pointers, keep the working set small enough to fit in cache.

### What the Numbers Mean for Your Code

| If your hot loop's working set is... | Expected performance |
|---------------------------------------|---------------------|
| < 32 KB | L1-speed: ~1 ns/access, ~130 GB/s |
| < 512 KB | L2-speed: ~3 ns/access, ~100 GB/s |
| < 32 MB | L3-speed: ~13 ns/access, ~65 GB/s |
| > 64 MB | DRAM-speed: ~50 ns/access, ~28 GB/s read, ~21 GB/s write |

A hash table that fits in L2 (< 512 KB ≈ 32K entries of 16 bytes) will be **17× faster per lookup** than one that spills to DRAM.

### When to Worry About Memory Performance

1. **Pointer-chasing workloads**: Trees, graphs, linked lists, hash tables with chaining. Profile for cache miss rates (`perf stat -e cache-misses`).

2. **Large working sets**: Any data structure > L3 size. Consider tiling, blocking, or streaming.

3. **Multi-threaded shared data**: False sharing (two threads writing to the same cache line) costs 40-100 ns per access instead of 1 ns. Align shared-but-independent data to cache line boundaries.

4. **NUMA effects**: On multi-socket systems, accessing remote DRAM adds ~100 ns. Use `numactl` or equivalent to pin memory allocation.

### Mental Model

Think of your program's memory access as flowing through this pipeline:

```
Register (0 cycles) → L1 (4 cycles) → L2 (12 cycles) → L3 (40 cycles) → DRAM (200 cycles)
     100%               ~90% hit        ~95% hit          ~98% hit         guaranteed
```

Every level filters most accesses. The goal of cache-friendly code is to maximize hit rates at each level by:
- **Temporal locality**: Access the same data repeatedly before moving on.
- **Spatial locality**: Access nearby addresses (same cache line, adjacent lines).
- **Minimal footprint**: Use the smallest data representation that works.

---

## 14. CPU Frequency Scaling Distorts First Measurements

### The 3× Inflation Bug

When porting to Apple M1, the first benchmark (read latency) measured **2.86 ns** at 16 KB — but the same function called later during cache detection measured **0.94 ns** for the same buffer size. Write latency (which ran second) also showed 0.94 ns.

**Root cause**: Modern CPUs dynamically scale clock frequency based on load. The M1's P-cores can run from ~600 MHz to 3.2 GHz. When the benchmark starts after an idle period, the CPU is in a low-power state:

```
First measurement:    3 cycles @ ~1.05 GHz = 2.86 ns   ← CPU still waking up
Second measurement:   3 cycles @ ~3.20 GHz = 0.94 ns   ← CPU at full speed
```

The warmup phase (one traversal of 256 nodes ≈ 240 ns of work) was far too short to trigger frequency ramp-up. The CPU needs sustained load (~100+ ms) before reaching peak frequency.

### Evidence of Progressive Ramp-Up

The read latency sweep ran sequentially through 8 sizes. The first few showed decreasing latency — not because larger buffers are faster, but because the CPU was ramping up:

```
16 KB:  2.86 ns   (CPU @ ~1.05 GHz — just starting)
32 KB:  2.55 ns   (CPU @ ~1.18 GHz — warming up)
128 KB: 2.34 ns   (CPU @ ~1.28 GHz — still ramping)
```

After fixing (adding 200 ms warmup), all three measured 0.94–0.97 ns, as expected for L1.

### The Fix: Pre-Benchmark Frequency Warmup

```c
static void cpu_freq_warmup(void) {
    uint64_t start = membench_timer_ns();
    volatile uint64_t sink = 0;
    while (membench_timer_ns() - start < 200000000ULL) { /* 200 ms */
        for (int i = 0; i < 10000; i++)
            sink += (uint64_t)i * 37;
    }
    (void)sink;
}
```

A 200 ms busy-loop before any benchmark ensures the CPU is at peak frequency. This is called once at startup, not per-test.

### Lesson

**Always stabilize CPU frequency before benchmarking.** This affects ARM (Apple Silicon, Qualcomm) and x86 (Intel SpeedStep, AMD Cool'n'Quiet) alike. The pattern is insidious because:
- Each individual measurement looks plausible (2.86 ns is a "reasonable" number).
- The bug only appears when comparing against an independent measurement of the same thing.
- Per-test warmup is insufficient — the CPU needs sustained load (hundreds of milliseconds) to reach peak frequency.

Linux users can alternatively set the performance governor (`cpufreq-set -g performance`), but a software warmup is the only portable solution.

---

## 15. Apple Silicon Is a Different Architecture

### big.LITTLE Heterogeneous Cores

Apple M1 has two types of CPU cores with fundamentally different performance characteristics:

| Property | P-core (Firestorm) | E-core (Icestorm) |
|----------|-------------------|-------------------|
| Clock | 3.2 GHz | 2.06 GHz |
| L1d | 128 KB | 64 KB |
| L2 | 12 MB (shared, 4P-cores) | 4 MB (shared, 4E-cores) |
| L3 | None | None |
| Decode width | 8-wide | 4-wide |

A benchmark running on an E-core will show ~50% higher latency and different cache boundaries than one on a P-core. macOS schedules threads based on QoS class, not explicit affinity — there is no `sched_setaffinity` on macOS.

**Fix**: Set QoS class to `QOS_CLASS_USER_INTERACTIVE` before latency-sensitive benchmarks:

```c
#if defined(MEMBENCH_PLATFORM_MACOS)
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
```

This strongly encourages (but does not guarantee) P-core scheduling.

### No L3, But a System Level Cache (SLC)

Unlike x86 CPUs, the M1 has no traditional L3 cache. Instead, it has a ~16 MB System Level Cache (SLC) that sits between the memory controller and all requestors (CPU, GPU, Neural Engine, ISP). The SLC is not CPU-addressable in the same way as L3 — it acts more like a victim cache for the memory fabric.

**Measured impact**: After spilling out of L2 (12 MB), latency jumps directly to DRAM levels (~90 ns). There is no intermediate "L3" plateau as seen on x86:

```
AMD 5800X3D:    L1 (0.9 ns) → L2 (3 ns) → L3 (13 ns, holds to 96 MB) → DRAM (52 ns)
Apple M1:       L1 (0.95 ns) → L2 (5.5 ns) → DRAM (90+ ns, no L3 buffer)
```

For workloads with 12–96 MB working sets, the M1 is **7× slower per access** than a V-Cache-equipped x86 chip.

### DRAM Trade-offs: Bandwidth vs. Latency

M1 uses LPDDR4X in a unified memory architecture:

```
M1 LPDDR4X:    Read BW: 56 GB/s    Latency: 104 ns
5800X3D DDR4:  Read BW: 28 GB/s    Latency:  52 ns
```

Apple optimized for **bandwidth** (2× higher) at the cost of **latency** (2× higher). This is the right trade-off for a GPU-integrated SoC (GPU workloads are bandwidth-bound), but hurts CPU pointer-chasing workloads.

### Read/Write Bandwidth Symmetry

Unlike Zen 3's ~25–40% read/write gap, M1 shows nearly symmetric bandwidth:

```
M1:     Read 56 GB/s, Write 59 GB/s  (ratio: 0.95)
5800X3D: Read 28 GB/s, Write 21 GB/s (ratio: 1.33)
```

Apple's memory controller handles writes more efficiently, likely due to the unified memory architecture avoiding the CPU-side write-combining bottleneck seen in discrete DDR controllers.

---

## 16. Cache Line Size Is Not Universal

### The 64-Byte Assumption Is Wrong on Apple Silicon

The original benchmark hardcoded `CACHE_LINE_BYTES = 64`, which is correct for x86_64. On Apple Silicon, the cache line size is **128 bytes** (`sysctl hw.cachelinesize` returns 128).

With a 64-byte pointer-chase stride on a 128-byte-line system, two adjacent nodes occupy the same cache line. In a random permutation, the probability that consecutive-in-chain nodes share a line is low (~1/N), so the direct impact on small-buffer latency is minimal. However, for large buffers where cache misses dominate, a 64-byte stride means:

- Each cache miss fetches 128 bytes but only uses 64 → 50% of fetched data is wasted.
- The effective number of cache lines touched is halved, artificially improving hit rates.
- Bandwidth measurements overcount by up to 2× (streaming 64 bytes per access but the bus transfers 128).

### The Fix: Runtime Detection

```c
static size_t membench_get_cache_line_size(void) {
#if defined(MEMBENCH_PLATFORM_MACOS)
    size_t line = 0, sz = sizeof(line);
    if (sysctlbyname("hw.cachelinesize", &line, &sz, NULL, 0) == 0 && line > 0)
        return line;
#endif
    return 64;  /* x86_64 default */
}
```

The latency benchmark now spaces pointer-chase nodes one actual cache line apart, ensuring each dereference fetches a unique line regardless of architecture.

### Lesson

**Never hardcode microarchitectural constants.** Cache line sizes vary: 64 bytes (x86), 128 bytes (Apple Silicon), 32 bytes (some embedded ARM). Page sizes also vary (4 KB vs 16 KB on Apple Silicon). Detect at runtime or via build-system probing.

---

## 17. OS APIs Lie About Cache Sizes

### macOS Reports E-Core Values by Default

On Apple M1, the standard `sysctl` keys return **E-core** cache sizes:

```
hw.l1dcachesize = 65536    (64 KB — E-core L1d)
hw.l2cachesize  = 4194304  (4 MB — E-core cluster L2)
```

The P-core values are larger:

```
hw.perflevel0.l1dcachesize = 131072     (128 KB — P-core L1d)
hw.perflevel0.l2cachesize  = 12582912   (12 MB — P-core cluster L2)
```

And the E-core values match the "generic" ones:

```
hw.perflevel1.l1dcachesize = 65536      (64 KB — E-core L1d)
hw.perflevel1.l2cachesize  = 4194304    (4 MB — E-core cluster L2)
```

If you report "L1 = 64 KB" to the user while running benchmarks on a P-core with 128 KB L1, the cache detection results appear to overshoot by 2× — creating false doubt in the tool's accuracy.

### The Fix

Prefer `hw.perflevel0.*` (P-core) values when available, falling back to the generic keys:

```c
if (sysctlbyname("hw.perflevel0.l1dcachesize", &val, &sz, NULL, 0) != 0 || val == 0) {
    sysctlbyname("hw.l1dcachesize", &val, &sz, NULL, 0);  /* fallback */
}
```

This is correct because benchmarks should reference the core type they're actually running on, and QoS hints push benchmark threads to P-cores.

### Lesson

**OS-reported cache sizes may not match the core you're running on.** On heterogeneous (big.LITTLE) systems, always query per-performance-level values. This applies to Apple Silicon, Qualcomm Snapdragon (Cortex-A7xx + Cortex-A5xx), and Intel Alder Lake+ (P-cores + E-cores with different L2 sizes).

---

## 18. Benchmarking on Low-RAM Systems

### The Swap Cliff

On an 8 GB MacBook Air, the original default bandwidth sweep went up to 10 GB:

```
Buffer Size    Read BW
256 MB         55.46 GB/s   ← real DRAM bandwidth
1 GB           49.50 GB/s   ← still DRAM (but system under pressure)
4 GB            3.32 GB/s   ← measuring swap, not DRAM
8 GB            3.43 GB/s   ← mostly swapping
10 GB           3.39 GB/s   ← entirely swapping
```

At 4 GB (50% of physical RAM), macOS starts aggressively swapping to reclaim pages for the system. The "bandwidth" measured is SSD throughput (~3.4 GB/s on the M1's NVMe controller), not memory bandwidth.

### Why 75% Is Still Too High

Even 75% of RAM can cause issues. macOS, Windows, and Linux all reserve 2–4 GB for system processes, kernel caches, and file system buffers. On an 8 GB system:

- 75% = 6 GB → leaves 2 GB for the OS → marginal
- 50% = 4 GB → leaves 4 GB for the OS → safe

### The Fix

Skip default sweep sizes ≥ 50% of physical RAM:

```c
membench_sysinfo_t si = {0};
membench_sysinfo_get(&si);
size_t ram_limit = si.total_ram > 0 ? si.total_ram / 2 : (size_t)-1;

if (DEFAULT_BW_SIZES[i] >= ram_limit) {
    printf("  (skipping — exceeds 50%% of RAM)\n");
    break;
}
```

Users can still explicitly request large sizes via `--size 8G` if they know their system can handle it.

### Lesson

**Benchmark defaults must adapt to the target system.** A benchmark designed on a 64 GB workstation will produce garbage results on an 8 GB laptop if it doesn't check available memory. This is especially important for CI/CD environments where benchmark tests may run on constrained VMs.

---

## Further Reading

- Ulrich Drepper, ["What Every Programmer Should Know About Memory"](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf) (2007) — The definitive reference.
- Intel, ["Intel 64 and IA-32 Architectures Optimization Reference Manual"](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) — Chapter on memory hierarchy.
- Agner Fog, ["Microarchitecture of Intel, AMD, and VIA CPUs"](https://www.agner.org/optimize/microarchitecture.pdf) — Detailed cache/memory latency tables.
- John McCalpin, ["STREAM Benchmark"](https://www.cs.virginia.edu/stream/) — The standard memory bandwidth benchmark.
- Wikichip, CPU microarchitecture pages — Cache hierarchy diagrams for specific processors.
