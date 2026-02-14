# Benchmark Results

Volatile MemBench measurements across different hardware platforms. All results collected using the same codebase with platform-appropriate settings (runtime cache line detection, P-core sysinfo, frequency warmup).

---

## Systems Tested

| | Apple M1 (MacBook Air) | AMD Ryzen 7 5800X3D (Desktop) |
|---|---|---|
| **Architecture** | ARM64 (Apple Silicon) | x86_64 (Zen 3 + 3D V-Cache) |
| **Clock** | 3.2 GHz (P-core) / 2.06 GHz (E-core) | ~4.5 GHz (boost) |
| **Cores** | 4P + 4E = 8 | 8C / 16T |
| **L1 Data** | 128 KB per P-core / 64 KB per E-core | 32 KB per core |
| **L2** | 12 MB shared (P-cluster) / 4 MB (E-cluster) | 512 KB per core |
| **L3** | — (no traditional L3) | 96 MB (3D V-Cache) |
| **SLC** | ~16 MB System Level Cache | — |
| **RAM** | 8 GB LPDDR4X (unified) | DDR4-3200 (dual-channel) |
| **Theoretical BW** | ~68 GB/s | ~51.2 GB/s |
| **Cache Line** | 128 bytes | 64 bytes |
| **GPU** | Integrated (7-core, no CUDA) | NVIDIA RTX 5080 (16 GB GDDR7) |
| **OS** | macOS 26.3 | Windows |

---

## 1. Read Latency (Pointer-Chase)

Random-access latency measured via cache-line-stride pointer chase with `volatile` loads.

| Buffer Size | Apple M1 (ns) | Ryzen 5800X3D (ns) | Memory Tier |
|-------------|:---:|:---:|---|
| 16 KB | 0.95 | 0.9 | L1 (both) |
| 32 KB | 0.97 | 0.9 | L1 (both) |
| 38 KB | — | 2.7 | 5800X3D L1→L2 spill |
| 128 KB | 0.97 | 2.7 | M1 still L1; 5800X3D in L2 |
| 362 KB | — | 3.0 | 5800X3D comfortable in L2 |
| 512 KB | 5.50 | 3.6 | M1 in L2; 5800X3D at L2 boundary |
| 1 MB | — | 10.0 | 5800X3D in L3 |
| 4 MB | 6.35 | 10.0 | M1 still L2 (12 MB); 5800X3D in L3 |
| 32 MB | 90.27 | 13.5 | M1 DRAM; 5800X3D still L3 (96 MB) |
| 64 MB | 97.24 | 28.0 | M1 deep DRAM; 5800X3D L3→DRAM spill |
| 256 MB | 104.25 | 52.0 | Both DRAM |

### Analysis

- **L1 latency is a tie** (~1 ns on both), but **M1's L1 is 4× larger** (128 KB vs 32 KB). The 5800X3D spills to L2 at ~38 KB; M1 stays in L1 through 128 KB.
- **L2 latency**: M1 is slower (5.5 ns vs 3.0 ns) — larger shared L2 with higher access latency.
- **The L3 chasm**: The 5800X3D's 96 MB V-Cache is the defining difference. At 32 MB, the 5800X3D is at 13.5 ns (L3) while M1 is at 90 ns (DRAM) — a **6.7× gap**.
- **DRAM latency**: M1 is 2× higher (104 ns vs 52 ns), the trade-off for LPDDR4X's lower power and higher bandwidth.

---

## 2. Write Latency (Dependent Read-Write Chain)

Each access writes a scratch word then follows the pointer chain, creating a serialized read→write→read dependency.

| Buffer Size | Apple M1 (ns) | Ryzen 5800X3D (ns) | Memory Tier |
|-------------|:---:|:---:|---|
| 16 KB | 0.94 | — | L1 |
| 32 KB | 0.94 | — | L1 |
| 128 KB | 0.94 | — | L1 |
| 512 KB | 6.26 | — | L2 |
| 4 MB | 6.69 | — | L2 |
| 32 MB | 92.02 | — | DRAM |
| 64 MB | 100.32 | — | DRAM |
| 256 MB | 110.11 | 51.0 | DRAM |

### Analysis

