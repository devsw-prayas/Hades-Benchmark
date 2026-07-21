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
#include <Hades.h>
#include <HadesChrono.h>

	#include <mutex>
namespace Hades::Runtime {

	double Chrono::RdtscChronoPoint::s_nsPerTick = 0.0;

	Chrono::RdtscChronoPoint::RdtscChronoPoint() noexcept {
		static std::once_flag once;
		std::call_once(once, []() {
			s_nsPerTick = internalCalibrate();
		});
	}

	Chrono::SteadyTimestamp Chrono::SteadyClockChronoPoint::nowImpl() noexcept {
		return SteadyTimestamp{ std::chrono::steady_clock::now() };
	}

	uint64_t Chrono::SteadyClockChronoPoint::deltaImpl(SteadyTimestamp v_Begin, SteadyTimestamp v_End) const noexcept {
		return static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				v_End.tp - v_Begin.tp).count());
	}


	Chrono::RdtscTimestamp Chrono::RdtscChronoPoint::nowImpl() noexcept {
		return RdtscTimestamp{ __rdtsc() };
	}

	uint64_t Chrono::RdtscChronoPoint::deltaImpl(RdtscTimestamp v_Begin, RdtscTimestamp v_End) const noexcept {
		const uint64_t ticks = v_End.ticks - v_Begin.ticks;
		return static_cast<uint64_t>(static_cast<double>(ticks) * s_nsPerTick);
	}

	double Chrono::RdtscChronoPoint::internalCalibrate() noexcept {
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
}
