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
#include <HadesCompiler.h>
#include <HadesDiagnositcs.h>

#include "Chrono.h"

#include <chrono>
#include <cstdint>

#if HADES_COMPILER_MSVC
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

namespace Hades::Runtime::Chrono {
	struct RdtscTimestamp {
		uint64_t ticks;
	};

	class HADES_RUNTIME_API RdtscChronoPoint final
		: public IChronoPoint<RdtscChronoPoint> {
	public:
		RdtscChronoPoint() noexcept
			: m_nsPerTick(internalCalibrate()) {
		}

		~RdtscChronoPoint() = default;

		RdtscChronoPoint(const RdtscChronoPoint&) = delete;
		RdtscChronoPoint& operator=(const RdtscChronoPoint&) = delete;
		RdtscChronoPoint(RdtscChronoPoint&&) = delete;
		RdtscChronoPoint& operator=(RdtscChronoPoint&&) = delete;

		HADES_NODISCARD_MSG("Cannot discard rdtsc timestamp")
			HADES_FORCEINLINE RdtscTimestamp nowImpl() noexcept {
			return RdtscTimestamp{ __rdtsc() };
		}

		HADES_NODISCARD_MSG("Cannot discard rdtsc delta")
			HADES_FORCEINLINE uint64_t deltaImpl(RdtscTimestamp v_Begin, RdtscTimestamp v_End) const noexcept {
			const uint64_t ticks = v_End.ticks - v_Begin.ticks;
			return static_cast<uint64_t>(static_cast<double>(ticks) * m_nsPerTick);
		}

	private:
		// Calibrates nanoseconds-per-tick by measuring a known chrono interval.
		// Called once at construction - not in any hot path.
		static double internalCalibrate() noexcept {
			constexpr uint32_t CALIBRATION_MS = 10;

			const uint64_t tscStart = __rdtsc();
			const auto     t0 = std::chrono::steady_clock::now();

			// Busy-wait for CALIBRATION_MS milliseconds
			while (true) {
				const auto elapsed = std::chrono::steady_clock::now() - t0;
				if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
					>= CALIBRATION_MS)
					break;
			}

			const uint64_t tscEnd = __rdtsc();
			const auto     t1 = std::chrono::steady_clock::now();

			const uint64_t ticks = tscEnd - tscStart;
			const double   ns = static_cast<double>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());

			return (ticks > 0) ? (ns / static_cast<double>(ticks)) : 1.0;
		}

		double m_nsPerTick;   // nanoseconds per TSC tick, calibrated at construction
	};

	struct SteadyTimestamp {
		std::chrono::steady_clock::time_point tp;
	};

	class HADES_RUNTIME_API SteadyClockChronoPoint final
		: public IChronoPoint<SteadyClockChronoPoint> {
	public:
		SteadyClockChronoPoint() = default;
		~SteadyClockChronoPoint() = default;

		SteadyClockChronoPoint(const SteadyClockChronoPoint&) = delete;
		SteadyClockChronoPoint& operator=(const SteadyClockChronoPoint&) = delete;
		SteadyClockChronoPoint(SteadyClockChronoPoint&&) = delete;
		SteadyClockChronoPoint& operator=(SteadyClockChronoPoint&&) = delete;

		HADES_NODISCARD_MSG("Cannot discard steady_clock timestamp")
			HADES_FORCEINLINE SteadyTimestamp nowImpl() noexcept {
			return SteadyTimestamp{ std::chrono::steady_clock::now() };
		}

		HADES_NODISCARD_MSG("Cannot discard steady_clock delta")
			HADES_FORCEINLINE uint64_t deltaImpl(SteadyTimestamp v_Begin, SteadyTimestamp v_End) const noexcept {
			return static_cast<uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					v_End.tp - v_Begin.tp).count());
		}
	};
}