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
#include <HadesCompiler.h>
#include <HadesDiagnostics.h>

#include <atomic>

#include "Fixture.h"
#include "Adapter.h"
#include "Chrono.h"
#include "Configuration.h"
#include "BenchmarkResult.h"
#include "StatsAccumulator.h"
#include "Validator.h"

namespace Hades::Runtime {

	// v2's CyclicBarrier synchronized two threads across an epoch boundary.
	// SequentialRunner drives exactly one adapter/fixture pair, so there is
	// nothing left to actually barrier on - this exists only to preserve the
	// seq_cst fence v2 relied on at the same call sites (host writes visible
	// before a GPU kernel launch, device writes visible before a host read).
	class CompletionFence final {
	public:
		CompletionFence() = delete;

		HADES_FORCEINLINE static void signal() noexcept {
			std::atomic_thread_fence(std::memory_order_seq_cst);
		}
	};

	// Drives one IFixture<D,A> through its full measurement lifecycle:
	// startup -> warmup -> calibration -> CV-converging slice loop -> teardown.
	// Zero virtual dispatch - D, A, C, H are all resolved at compile time.
	//
	// Caller constructs ro_Fixture/ro_Adapter (adapter injected into fixture at
	// construction, per IFixture<D,A>::IFixture(A&)) and owns their lifetime for
	// the duration of run(). Iterating multiple queued tests, and construct/
	// destroy per test, is SuiteDriver's job - SequentialRunner runs exactly one.
	template<typename D, typename A, typename C, typename H>
	class SequentialRunner final {
		using fixture_ = D;
		using adapter_ = A;
		using chrono_ = C;
		using hash_ = H;

	public:
		SequentialRunner() = delete;

		HADES_NODISCARD_MSG("Cannot discard benchmark result")
			static BenchmarkResult run(fixture_& ro_Fixture, adapter_& ro_Adapter, const Config& ro_Config, bool v_IsGpuRun) noexcept {
			chrono_ chrono;
			hash_   hashAccumulator;
			DeterminismValidator<hash_> validator(hashAccumulator);
			StatsAccumulator<adapter_> stats(v_IsGpuRun);

			const auto runStart = chrono.now();

			ro_Fixture.startup();
			CompletionFence::signal();

			if (ro_Config.m_Kind == TestKind::Correctness) {
				return internalRunCorrectnessOnce(ro_Fixture, ro_Adapter, chrono, runStart, v_IsGpuRun);
			}

			for (uint32_t i = 0; i < ro_Config.m_WarmupCount; ++i) {
				ro_Fixture.execute();
				ro_Adapter.synchronize();
			}

			// Calibration slice - not fed into stats, only used to derive stopping knobs.
			const uint64_t iterationsPerSlice = internalResolveIterationsPerSlice(ro_Config);
			const SliceTimings calibration = internalRunSlice(ro_Fixture, ro_Adapter, chrono, iterationsPerSlice, v_IsGpuRun);

			StoppingKnobs configKnobs;
			configKnobs.cvThreshold = ro_Config.m_CvThreshold;
			configKnobs.minSlices = ro_Config.m_MinSlices;
			configKnobs.maxSlices = ro_Config.m_MaxSlices;
			configKnobs.consecutiveDiscardCap = ro_Config.m_ConsecutiveDiscardCap;
			configKnobs.totalDiscardCap = ro_Config.m_TotalDiscardCap;
			stats.deriveKnobs(calibration.cpuWallTime, configKnobs);

			while (true) {
				const SliceTimings timings = internalRunSlice(ro_Fixture, ro_Adapter, chrono, iterationsPerSlice, v_IsGpuRun);

				// BARRIER 1 collapses to CompletionFence with sequential execution
				CompletionFence::signal();
				stats.solidify(ro_Adapter);
				const uint64_t hash = ro_Fixture.getDeterminismHash();
				validator.feedThreadHash(hash);
				validator.validateSlice();

				stats.feedSlice(timings);
				// BARRIER 2 collapses to CompletionFence
				CompletionFence::signal();

				if (stats.shouldStop())
					break;
			}

			CompletionFence::signal();
			ro_Fixture.teardown();
			stats.solidify(ro_Adapter);

			const auto runEnd = chrono.now();

			BenchmarkResult result{};
			stats.assembleResult(result, iterationsPerSlice);

			result.totalRunTime = internalSecondsBetween(chrono, runStart, runEnd);
			result.threadCount = 1;
			result.iterationsPerSlice = iterationsPerSlice;
			result.warmupCount = ro_Config.m_WarmupCount;
			result.minSlices = stats.knobs().minSlices;
			result.maxSlices = stats.knobs().maxSlices;
			result.cvThreshold = stats.knobs().cvThreshold;

			result.deterministic = validator.isDeterministic();
			result.referenceHash = validator.referenceHash();
			result.uniqueHashCount = validator.uniqueHashCount();
			result.firstDivergenceSlice = validator.firstDivergenceSlice();
			result.runFailed = stats.runFailed();

			result.isHotspotRun = false;

			return result;
		}

