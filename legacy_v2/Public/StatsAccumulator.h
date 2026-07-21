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

#include "BenchmarkResult.h"

namespace Hades::Runtime {
	struct SliceTimings {
		double cpuWallTime;       // max(thread cpu wall times) for this slice (seconds)
		double deviceKernelTime;  // max(thread device kernel times) for this slice (ms, GPU only)
		double driverOverhead;    // max(thread driver overheads) for this slice (seconds)
	};

	struct HADES_RUNTIME_API StoppingKnobs {
		double   cvThreshold = 0.0;
		uint32_t minSlices = 0;
		uint32_t maxSlices = 0;
		uint32_t consecutiveDiscardCap = 0;
		uint32_t totalDiscardCap = 0;
	};

	enum class StopReason : uint8_t {
		CvConverged,           // CV <= cvThreshold after min_floor
		HardCapReached,        // slices >= maxSlices
		ConsecutiveDiscards,   // consecutive jitter-contaminated slices >= cap -> failed
		TotalDiscards,         // total jitter-contaminated slices >= cap -> failed
		NotStopped,            // run still in progress
	};

	struct WelfordState {
		uint32_t count = 0;
		double   meanV = 0.0;
		double   m2 = 0.0;   // sum of squared deviations from meanV

		void update(double v_Value) noexcept {
			++count;
			const double delta = v_Value - meanV;
			meanV += delta / static_cast<double>(count);
			const double delta2 = v_Value - meanV;
			m2 += delta * delta2;
		}

		HADES_NODISCARD_MSG("Cannot discard Welford stddev")
			double stddev() const noexcept {
			return (count < 2) ? 0.0 : sqrt(m2 / static_cast<double>(count - 1));
		}
	};

	static constexpr uint32_t MAX_THREADS = 64;

	template<typename A>
	class StatsAccumulator final {
		using adapter_ = A;

	public:
		explicit StatsAccumulator(bool v_IsGpuRun) noexcept
			: m_isGpuRun(v_IsGpuRun)
			, m_acceptedSlices(0)
			, m_consecutiveDiscards(0)
			, m_totalDiscards(0)
			, m_runFailed(false)
			, m_stopReason(StopReason::NotStopped)
			, m_jitterThreshold(0.20)   // 20% deviation from thread median -> jitter
			, m_pcieTransferTime(0.0) {
		}

		~StatsAccumulator() = default;

		StatsAccumulator(const StatsAccumulator&) = delete;
		StatsAccumulator& operator=(const StatsAccumulator&) = delete;
		StatsAccumulator(StatsAccumulator&&) = delete;
		StatsAccumulator& operator=(StatsAccumulator&&) = delete;

		// Called once after the calibration slice to derive stopping knobs.
		// v_CalibrationWallTime is the cpu wall time of the calibration slice.
		// v_ConfigKnobs contains any user-provided overrides (0 = use derived).
		void deriveKnobs(double v_CalibrationWallTime, const StoppingKnobs& ro_ConfigKnobs) noexcept {
			// Derive defaults from calibration wall time
			// Faster fixtures need more slices to converge; scale floor accordingly
			if (v_CalibrationWallTime < 1e-4) {
				m_knobs.minSlices = 30;
				m_knobs.maxSlices = 500;
			} else if (v_CalibrationWallTime < 1e-2) {
				m_knobs.minSlices = 20;
				m_knobs.maxSlices = 300;
			} else {
				m_knobs.minSlices = 10;
				m_knobs.maxSlices = 200;
			}
			m_knobs.cvThreshold = 0.02;
			m_knobs.consecutiveDiscardCap = 5;
			m_knobs.totalDiscardCap = 20;

			// Apply user overrides - non-zero values override calibration result

			if (ro_ConfigKnobs.cvThreshold > 0.0) m_knobs.cvThreshold = ro_ConfigKnobs.cvThreshold;
			if (ro_ConfigKnobs.minSlices > 0)   m_knobs.minSlices = ro_ConfigKnobs.minSlices;
			if (ro_ConfigKnobs.maxSlices > 0)   m_knobs.maxSlices = ro_ConfigKnobs.maxSlices;
			if (ro_ConfigKnobs.consecutiveDiscardCap > 0)   m_knobs.consecutiveDiscardCap = ro_ConfigKnobs.consecutiveDiscardCap;
			if (ro_ConfigKnobs.totalDiscardCap > 0)   m_knobs.totalDiscardCap = ro_ConfigKnobs.totalDiscardCap;
		}

		// Feeds per-thread cpu wall times for jitter detection.
		// Must be called before feedSlice() each slice.
		// v_ThreadCount is the number of participating threads this slice.
		void feedThreadTimes(const double* pa_ThreadTimes, uint32_t v_ThreadCount) noexcept {
			HADES_ASSERT(pa_ThreadTimes != nullptr);
			HADES_ASSERT(v_ThreadCount > 0 && v_ThreadCount <= MAX_THREADS);

			for (uint32_t i = 0; i < v_ThreadCount; ++i)
				m_threadTimes[i] = pa_ThreadTimes[i];

			m_threadCount = v_ThreadCount;
		}

		// Ingests one slice's aggregated timings.
		// Returns true if the slice was accepted, false if discarded as jitter.
		bool feedSlice(const SliceTimings& ro_Timings) noexcept {
			if (internalIsJittered(ro_Timings.cpuWallTime)) {
				++m_totalDiscards;
				++m_consecutiveDiscards;
				return false;
			}

			m_consecutiveDiscards = 0;

			m_cpuWallTime.update(ro_Timings.cpuWallTime);

			if (m_isGpuRun) {
				m_deviceKernelTime.update(ro_Timings.deviceKernelTime);
				m_driverOverhead.update(ro_Timings.driverOverhead);
			}

			// Throughput tracked on cpu wall time stream
			// (iterationsPerSlice is baked into cpuWallTime by ThreadRuntime)
			++m_acceptedSlices;
			return true;
		}

