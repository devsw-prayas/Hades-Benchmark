# Hades

HPC-grade microbenchmarking for C++.

Hades is a low-overhead, statistically rigorous benchmarking pipeline for CPU and GPU workloads. It handles warmup, calibration, jitter rejection, convergence detection, and determinism validation, so you can focus on what you're actually measuring.

Hades is three libraries plus a CLI, not one monolith:

- **Hades-Core** — the runtime: `IFixture`, `SuiteDriver`, adapters, chrono backends, statistics.
- **Hades-Codegen** — generates a suite's `main.cpp`/`CMakeLists.txt` from `registry.ini` + `suite.toml`.
- **Hades-Driver** (`hades-driver`) — the CLI that drives Codegen and builds/runs the result.



## Features

- **Statistical convergence** — Welford online mean/stddev with a configurable CV threshold; keeps running slices until results are stable, not until a timer expires.
- **Jitter rejection** — per-slice outlier detection, with consecutive and total discard caps to bail out of pathological runs.
- **Calibration** — the first slice auto-derives stopping knobs (min/max slices, CV target) scaled to your fixture's actual runtime.
- **Determinism validation** — every slice hashes the fixture's output state; divergence is detected, tracked, and reported.
- **Two timing backends** — `SteadyClockChronoPoint` for portability, `RdtscChronoPoint` for lowest overhead on x86.
- **GPU-ready** — `IDeviceAdapter`/`IFixture` accommodate GPU fixtures; CPU fixtures use the no-op `NullDeviceAdapter`.
- **Codegen'd suites** — a suite is declarative (`registry.ini` + `suite.toml`); `hades-driver` regenerates and builds it fresh on every `run`, so there's one source of truth for what it contains.



## Requirements

- C++20
- CMake 3.20+



## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHADES_BUILD_TESTS=ON
cmake --build build
```

`HADES_BUILD_TESTS=ON` builds `Hades-Core-Tests`/`Hades-Codegen-Tests` alongside the three libraries.



## Usage

### 1. Adopt a suite

```sh
hades-driver init-suite my-suite   # or find-suite, to adopt an existing one
```

This creates `suite.toml`/`registry.ini` in `my-suite/` and remembers it as the current suite (via a `.hades-suite` marker in your cwd) for every command below. `hades-driver rm my-suite` deletes a suite outright (and clears the marker if it was the adopted one).

### 2. Write a fixture

A fixture implements five methods and derives from `IFixture<Derived, Adapter>`:

```cpp
#include <Fixture.h>
#include <HadesAdapters.h>

using namespace Hades::Runtime;

class MyFixture final : public IFixture<MyFixture, NullDeviceAdapter> {
public:
    explicit MyFixture(NullDeviceAdapter& ro_Adapter) noexcept : IFixture(ro_Adapter) {}

    void startupImpl() noexcept { /* allocate, upload data */ }
    void executeImpl() noexcept { /* the thing you are measuring */ }
    void resetImpl(NullDeviceAdapter&) noexcept { /* reset state between slices */ }
    void teardownImpl() noexcept { /* free resources */ }

    uint64_t getDeterminismHashImpl() noexcept { return myOutputHash(); }
};
```

Register it and scaffold a test in one step:

```sh
hades-driver new-test MyFixture my_test \
    --header=MyFixture.h --adapter=NullDeviceAdapter \
    --chrono=Hades::Runtime::Chrono::SteadyClockChronoPoint --hash=XorHashAccumulator
```

The first time a fixture name is used, `--header`/`--adapter`/`--chrono`/`--hash` are required and get written into `registry.ini`; later tests against the same fixture only need `--kind`. A fixture linking against another library (its own `common/` submodule, say) takes `--extra-link-dir=<path>` + `--extra-link-target=<cmake-target>`.

### 3. Run it

```sh
hades-driver run                    # regenerate, build, run every queued test
hades-driver run --fbt=my_*         # filter by test-id glob
hades-driver run --format=json      # machine-readable output
hades-driver validate               # build into a scratch dir first; only promotes on success
```



## Result fields

| Field | Description |
|---|---|
| `meanThroughput` | iterations / mean slice wall time |
| `cv` | coefficient of variation (stddev / mean) |
| `sliceCount` | accepted measurement slices |
| `deterministic` | false if any slice hash diverged from the reference |
| `runFailed` | true if the discard cap was exceeded before convergence |
| `totalRunTime` | wall time from `startupImpl()` to `teardownImpl()` |



## License

MIT. See license header in any source file.



> Hades is part of the [Spectra](https://github.com/devsw-prayas/SpectraRenderer) ecosystem.