	private:
		// TestKind::Correctness: execute() called exactly once (no warmup, no
		// calibration, no CV-converging slice loop) - Pass/Failed only. There
		// is no reference hash to diverge from on a single call, so the run is
		// trivially deterministic; a fixture that wants a real correctness
		// check asserts/traps inside executeImpl() itself.
		template<typename T>
		HADES_NODISCARD_MSG("Cannot discard benchmark result")
			static BenchmarkResult internalRunCorrectnessOnce(
				fixture_& ro_Fixture, adapter_& ro_Adapter, chrono_& ro_Chrono,
				T v_RunStart, bool v_IsGpuRun) noexcept {
			ro_Fixture.execute();
			ro_Adapter.synchronize();
			CompletionFence::signal();

			const uint64_t hash = ro_Fixture.getDeterminismHash();

			ro_Fixture.reset(ro_Adapter);
			CompletionFence::signal();
			ro_Fixture.teardown();

			const auto runEnd = ro_Chrono.now();

			BenchmarkResult result{};
			result.totalRunTime = internalSecondsBetween(ro_Chrono, v_RunStart, runEnd);
			result.threadCount = 1;
			result.iterationsPerSlice = 1;
			result.sliceCount = 1;
			result.deterministic = true;
			result.referenceHash = hash;
			result.uniqueHashCount = 1;
			result.runFailed = false;
			result.isGpuRun = v_IsGpuRun;
			result.isHotspotRun = false;
			return result;
		}

		// Config.m_Iterations == 0 ("calibration-derived") resolves to a single
		// execute() call per slice - deriveKnobs already scales min/max slice
		// counts for sub-100us fixtures, so noise is absorbed by taking more
		// slices rather than inflating the per-slice iteration count.
		HADES_NODISCARD_MSG("Cannot discard resolved iteration count")
			static uint64_t internalResolveIterationsPerSlice(const Config& ro_Config) noexcept {
			return (ro_Config.m_Iterations > 0) ? ro_Config.m_Iterations : 1;
		}

		HADES_NODISCARD_MSG("Cannot discard slice timings")
			static SliceTimings internalRunSlice(fixture_& ro_Fixture, adapter_& ro_Adapter, chrono_& ro_Chrono, uint64_t v_Iterations, bool v_IsGpuRun) noexcept {
			typename adapter_::event_type startEvent{};
			typename adapter_::event_type endEvent{};

			const auto cpuStart = ro_Chrono.now();
			ro_Adapter.recordEvent(startEvent);

			for (uint64_t i = 0; i < v_Iterations; ++i)
				ro_Fixture.execute();

			ro_Adapter.recordEvent(endEvent);
			ro_Adapter.synchronize();
			const auto cpuEnd = ro_Chrono.now();

			const double cpuWallTime = internalSecondsBetween(ro_Chrono, cpuStart, cpuEnd);
			const double deviceKernelTime = v_IsGpuRun
				? static_cast<double>(ro_Adapter.elapsedTime(startEvent, endEvent))
				: 0.0;

			ro_Fixture.reset(ro_Adapter);
			ro_Adapter.synchronize();

			SliceTimings timings{};
			timings.cpuWallTime = cpuWallTime;
			timings.deviceKernelTime = deviceKernelTime;
			timings.driverOverhead = v_IsGpuRun ? (cpuWallTime - deviceKernelTime * 1e-3) : 0.0;
			return timings;
		}

		template<typename T>
		HADES_NODISCARD_MSG("Cannot discard elapsed seconds")
			static double internalSecondsBetween(chrono_& ro_Chrono, T v_Begin, T v_End) noexcept {
			return static_cast<double>(ro_Chrono.delta(v_Begin, v_End)) * 1e-9;   // ns -> s
		}
	};
}