- **Write ≈ read at L1/L2** on M1 — the store to a scratch word within the same cache line completes in the store buffer without stalling.
- **DRAM write is ~6% slower than read** on M1 (110 ns vs 104 ns) due to read-for-ownership overhead.
- On the 5800X3D, DRAM write latency (~51 ns) is comparable to read (~52 ns) at the same buffer size.

---

## 3. Read Bandwidth (Sequential Streaming)

Sequential 64-bit loads across the buffer, measuring sustained GB/s.

| Buffer Size | Apple M1 (GB/s) | Ryzen 5800X3D (GB/s) | Memory Tier |
|-------------|:---:|:---:|---|
| 16 KB | 100.45 | 129 | L1 |
| 32 KB | 96.51 | — | L1 |
| 128 KB | 94.04 | — | L1/L2 |
| 512 KB | 80.43 | — | L2 |
| 4 MB | 79.99 | — | L2 |
| 32 MB | 40.45 | — | DRAM (see note) |
| 64 MB | 58.40 | — | DRAM |
| 256 MB | 56.39 | 28 | DRAM |
| 1 GB | 56.50 | 28 | DRAM |
| 4 GB | — | 28 | DRAM (M1 skipped: >50% of 8 GB RAM) |
| 10 GB | — | 28 | DRAM |

### Analysis

- **M1 DRAM read BW ≈ 56 GB/s** — 2× the 5800X3D's 28 GB/s, thanks to the unified memory architecture's wide bus.
- **L1 BW**: 5800X3D wins (129 vs 100 GB/s) due to higher clock speed (4.5 vs 3.2 GHz).
- **32 MB dip on M1**: The 40 GB/s measurement at 32 MB is an anomalous transient; surrounding sizes show steady ~56-58 GB/s.
- **M1 LPDDR4X efficiency**: 56 / 68 GB/s theoretical = **82%** utilization.
- **5800X3D DDR4 efficiency**: 28 / 51.2 GB/s theoretical = **55%** utilization.

---

## 4. Write Bandwidth (Sequential Streaming)

Sequential 64-bit stores across the buffer.

| Buffer Size | Apple M1 (GB/s) | Ryzen 5800X3D (GB/s) | Memory Tier |
|-------------|:---:|:---:|---|
| 16 KB | 72.52 | — | L1 |
| 32 KB | 71.30 | — | L1 |
| 128 KB | 61.68 | — | L2 |
| 512 KB | 60.85 | — | L2 |
| 4 MB | 58.97 | — | L2 |
| 32 MB | 59.21 | — | DRAM |
| 64 MB | 59.62 | — | DRAM |
| 256 MB | 58.45 | 20.8 | DRAM |
| 1 GB | 59.48 | 20.8 | DRAM |
| 4 GB | — | 20.8 | DRAM |
| 10 GB | — | 20.8 | DRAM |

### Analysis

- **M1 write BW ≈ 59 GB/s** — nearly 3× the 5800X3D's ~21 GB/s.
- **Read/write asymmetry** is dramatically different:
  - M1: 56 read / 59 write = **1.05 ratio** (nearly symmetric)
  - 5800X3D: 28 read / 21 write = **1.33 ratio** (reads 33% faster than writes)
- Apple's unified memory controller avoids the write-combining bottleneck seen in discrete DDR controllers.
- **M1 write BW is remarkably flat** (59–72 GB/s across all sizes), suggesting the write path is consistently optimized.

---

## 5. GPU Benchmarks (5800X3D + RTX 5080 Only)

M1 has no discrete GPU and no CUDA support. GPU benchmarks ran only on the 5800X3D system with an NVIDIA RTX 5080 (16 GB GDDR7, 256-bit bus, 960 GB/s theoretical).

### GPU Latency

| Buffer Size | RTX 5080 (ns) |
|-------------|:---:|
| 1 MB | ~108 |
| 4 MB | ~108 |
| 32 MB | ~108 |

GPU VRAM latency is **flat** (~108 ns) regardless of buffer size — there is no visible cache hierarchy from a single-thread pointer chase.

### GPU Bandwidth

| Buffer Size | Read (GB/s) | Write (GB/s) | Notes |
|-------------|:---:|:---:|---|
| 16 MB | 1381 | 578 | L2 cache effect (64 MB L2) |
| 1 GB | — | 801 | VRAM |
| 4 GB | 798 | — | VRAM |
| 10 GB | 806 | 767 | VRAM steady state |