		// Called by thread 0 before BARRIER 1 to pull fixture output onto host.
		// For CPU fixtures, adapter is a no-op pass-through.
		void solidify(adapter_& ro_Adapter) noexcept {
			ro_Adapter.synchronize();
		}

		// Records the PCIe transfer time captured during startup().
		void setPcieTransferTime(double v_Time) noexcept {
			m_pcieTransferTime = v_Time;
		}

		// Returns true if the run should stop after the current slice.
		HADES_NODISCARD_MSG("Cannot discard stop decision")
			bool shouldStop() noexcept {
			if (m_consecutiveDiscards >= m_knobs.consecutiveDiscardCap) {
				m_runFailed = true;
				m_stopReason = StopReason::ConsecutiveDiscards;
				return true;
			}
			if (m_totalDiscards >= m_knobs.totalDiscardCap) {
				m_runFailed = true;
				m_stopReason = StopReason::TotalDiscards;
				return true;
			}
			if (m_acceptedSlices >= m_knobs.maxSlices) {
				m_stopReason = StopReason::HardCapReached;
				return true;
			}
			if (m_acceptedSlices >= m_knobs.minSlices) {
				const double cv = internalCurrentCv();
				if (cv <= m_knobs.cvThreshold) {
					m_stopReason = StopReason::CvConverged;
					return true;
				}
			}
			return false;
		}

		HADES_NODISCARD_MSG("Cannot discard stop reason")
			StopReason stopReason() const noexcept {
			return m_stopReason;
		}

		HADES_NODISCARD_MSG("Cannot discard run failed flag")
			bool runFailed() const noexcept {
			return m_runFailed;
		}

		void assembleResult(BenchmarkResult& ro_Result, uint64_t v_IterationsPerSlice) const noexcept {
			// Throughput = iterations / cpu wall time meanV
			ro_Result.meanThroughput = (m_cpuWallTime.meanV > 0.0)
				? (static_cast<double>(v_IterationsPerSlice) / m_cpuWallTime.meanV)
				: 0.0;
			ro_Result.stddev = m_cpuWallTime.stddev();
			ro_Result.cv = internalCurrentCv();
			ro_Result.sliceCount = m_acceptedSlices;
			ro_Result.finalSliceWallTime = m_cpuWallTime.meanV;   // last accepted meanV

			ro_Result.isGpuRun = m_isGpuRun;

			if (m_isGpuRun) {
				ro_Result.cpuWallTimeMean = m_cpuWallTime.meanV;
				ro_Result.cpuWallTimeStddev = m_cpuWallTime.stddev();
				ro_Result.deviceKernelTimeMean = m_deviceKernelTime.meanV;
				ro_Result.deviceKernelTimeStddev = m_deviceKernelTime.stddev();
				ro_Result.driverOverheadMean = m_driverOverhead.meanV;
				ro_Result.driverOverheadStddev = m_driverOverhead.stddev();
				ro_Result.pcieTransferTime = m_pcieTransferTime;
			}
		}

		HADES_NODISCARD_MSG("Cannot discard accepted slice count")
			uint32_t acceptedSlices() const noexcept { return m_acceptedSlices; }

		HADES_NODISCARD_MSG("Cannot discard current CV")
			double currentCv() const noexcept { return internalCurrentCv(); }

		HADES_NODISCARD_MSG("Cannot discard stopping knobs")
			const StoppingKnobs& knobs() const noexcept { return m_knobs; }

	private:
		// Returns true if the slice's max cpu wall time is a jitter outlier
		// relative to the per-thread median.
		bool internalIsJittered(double v_SliceWallTime) const noexcept {
			if (m_threadCount == 0)
				return false;

			// Compute median of per-thread times via simple insertion sort on a copy
			double sorted[MAX_THREADS];
			for (uint32_t i = 0; i < m_threadCount; ++i)
				sorted[i] = m_threadTimes[i];

			for (uint32_t i = 1; i < m_threadCount; ++i) {
				const double key = sorted[i];
				int32_t j = static_cast<int32_t>(i) - 1;
				while (j >= 0 && sorted[j] > key) {
					sorted[j + 1] = sorted[j];
					--j;
				}
				sorted[j + 1] = key;
			}

			const double median = (m_threadCount % 2 == 1)
				? sorted[m_threadCount / 2]
				: (sorted[m_threadCount / 2 - 1] + sorted[m_threadCount / 2]) * 0.5;

			if (median <= 0.0)
				return false;

			const double deviation = (v_SliceWallTime - median) / median;
			return deviation > m_jitterThreshold;
		}

		HADES_NODISCARD_MSG("Cannot discard CV")
			double internalCurrentCv() const noexcept {
			return (m_cpuWallTime.meanV > 0.0)
				? (m_cpuWallTime.stddev() / m_cpuWallTime.meanV)
				: 0.0;
		}

		bool         m_isGpuRun;
		uint32_t     m_acceptedSlices;
		uint32_t     m_consecutiveDiscards;
		uint32_t     m_totalDiscards;
		bool         m_runFailed;
		StopReason   m_stopReason;
		double       m_jitterThreshold;
		double       m_pcieTransferTime;
		StoppingKnobs m_knobs;

		// Per-thread time scratch buffer for jitter detection
		uint32_t m_threadCount = 0;
		double   m_threadTimes[MAX_THREADS] = {};

		// Welford accumulators - one per timing stream
		WelfordState m_cpuWallTime;
		WelfordState m_deviceKernelTime;
		WelfordState m_driverOverhead;
	};
}
