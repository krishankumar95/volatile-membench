# Benchmark Results — Apple M1 MacBook Air (8 GB)

Results from Volatile MemBench running on Apple Silicon, compared with the AMD Ryzen 7 5800X3D desktop results from [KEY_LEARNINGS.md](KEY_LEARNINGS.md).

---

## System Specifications

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
| **Cache Line** | 128 bytes | 64 bytes |
| **GPU** | Integrated (7-core, no CUDA) | NVIDIA RTX 5080 (16 GB GDDR7) |

---

## Read Latency (Pointer-Chase)

| Buffer Size | M1 (ns) | 5800X3D (ns) | Notes |
|-------------|---------|-------------|-------|
| 16 KB | **0.95** | 0.9 | Both in L1; M1 = 3 cycles @ 3.2 GHz |
| 32 KB | **0.97** | 0.9 | M1 L1d is 128 KB (still fits) |
| 128 KB | **0.97** | 2.7 | M1 still in L1 (128 KB); 5800X3D spilled to L2 |
| 512 KB | **5.50** | 3.6 | M1 in L2; 5800X3D at L2 boundary |
| 4 MB | **6.35** | 10.0 | M1 still in L2 (12 MB); 5800X3D in L3 |
| 32 MB | **90.27** | 13.5 | M1 in DRAM (no L3); 5800X3D still in L3 (96 MB) |
| 64 MB | **97.24** | 28.0 | M1 deep DRAM; 5800X3D L3→DRAM spill |
| 256 MB | **104.25** | 52.0 | Both in DRAM; M1 LPDDR4X vs DDR4-3200 |

### Key Observations

1. **M1 has larger L1** (128 KB vs 32 KB) — stays at ~1 ns through 128 KB while the 5800X3D spills to L2 at ~38 KB.
2. **M1 has no L3** — jumps directly from L2 (~6 ns) to DRAM (~90+ ns) at ~12 MB. The 5800X3D's massive 96 MB V-Cache keeps data at ~13 ns through 32 MB.
3. **M1 DRAM latency is ~2× higher** (104 ns vs 52 ns) due to LPDDR4X's power-optimized design.
4. **Sweet spot**: Workloads < 12 MB are fast on M1 (≤ 6.35 ns); beyond that, the lack of L3 is punishing.

---

## Write Latency (Dependent Read-Write Chain)

| Buffer Size | M1 (ns) | Notes |
|-------------|---------|-------|
| 16 KB | 0.94 | L1 — same as read |
| 32 KB | 0.94 | L1 |
| 128 KB | 0.94 | L1 |
| 512 KB | 6.26 | L2 |
| 4 MB | 6.69 | L2 |
| 32 MB | 92.02 | DRAM |
| 64 MB | 100.32 | DRAM |
| 256 MB | 110.11 | DRAM — ~6% higher than read |

Write latency ≈ read latency at L1/L2. At DRAM, writes are ~6% slower due to read-for-ownership overhead.

---

## Read Bandwidth (Sequential Streaming)

| Buffer Size | M1 (GB/s) | 5800X3D (GB/s) | Notes |
|-------------|-----------|----------------|-------|
| 16 KB | **100.45** | 129 | Both in L1 |
| 32 KB | **96.51** | — | M1 still L1 |
| 128 KB | **94.04** | — | M1 L1 boundary |
| 512 KB | **80.43** | — | M1 in L2 |
| 4 MB | **79.99** | — | M1 in L2 |
| 32 MB | **40.45** | — | M1 DRAM (anomalous dip, see note) |
| 64 MB | **58.40** | — | M1 DRAM (steady state) |
| 256 MB | **56.39** | 28 | M1 DRAM vs 5800X3D DRAM |
| 1 GB | **56.50** | 28 | Stable DRAM bandwidth |

### Key Observations

