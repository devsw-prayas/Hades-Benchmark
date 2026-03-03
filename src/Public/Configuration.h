/*
* Copyright (c) 2026 StormWeaver
*
* This file is part of Hades Benchmark
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
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#pragma once
#include <Hades.h>
#include "HadesCompiler.h"

namespace Hades::Runtime {
	enum class HADES_RUNTIME_API ChronoBackend : uint8_t {
		Rdtsc,         // RdtscChronoPoint    - lowest overhead, rdtsc/rdtscp
		SteadyClock,   // SteadyClockChronoPoint - portable, frequency-scaling immune
	};

	struct HADES_RUNTIME_API Config final {
		uint32_t      m_ThreadCount = 0;    // 0 = hardware_concurrency
		uint64_t      m_Iterations = 0;   // 0 = calibration-derived
		ChronoBackend m_ChronoBackend = ChronoBackend::SteadyClock;

		uint32_t m_WarmupCount = 0;   // user-provided; not calibration-derived

		double   m_CvThreshold = 0.0;   // coefficient of variation target
		uint32_t m_MinSlices = 0;     // minimum slice floor before stopping
		uint32_t m_MaxSlices = 0;     // hard slice cap regardless of CV
		uint32_t m_ConsecutiveDiscardCap = 0;     // consecutive jitter-contaminated slice limit
		uint32_t m_TotalDiscardCap = 0;     // total jitter-contaminated slice limit
	};

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setThreadCount(Config& ro_Config, uint32_t v_Count = 0) {
		ro_Config.m_ThreadCount = v_Count;
	}

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setIterations(Config& ro_Config, uint64_t v_Count = 0) {
		ro_Config.m_Iterations = v_Count;
	}

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setChronoBackend(Config& ro_Config, ChronoBackend v_Backend) {
		ro_Config.m_ChronoBackend = v_Backend;
	}

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setCvThreshold(Config& ro_Config, double v_Threshold) {
		ro_Config.m_CvThreshold = v_Threshold;
	}

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setMinSlices(Config& ro_Config, uint32_t v_Slices) {
		ro_Config.m_MinSlices = v_Slices;
	}

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setMaxSlices(Config& ro_Config, uint32_t v_Slices) {
		ro_Config.m_MaxSlices = v_Slices;
	}

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setConsecutiveDiscards(Config& ro_Config, uint32_t v_Discards) {
		ro_Config.m_ConsecutiveDiscardCap = v_Discards;
	}

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setTotalDiscards(Config& ro_Config, uint32_t v_Discards) {
		ro_Config.m_TotalDiscardCap = v_Discards;
	}

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setWarmup(Config& ro_Config, uint32_t v_Count) {
		ro_Config.m_WarmupCount = v_Count;
	}
} // namespace Hades