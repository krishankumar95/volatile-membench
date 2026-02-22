---
layout: default
title: Volatile MemBench
---

<!-- Hero -->
<div class="hero">
  <div class="container">
    <h1>Measure your memory.<br><span>Every nanosecond.</span></h1>
    <p>A cross-platform C11 tool that reveals cache latency, DRAM bandwidth, and memory hierarchy boundaries — with sub-nanosecond precision.</p>
    <div class="hero-buttons">
      <a href="https://github.com/krishankumar95/volatile-membench" class="btn btn-primary">
        ★ View on GitHub
      </a>
      <a href="#quickstart" class="btn btn-secondary">
        Quick Start →
      </a>
    </div>

    <!-- Terminal demo -->
    <div class="terminal">
      <div class="terminal-bar">
        <div class="terminal-dot red"></div>
        <div class="terminal-dot yellow"></div>
        <div class="terminal-dot green"></div>
        <span>membench — interactive mode</span>
      </div>
<pre>
<span class="prompt">$</span> <span class="cmd">./membench</span>

  <span class="heading">╭──────────────────────────────────╮</span>
  <span class="heading">│      Volatile MemBench           │</span>
  <span class="heading">│   Memory Performance Benchmark   │</span>
  <span class="heading">╰──────────────────────────────────╯</span>

  <span class="label">?</span> <span class="highlight">Select target</span>
    <span class="value">●</span> CPU
      GPU
      Both (CPU + GPU)

  <span class="label">?</span> <span class="highlight">Select tests</span> <span class="label">(space to toggle)</span>
    <span class="value">[✓]</span> Latency
    <span class="value">[✓]</span> Bandwidth
    <span class="label">[ ]</span> Cache Detection
</pre>
    </div>
  </div>
</div>

<!-- Stats bar -->
<section>
  <div class="container">
    <div class="stat-row">
      <div class="stat">
        <div class="stat-value">&lt;1 ns</div>
        <div class="stat-label">L1 cache latency</div>
      </div>
      <div class="stat">
        <div class="stat-value">77</div>
        <div class="stat-label">sweep data points</div>
      </div>
      <div class="stat">
        <div class="stat-value">0</div>
        <div class="stat-label">external dependencies</div>
      </div>
      <div class="stat">
        <div class="stat-value">3</div>
        <div class="stat-label">platforms supported</div>
      </div>
    </div>
  </div>
</section>

<!-- Features -->
<section id="features">
  <div class="container">
    <div class="section-label">Features</div>
    <h2 class="section-title">What it measures</h2>
    <p class="section-desc">Every benchmark uses techniques that defeat hardware optimizations — randomized pointer chases, volatile loads, and memory fences — so you measure real hardware, not compiler artifacts.</p>

    <div class="features">
      <div class="feature-card">
        <div class="feature-icon">⚡</div>
        <h3>Read & Write Latency</h3>
        <p>Pointer-chase through randomized Hamiltonian cycles at cache-line stride. Volatile casts prevent compiler elimination. Defeats spatial and streaming prefetchers.</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">📊</div>
        <h3>Streaming Bandwidth</h3>
        <p>Sequential read/write throughput across every memory tier. RAM-aware — automatically skips sizes that would trigger swap on low-memory systems.</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">🔍</div>
        <h3>Cache Detection</h3>
        <p>Sweeps 77 buffer sizes, then applies signal processing — finite-difference derivatives and Gaussian smoothing — to find L1/L2/L3 boundaries automatically.</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">🖥️</div>
        <h3>GPU VRAM Benchmarks</h3>
        <p>CUDA (NVIDIA) and HIP (AMD) global memory latency and bandwidth. Gracefully skipped when no GPU SDK is available.</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">🎯</div>
        <h3>Interactive CLI</h3>
        <p>Arrow-key navigation, checkbox toggles, text input — all in pure C with ANSI escape codes. No arguments needed, just run it.</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">🔧</div>
        <h3>Runtime Adaptation</h3>
        <p>Detects cache line size (128B on Apple Silicon, 64B on x86), CPU frequency scaling, and P-core vs E-core topology at runtime.</p>
      </div>
    </div>
  </div>