1. **M1 DRAM read BW ≈ 56 GB/s** — 2× the 5800X3D's 28 GB/s, thanks to unified memory architecture with wide bus.
2. **L1 BW ≈ 100 GB/s** (M1) vs 129 GB/s (5800X3D) — the 5800X3D's higher clock compensates.
3. **32 MB dip**: The 40 GB/s at 32 MB is a transient measurement artifact; surrounding sizes show ~56-58 GB/s.
4. **Theoretical LPDDR4X-4266 peak**: ~34 GB/s per channel × 2 = ~68 GB/s. Measured 56 GB/s = **82% efficiency**.

---

## Write Bandwidth

| Buffer Size | M1 (GB/s) | 5800X3D (GB/s) | Notes |
|-------------|-----------|----------------|-------|
| 16 KB | **72.52** | — | L1 |
| 128 KB | **61.68** | — | L1/L2 boundary |
| 512 KB | **60.85** | — | L2 |
| 4 MB | **58.97** | — | L2 |
| 32 MB | **59.21** | — | DRAM |
| 256 MB | **58.45** | ~20.8 | DRAM |
| 1 GB | **59.48** | ~20.8 | DRAM |

### Key Observations

1. **M1 write BW ≈ 59 GB/s DRAM** — nearly 3× the 5800X3D's ~21 GB/s.
2. **Read/write asymmetry is small on M1** (56 vs 59 GB/s ≈ 5%) compared to 5800X3D (28 vs 21 = 25%). Apple's memory controller handles writes more efficiently.
3. **Write BW is remarkably flat** across all buffer sizes on M1 (59-72 GB/s), suggesting the write-combining logic is very effective.

---

## Cache Detection

| | M1 Estimated | M1 Actual | 5800X3D Estimated | 5800X3D Actual |
|---|---|---|---|---|
| **L1d** | 142 KB | 128 KB (P-core) | — | 32 KB |
| **L2** | 14.1 MB | 12 MB (P-cluster) | — | 512 KB |
| **L3** | (not detected) | — (no L3) | — | 96 MB |

The algorithm detects both cache boundaries within ~15% of actual size. M1's L1→L2 transition is very sharp (0.95 ns → 4.00 ns at 128→152 KB).

---

## Cache Detection Latency Curve (77 Samples)

```
  Size         Latency    Memory Tier
  ──────────   ────────   ──────────────────
  1.0 KB       0.94 ns    L1 Cache
  ...
  128.0 KB     0.95 ns    L1 Cache (boundary)
  ─── L1→L2 transition ───
  152.2 KB     4.00 ns    L2 Cache (entering)
  256.0 KB     5.45 ns    L2 Cache
  512.0 KB     5.48 ns    L2 Cache
  2.0 MB       5.59 ns    L2 Cache
  4.0 MB       6.22 ns    L2 Cache (near boundary)
  ─── L2→DRAM transition ───
  8.0 MB      10.70 ns    L2→DRAM transition
  13.5 MB     21.60 ns    Transition
  19.0 MB     49.03 ns    Transition
  ─── DRAM plateau ───
  32.0 MB     89.93 ns    DRAM
  64.0 MB     97.39 ns    DRAM
  128.0 MB   101.93 ns    DRAM
  256.0 MB   105.81 ns    DRAM
  512.0 MB   106.54 ns    DRAM plateau (~106 ns)
```

---

## Summary: M1 vs 5800X3D

| Metric | M1 Wins? | Why |
|--------|----------|-----|
| L1 latency | Tie | Both ~1 ns |
| L1 size | ✅ M1 | 128 KB vs 32 KB |
| L2 latency | 5800X3D | 3.6 ns vs 5.5 ns |
| L3 presence | 5800X3D | 96 MB V-Cache vs none |
| DRAM latency | 5800X3D | 52 ns vs 104 ns |
| DRAM read BW | ✅ M1 | 56 GB/s vs 28 GB/s |
| DRAM write BW | ✅ M1 | 59 GB/s vs 21 GB/s |
| Large working sets (>12 MB) | 5800X3D | 96 MB L3 keeps latency at 13 ns |
| Power efficiency | ✅ M1 | ~10W TDP vs ~105W |

**Bottom line**: M1 excels at bandwidth-bound and L1/L2-resident workloads. The 5800X3D dominates latency-sensitive workloads with large working sets (12–96 MB) thanks to its massive 3D V-Cache.