- **Read BW converges to ~806 GB/s** (84% of 960 GB/s theoretical) at multi-GB sizes.
- **The 16 MB anomaly**: 1381 GB/s exceeds theoretical VRAM bandwidth — data served from the 64 MB L2 cache.
- **Write BW plateaus at ~767 GB/s** — GPU write paths incur read-modify-write overhead at the memory controller.

---

## 6. Cache Hierarchy Detection

Automatic detection from the 77-point latency sweep using derivative peak-finding with geometric-mean crossing.

| | M1 Estimated | M1 Actual | 5800X3D Estimated | 5800X3D Actual |
|---|:---:|:---:|:---:|:---:|
| **L1d** | 142 KB | 128 KB (P-core) | ~34 KB | 32 KB |
| **L2** | 14.1 MB | 12 MB (P-cluster) | ~600 KB | 512 KB |
| **L3** | — | — (no L3) | ~60 MB | 96 MB (V-Cache) |

### M1 Latency Curve (Key Points)

```
  Size          Latency     Tier
  ──────────    ────────    ──────────────
  1–128 KB      0.94 ns     L1 Cache (flat plateau)
     ─── sharp L1→L2 transition ───
  152 KB        4.00 ns     Entering L2
  256 KB–2 MB   5.5 ns      L2 plateau
  4 MB          6.22 ns     L2 (near boundary)
     ─── gradual L2→DRAM transition ───
  8 MB         10.70 ns     Transition
  13.5 MB      21.60 ns     Transition
  19 MB        49.03 ns     Transition
     ─── DRAM plateau ───
  32 MB        89.93 ns     DRAM
  128 MB      101.93 ns     DRAM
  512 MB      106.54 ns     DRAM plateau (~106 ns)
```

### 5800X3D Latency Curve (Key Points)

```
  Size          Latency     Tier
  ──────────    ────────    ──────────────
  16–32 KB      0.9 ns      L1 Cache
     ─── sharp L1→L2 transition at ~38 KB ───
  38 KB         2.7 ns      Entering L2
  362–512 KB    3.0–3.6 ns  L2 plateau
     ─── gradual L2→L3 transition ───
  1 MB         10.0 ns      L3
  32 MB        13.5 ns      L3 (still fits in 96 MB V-Cache)
     ─── gradual L3→DRAM transition (45–128 MB) ───
  64 MB        28.0 ns      L3→DRAM spill
  256 MB       52.0 ns      DRAM plateau
```

---

## 7. Summary Comparison

| Metric | Apple M1 | Ryzen 5800X3D | Winner |
|--------|:---:|:---:|---|
| **L1 latency** | 0.95 ns | 0.9 ns | Tie |
| **L1 size** | 128 KB | 32 KB | **M1** (4×) |
| **L2 latency** | 5.5 ns | 3.0 ns | **5800X3D** |
| **L2 size** | 12 MB | 512 KB | **M1** (24×) |
| **L3 latency** | — | 13.5 ns | **5800X3D** (M1 has no L3) |
| **L3 size** | — | 96 MB | **5800X3D** |
| **DRAM latency** | 104 ns | 52 ns | **5800X3D** (2×) |
| **DRAM read BW** | 56 GB/s | 28 GB/s | **M1** (2×) |
| **DRAM write BW** | 59 GB/s | 21 GB/s | **M1** (2.8×) |
| **R/W BW symmetry** | 1.05 ratio | 1.33 ratio | **M1** |
| **GPU BW** | — | 806 GB/s | 5800X3D (discrete GPU) |
| **Power (TDP)** | ~10 W | ~105 W | **M1** (10×) |

### When Each System Wins

| Workload Pattern | Better System | Why |
|------------------|:---:|---|
| Working set < 128 KB | **M1** | Larger L1, similar latency |
| Working set 128 KB–12 MB | **Tie** | M1's larger L2 vs 5800X3D's lower L2 latency |
| Working set 12–96 MB | **5800X3D** | 96 MB L3 at 13 ns vs M1 DRAM at 90+ ns |
| Working set > 96 MB | **5800X3D** | Lower DRAM latency (52 vs 104 ns) |
| Streaming / bandwidth-bound | **M1** | 2–3× higher DRAM bandwidth |
| GPU compute | **5800X3D** | Discrete RTX 5080 (806 GB/s) |
| Battery-powered / portable | **M1** | 10W vs 105W TDP |