</section>

<!-- Benchmark Results -->
<section id="results">
  <div class="container">
    <div class="section-label">Benchmark Data</div>
    <h2 class="section-title">Real results, two architectures</h2>
    <p class="section-desc">Measured on Apple M1 (ARM64, LPDDR4X) and AMD Ryzen 7 5800X3D (x86_64, 96 MB V-Cache). Same codebase, same methodology.</p>

    <!-- Latency chart -->
    <div class="chart-container">
      <h3>Read Latency by Buffer Size — Apple M1 vs AMD 5800X3D</h3>

      <div class="bar-row">
        <div class="bar-label mono">16 KB</div>
        <div class="bar-track">
          <div class="bar-fill bar-m1" style="width: 3%">
          </div>
        </div>
        <span class="bar-value-outside mono">0.95 ns <span style="color:var(--accent)">M1</span></span>
      </div>
      <div class="bar-row">
        <div class="bar-label mono"></div>
        <div class="bar-track">
          <div class="bar-fill bar-amd" style="width: 2.5%">
          </div>
        </div>
        <span class="bar-value-outside mono">0.9 ns <span style="color:var(--blue)">5800X3D</span></span>
      </div>

      <div style="height:12px"></div>

      <div class="bar-row">
        <div class="bar-label mono">512 KB</div>
        <div class="bar-track">
          <div class="bar-fill bar-m1" style="width: 8%">
          </div>
        </div>
        <span class="bar-value-outside mono">5.5 ns</span>
      </div>
      <div class="bar-row">
        <div class="bar-label mono"></div>
        <div class="bar-track">
          <div class="bar-fill bar-amd" style="width: 5%">
          </div>
        </div>
        <span class="bar-value-outside mono">3.6 ns</span>
      </div>

      <div style="height:12px"></div>

      <div class="bar-row">
        <div class="bar-label mono">32 MB</div>
        <div class="bar-track">
          <div class="bar-fill bar-m1" style="width: 86%">
          </div>
        </div>
        <span class="bar-value-outside mono">90 ns</span>
      </div>
      <div class="bar-row">
        <div class="bar-label mono"></div>
        <div class="bar-track">
          <div class="bar-fill bar-amd" style="width: 13%">
          </div>
        </div>
        <span class="bar-value-outside mono">13.5 ns</span>
      </div>

      <div style="height:12px"></div>

      <div class="bar-row">
        <div class="bar-label mono">256 MB</div>
        <div class="bar-track">
          <div class="bar-fill bar-m1" style="width: 100%">
          </div>
        </div>
        <span class="bar-value-outside mono">104 ns</span>
      </div>
      <div class="bar-row">
        <div class="bar-label mono"></div>
        <div class="bar-track">
          <div class="bar-fill bar-amd" style="width: 50%">
          </div>
        </div>
        <span class="bar-value-outside mono">52 ns</span>
      </div>
    </div>

    <!-- Summary cards -->
    <div class="results-grid" style="margin-top: 24px">
      <div class="result-card">
        <h3>⚡ Latency Highlights</h3>
        <table>
          <tr>
            <th>Metric</th><th>M1</th><th>5800X3D</th>
          </tr>
          <tr>
            <td>L1 latency</td>
            <td class="value-cell">0.95 ns</td>
            <td class="value-cell">0.9 ns</td>
          </tr>
          <tr>
            <td>L1 size</td>
            <td class="value-cell">128 KB</td>
            <td>32 KB</td>
          </tr>
          <tr>
            <td>L2 latency</td>
            <td>5.5 ns</td>
            <td class="value-cell">3.0 ns</td>
          </tr>
          <tr>
            <td>L3 size</td>
            <td>—</td>
            <td class="value-cell">96 MB</td>
          </tr>
          <tr>
            <td>DRAM latency</td>
            <td>104 ns</td>
            <td class="value-cell">52 ns</td>
          </tr>
        </table>
      </div>
      <div class="result-card">
        <h3>📊 Bandwidth Highlights</h3>
        <table>
          <tr>
            <th>Metric</th><th>M1</th><th>5800X3D</th>
          </tr>
          <tr>
            <td>DRAM read</td>
            <td class="value-cell">56 GB/s</td>
            <td>28 GB/s</td>
          </tr>
          <tr>
            <td>DRAM write</td>
            <td class="value-cell">59 GB/s</td>
            <td>21 GB/s</td>
          </tr>
          <tr>
            <td>R/W ratio</td>
            <td class="value-cell">1.05×</td>
            <td>1.33×</td>
          </tr>
          <tr>
            <td>GPU BW</td>
            <td>—</td>
            <td class="value-cell">806 GB/s</td>
          </tr>
          <tr>
            <td>Power (TDP)</td>
            <td class="value-cell">10 W</td>
            <td>105 W</td>
          </tr>
        </table>
      </div>
    </div>
  </div>
