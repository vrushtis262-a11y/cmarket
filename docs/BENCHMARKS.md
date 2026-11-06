
# cmarket — Benchmark Results

## 1. Overview

This document records the Week 9 performance
measurements for cmarket, a C++20 order matching system.

The benchmarks evaluate:

- Limit order placement
- Order cancellation
- Order modification
- Order matching
- Mixed operation throughput
- Concurrent submission and execution
- Operation latency distributions

Results were collected on November 6, 2026.

All reported measurements are workload-specific and
should not be interpreted as guaranteed performance.

## 2. Benchmark Environment

| Component | Configuration |
|---|---|
| Processor | Apple M1 |
| Architecture | ARM64 |
| Operating system | macOS |
| Compiler | Apple Clang 21.0.0 |
| Build system | CMake 4.4.0 |
| Build type | Release |
| Language | C++20 |

The benchmarks were executed locally on the same machine.

The available results do not specify CPU temperature,
background system utilization, memory pressure, or
exact compiler optimization flags.

## 3. Benchmark Implementations

The project includes four benchmark executables.

| Executable | Purpose |
|---|---|
| baseline_benchmark | Average operation latency and throughput |
| latency_benchmark | P50, P95, P99, and worst-case sampled latency |
| throughput_benchmark | Throughput under different workloads |
| concurrent_benchmark | Concurrent throughput and completion latency |

Source files are located in `benchmarks/`.

## 4. Reproduction Instructions

From the repository root, configure the Release build:

```bash
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release
```

Compile the benchmark targets:

```bash
cmake --build build-release \
  --target baseline_benchmark \
  latency_benchmark \
  throughput_benchmark \
  concurrent_benchmark \
  -j 4
```

Create the results directory:

```bash
mkdir -p benchmarks/results
```

Run the four benchmarks:

```bash
./build-release/baseline_benchmark \
  | tee benchmarks/results/week9_baseline_results.txt

./build-release/latency_benchmark \
  | tee benchmarks/results/week9_latency_results.txt

./build-release/throughput_benchmark \
  | tee benchmarks/results/week9_throughput_results.txt

./build-release/concurrent_benchmark \
  | tee benchmarks/results/week9_concurrent_results.txt
```

For repeatable comparisons:

1. Use the same hardware and compiler configuration.
2. Use the same Release build settings.
3. Avoid heavy background workloads.
4. Run multiple independent benchmark trials.
5. Compare results using consistent workload definitions.
6. Record software and hardware changes between runs.

## 5. Baseline Benchmark

Each baseline workload executes 100,000 operations.

| Operation | Latency (ns/op) | Throughput (ops/sec) |
|---|---:|---:|
| Limit buy placement | 557.48 | 1,793,800 |
| Limit sell placement | 303.49 | 3,295,056 |
| Order cancellation | 122.74 | 8,147,026 |
| Order modification | 294.11 | 3,400,079 |
| Market buy matching | 211.49 | 4,728,356 |

### Observations

Order cancellation had the lowest measured average
latency in the baseline benchmark.

Limit buy placement had the highest measured average
latency among these five workloads.

Differences may reflect the operation's workload,
data structure access patterns, allocation behavior,
and benchmark setup.

The results should not be generalized to every
possible order-book state.

## 6. Latency Benchmark

Each operation uses:

- 100 warmup iterations
- 1,000 measured iterations

Book sizes evaluated:

- 100
- 1,000
- 10,000
- 100,000

### Latency at 100,000 Orders

All values are in nanoseconds.

| Operation | P50 | P95 | P99 | Worst |
|---|---:|---:|---:|---:|
| Place order | 209 | 250 | 250 | 292 |
| Match order | 250 | 333 | 459 | 3,041 |
| Cancel order | 250 | 250 | 250 | 334 |
| Modify order | 41 | 42 | 42 | 83 |
| Modify price | 500 | 500 | 542 | 625 |
| Multi-level match | 2,167 | 2,250 | 2,292 | 15,125 |

### Interpretation

P50 represents the median sampled latency.

P95 is the threshold at or below which approximately
95% of measured operations completed.

P99 is the corresponding threshold for approximately
99% of measured operations.

The worst value is the maximum observed sample,
not a theoretical upper bound.

### Tail Latency

