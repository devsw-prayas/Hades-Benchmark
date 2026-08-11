/*
* Copyright (c) 2026 StormWeaver
*
* This file is part of the Hades Benchmarking API
*
* Licensed under the MIT License. You may obtain a copy of the License at
* https://opensource.org/licenses/MIT
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND...
*/
#pragma once
#include <Hades.h>
#include "HadesDiagnostics.h"

namespace Hades::Runtime {

    struct HADES_RUNTIME_API BenchmarkResult final{

        BenchmarkResult() = default;
        BenchmarkResult(const BenchmarkResult&) = default;
        BenchmarkResult& operator=(const BenchmarkResult&) = default;

        BenchmarkResult(BenchmarkResult&&) noexcept = default;
        BenchmarkResult& operator=(BenchmarkResult&&) noexcept = default;
        ~BenchmarkResult() = default;

        double totalRunTime;   // wall time from startup() to teardown() (seconds)

        double   meanThroughput;        // iterations / sliceWallTime, Welford meanV
        double   stddev;                // Welford standard deviation of throughput
        double   cv;                    // coefficient of variation = stddev / meanV
        double   finalSliceWallTime;    // wall time of the last accepted slice (seconds)
        uint32_t sliceCount;            // number of real measurement slices completed

        uint32_t threadCount;           // threads that participated in this run
        uint64_t iterationsPerSlice;    // fixed iteration count per chunk per slice
        uint32_t minSlices;             // calibration-derived min_floor (after overrides)
        uint32_t maxSlices;             // calibration-derived hard_max_cap (after overrides)
        double   cvThreshold;           // calibration-derived cv_threshold (after overrides)
        uint32_t warmupCount;           // warmup iterations executed before calibration

        uint32_t uniqueHashCount;         // distinct hashes observed across all slices
        uint64_t referenceHash;           // combined hash from the first accepted slice
        uint32_t firstDivergenceSlice;    // index of first diverging slice (0 if none)
        bool     deterministic;           // false if any slice hash diverged from reference
        bool     runFailed;               // true if consecutive/total discard cap was exceeded

        bool isGpuRun;   // true when fixture was dispatched with a GPU adapter

        double cpuWallTimeMean;           // Welford meanV   of per-slice max cpu wall times
        double cpuWallTimeStddev;         // Welford stddev of per-slice max cpu wall times
        double deviceKernelTimeMean;      // Welford meanV   of per-slice max kernel times
        double deviceKernelTimeStddev;    // Welford stddev of per-slice max kernel times
        double driverOverheadMean;        // Welford meanV   of per-slice max driver overhead
        double driverOverheadStddev;      // Welford stddev of per-slice max driver overhead

        double pcieTransferTime;   // host->device transfer time captured during startup()

        // v3: IHotspotFixture two-phase results. Populated only when isHotspotRun.
        bool     isHotspotRun;
        double   readLatencyBaselineMean;
        double   writeLatencyBaselineMean;
        double   readLatencyContendedMean,  readLatencyContendedP50;
        double   readLatencyContendedP99,   readLatencyContendedMax;
        double   writeLatencyContendedMean, writeLatencyContendedP50;
        double   writeLatencyContendedP99,  writeLatencyContendedMax;
        double   readDivergenceRatio;    // contended_mean / baseline_mean
        double   writeDivergenceRatio;
        uint32_t samplesPerExecute, helperThreadCount, fixedSliceCount;
        double   readCriticality;
    };

    HADES_STATIC_ASSERT(sizeof(BenchmarkResult) == 280, "BenchmarkResult layout changed - update size comment");
    HADES_STATIC_ASSERT(std::is_standard_layout_v<BenchmarkResult>, "Benchmark result is not of standard layout");
    HADES_STATIC_ASSERT(std::is_trivially_copy_assignable_v<BenchmarkResult>, "Benchmark must be trivially copyable");
    HADES_STATIC_ASSERT(std::is_trivially_move_assignable_v<BenchmarkResult>, "Benchmark must be trivially move assignable");

}