</section>

<!-- Platform support -->
<section>
  <div class="container">
    <div class="section-label">Compatibility</div>
    <h2 class="section-title">Runs where you do</h2>
    <p class="section-desc">Pure C11 with zero external dependencies. Runtime detection handles architecture differences automatically.</p>

    <div class="platform-grid">
      <div class="platform-card">
        <div class="os">🐧</div>
        <h3>Linux</h3>
        <p><span class="check">✓</span> x86_64 · <span class="check">✓</span> ARM64</p>
        <p>GCC 9+ / Clang 11+</p>
      </div>
      <div class="platform-card">
        <div class="os">🍎</div>
        <h3>macOS</h3>
        <p><span class="check">✓</span> x86_64 · <span class="check">✓</span> Apple Silicon</p>
        <p>Apple Clang (Xcode CLI)</p>
      </div>
      <div class="platform-card">
        <div class="os">🪟</div>
        <h3>Windows</h3>
        <p><span class="check">✓</span> x86_64</p>
        <p>MSVC 2019+ / Visual Studio</p>
      </div>
    </div>
  </div>
</section>

<!-- Quick Start -->
<section id="quickstart">
  <div class="container">
    <div class="section-label">Get Started</div>
    <h2 class="section-title">Three commands to benchmark</h2>

    <div class="code-block">
<pre><span class="comment"># Clone and build</span>
<span class="cmd">git clone https://github.com/krishankumar95/volatile-membench.git
cd volatile-membench
cmake --preset default && cmake --build build</span>

<span class="comment"># Run interactively (guided menu with arrow keys)</span>
<span class="cmd">./build/src/membench</span>

<span class="comment"># Or with arguments</span>
<span class="cmd">./build/src/membench --test latency --size 32K
./build/src/membench --target cpu --format csv
./build/src/membench --test cache-detect --verbose</span></pre>
    </div>

    <div class="features" style="margin-top: 24px">
      <div class="feature-card">
        <h3>📖 User Guide</h3>
        <p>Full CLI reference, all flags, output formats, and how to interpret results.</p>
        <p style="margin-top: 8px"><a href="https://github.com/krishankumar95/volatile-membench/blob/main/docs/USER_GUIDE.md">Read the guide →</a></p>
      </div>
      <div class="feature-card">
        <h3>🧠 Key Learnings</h3>
        <p>18 technical deep-dives on memory walls, volatile semantics, prefetcher defeat, and cache detection.</p>
        <p style="margin-top: 8px"><a href="https://github.com/krishankumar95/volatile-membench/blob/main/docs/KEY_LEARNINGS.md">Read the learnings →</a></p>
      </div>
      <div class="feature-card">
        <h3>📊 Full Results</h3>
        <p>Complete benchmark data with analysis — Apple M1 vs AMD 5800X3D across all metrics.</p>
        <p style="margin-top: 8px"><a href="https://github.com/krishankumar95/volatile-membench/blob/main/docs/RESULTS.md">View results →</a></p>
      </div>
    </div>
  </div>
</section>
