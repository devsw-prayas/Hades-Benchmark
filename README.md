# Hades

HPC-grade microbenchmarking for C++.

Hades is a low-overhead, statistically rigorous benchmarking pipeline designed for measuring performance of CPU and GPU workloads with precision. It handles the hard parts — warmup, calibration, jitter rejection, convergence detection, determinism validation, and multi-threaded fan-out — so you can focus on what you're actually measuring.

---

## Features

- **Statistical convergence** — Welford online mean/stddev with configurable CV threshold. Hades keeps running slices until your results are stable, not just until a timer expires.
- **Jitter rejection** — per-slice outlier detection based on per-thread time deviation. Contaminated slices are discarded automatically, with consecutive and total discard caps to bail out of pathological runs.
- **Calibration** — the first slice is used to auto-derive stopping knobs (min/max slices, CV target) scaled to your fixture's actual runtime. Fast fixtures get more slices; slow fixtures get fewer.
- **Determinism validation** — every slice hashes your fixture's output state. Divergence across slices is detected, tracked, and reported in the result.
- **Multi-threaded fan-out** — parallel chunk dispatch via a Chase-Lev work-stealing deque with a cyclic barrier for slice synchronization. Thread count is configurable or auto-derived from hardware concurrency.
- **Two timing backends** — `SteadyClock` for portability and frequency-scaling immunity, `RDTSC` for lowest possible overhead on x86.
- **GPU-ready** — the `IDeviceAdapter` and `IFixture` interfaces are designed to accommodate GPU fixtures (CUDA etc.). CPU fixtures use the no-op `NullDeviceAdapter`.
- **Pluggable storage** — ship results to memory (`MemoryStorageBackend`) or JSON (`JsonStorageBackend`), or implement your own via `IStorageBackend`.
- **SPSC engine queue** — `HadesEngine` accepts benchmark runs from a producer thread via a lock-free SPSC queue with back-pressure. The engine drains it on a dedicated thread.

---

## Requirements

- C++20
- CMake 3.20+ (or integrate the sources directly)
- x86/x86-64 for `RDTSC` backend — `SteadyClock` backend works on any architecture

---

## Building

Hades is built as a shared library.

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build
```

Or manually:

```bash
g++ -std=c++20 -O2 -pthread -shared -fPIC \
    -DHADES_SHARED -DHADES_BUILDING_RUNTIME \
    -I src/Public \
    src/Private/Hades.cpp src/Private/HadesChrono.cpp \
    -o libhades.so
```

---

## Quick Start

### 1. Define a Fixture

A fixture describes what you want to benchmark. It inherits from `IFixture<Derived, Adapter>` and implements five methods.

```cpp
#include <HadesEngine.h>
#include <HadesAdapters.h>

struct MyFixture : public Hades::Runtime::IFixture<MyFixture, Hades::Runtime::NullDeviceAdapter>
{
    explicit MyFixture(Hades::Runtime::NullDeviceAdapter& adapter)
        : IFixture(adapter) {}

    void startupImpl()  noexcept { /* allocate, upload data */ }
    void executeImpl()  noexcept { /* the thing you are measuring */ }
    void resetImpl(Hades::Runtime::NullDeviceAdapter&) noexcept { /* reset state between iterations */ }
    void teardownImpl() noexcept { /* free resources */ }

    uint64_t getDeterminismHashImpl() noexcept {
        // return a hash of your output state
        return myOutputHash();
    }
};
```

### 2. Configure and Run

```cpp
#include <HadesEngine.h>
#include <HadesStorage.h>
#include <HadesAdapters.h>
#include <HadesHash.h>

using namespace Hades::Runtime;

// Storage backend — keeps results in memory
MemoryStorageBackend storage;

// Engine — 1 worker thread
using Engine = HadesEngine<MyFixture, NullDeviceAdapter, XorHashAccumulator, MemoryStorageBackend>;
Engine engine(storage, 1);

// Configure the run
Config cfg;
setIterations(cfg, 10000);       // iterations per slice (0 = calibration-derived)
setWarmup(cfg, 5);               // warmup iterations before measurement
setCvThreshold(cfg, 0.02);       // stop when CV <= 2%
setChronoBackend(cfg, ChronoBackend::SteadyClock);

// Start the engine on a background thread
std::thread engineThread([&]{ engine.run(); });

// Submit a run (engine internally constructs fixture + adapter per worker thread)
MyFixture dummy(*(NullDeviceAdapter*)nullptr); // placeholder pointer
while (!engine.submit(&dummy, cfg))
    std::this_thread::yield();

// Wait for result
while (storage.count() == 0)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

// Shut down
engine.shutdown();
engineThread.join();

// Read result
const BenchmarkResult& result = storage.resultAt(0);
printf("Throughput:    %.2f iter/s\n", result.meanThroughput);
printf("CV:            %.4f\n",        result.cv);
printf("Slices:        %u\n",          result.sliceCount);
printf("Deterministic: %s\n",          result.deterministic ? "yes" : "no");
```

---

## Result Fields

| Field | Description |
|---|---|
| `meanThroughput` | iterations / mean slice wall time |
| `stddev` | standard deviation of wall time across slices |
| `cv` | coefficient of variation (stddev / mean) |
| `sliceCount` | number of accepted measurement slices |
| `deterministic` | false if any slice hash diverged from the reference |
| `referenceHash` | combined hash from the first accepted slice |
| `threadCount` | threads that participated |
| `iterationsPerSlice` | fixed iteration count per chunk per slice |
| `warmupCount` | warmup iterations executed before measurement |
| `totalRunTime` | wall time from startup() to teardown() |
| `cpuWallTimeMean` | Welford mean of per-slice max CPU wall times |
| `deviceKernelTimeMean` | Welford mean of per-slice max kernel times (GPU runs) |
| `driverOverheadMean` | Welford mean of per-slice driver overhead (GPU runs) |
| `pcieTransferTime` | host→device transfer time from startup() (GPU runs) |

---

## Configuration Reference

| Setter | Field | Default | Description |
|---|---|---|---|
| `setThreadCount` | `m_ThreadCount` | 0 (hw concurrency) | Worker threads |
| `setIterations` | `m_Iterations` | 0 (calibrated) | Iterations per slice |
| `setWarmup` | `m_WarmupCount` | 0 | Warmup iterations |
| `setChronoBackend` | `m_ChronoBackend` | `SteadyClock` | Timing backend |
| `setCvThreshold` | `m_CvThreshold` | 0 (calibrated) | CV convergence target |
| `setMinSlices` | `m_MinSlices` | 0 (calibrated) | Minimum slices before CV check |
| `setMaxSlices` | `m_MaxSlices` | 0 (calibrated) | Hard slice cap |
| `setConsecutiveDiscards` | `m_ConsecutiveDiscardCap` | 0 (calibrated) | Consecutive jitter limit |
| `setTotalDiscards` | `m_TotalDiscardCap` | 0 (calibrated) | Total jitter limit |

All fields default to 0, which means Hades derives the value from the calibration slice. Non-zero values override calibration.

---

## Architecture

```
HadesEngine
  └── SpscQueue<RunRequest>          # producer submits, engine drains
        └── ThreadRuntime
              ├── CyclicBarrier      # synchronizes slice boundaries
              ├── ChaseLevDeque      # work-stealing job dispatch
              ├── StatsAccumulator   # Welford stats + jitter + stopping logic
              └── DeterminismValidator # per-slice hash comparison
```

---

## License

MIT. See license header in any source file.

---

> Hades is part of the [Spectra](https://github.com/devsw-prayas/Spectra) ecosystem.