At 100,000 resting orders:

- Order placement P99: 250 ns
- Order matching P99: 459 ns
- Order cancellation P99: 250 ns
- Quantity modification P99: 42 ns
- Price modification P99: 542 ns
- Multi-level matching P99: 2,292 ns

Multi-level matching has greater latency than the
simpler measured operations, reflecting its different
workload.

At a book size of 100, one multi-level matching
sample reached 92,209 ns.

Single-run outliers should be investigated using
repeated measurements before attributing them to
a specific implementation bottleneck.

## 7. Throughput Benchmark

Configuration:

- 100,000 operations per workload
- 100,000 initial active orders

| Workload | Seconds | Throughput (ops/sec) |
|---|---:|---:|
| Order placement | 0.039 | 2,560,792 |
| Heavy matching | 0.028 | 3,630,467 |
| Place/cancel mix | 0.030 | 3,365,927 |
| Place/modify mix | 0.018 | 5,614,639 |
| Mixed workload | 0.022 | 4,549,375 |

### Observations

Place/modify mix achieved the highest measured
throughput at approximately 5.61 million
operations per second.

Order placement achieved approximately 2.56 million
operations per second.

The workload composition differs between scenarios,
so throughput results are not direct measurements
of identical operations.

Elapsed times displayed in seconds are rounded.

## 8. Concurrent Benchmark

Configuration:

- 4 producer threads
- 25,000 operations per producer
- 100,000 total operations
- 20,000 latency samples

### Concurrent Throughput

| Workload | Seconds | Throughput (ops/sec) |
|---|---:|---:|
| Concurrent placement | 0.205 | 487,784 |
| Batched async submission | 0.067 | 1,502,605 |
| Concurrent market execution | 0.254 | 393,179 |

### Concurrent Latency

All values are in nanoseconds.

| Workload | Mean | P50 | P95 | P99 |
|---|---:|---:|---:|---:|
| Synchronous round-trip | 7,783 | 4,833 | 12,000 | 37,958 |
| Queued async completion | 7,786,238 | 7,817,416 | 9,213,708 | 9,392,666 |

### Interpretation

Batched asynchronous submission produced the highest
throughput of the three concurrent workloads.

Queued asynchronous completion showed substantially
higher completion latency than synchronous round-trip
execution.

These measurements use different submission strategies.

End-to-end completion times can include queueing and
scheduling overhead in addition to matching-engine work.

The concurrent measurements should not be directly
compared with single-threaded benchmarks without
accounting for their workload and execution differences.

## 9. Performance Analysis

### Strengths

- Sub-microsecond P99 measurements for several
  individual operations in the tested 100,000-order
  latency scenario.
- Millions of operations per second in multiple
  single-threaded throughput workloads.
- Separate benchmarks for synchronous and
  asynchronous concurrent execution.
- Support for recording median and tail latency.

### Limitations

- Results currently represent a single recorded
  local run per benchmark executable.
- The tested operations differ in complexity and
  workload composition.
- The latency benchmark tests a maximum listed
  book size of 100,000 orders.
- One-million-order scalability has not yet
  been established by these results.
- The measurements do not establish real-time
  latency guarantees.
- Long-duration performance stability and memory
  utilization were not measured in this session.

## 10. Result Files

Raw console outputs are stored in:

- `benchmarks/results/week9_baseline_results.txt`
- `benchmarks/results/week9_latency_results.txt`
- `benchmarks/results/week9_throughput_results.txt`
- `benchmarks/results/week9_concurrent_results.txt`

These files preserve the recorded measurements
for verification and future comparisons.

## 11. Next Steps

Recommended future measurements:

1. Repeat each benchmark multiple times.
2. Calculate cross-run averages and variation.
3. Test one-million-order workloads where practical.
4. Measure memory usage under increasing book sizes.
5. Profile CPU utilization and allocation overhead.
6. Investigate latency outliers.
7. Compare performance after targeted optimizations.

## 12. Related Documentation

- [C++ API Documentation](API.md)
- [Architecture Diagrams](ARCHITECTURE_DIAGRAMS.md)
- [Sequence Diagrams](SEQUENCE_DIAGRAMS.md)

A separate performance report can expand on
algorithmic complexity, bottlenecks, tradeoffs,
and optimization opportunities.
