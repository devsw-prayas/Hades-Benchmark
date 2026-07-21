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
#include "Storage.h"

#include <cstdio>
#include <cstring>

namespace Hades::Runtime {
	static constexpr uint32_t MAX_STORED_RESULTS = 1024;

	class HADES_RUNTIME_API MemoryStorageBackend final : public IStorageBackend<MemoryStorageBackend> {
	public:
		MemoryStorageBackend() noexcept
			: m_Results{}, m_Count(0) {
		}

		~MemoryStorageBackend() = default;

		MemoryStorageBackend(const MemoryStorageBackend&) = delete;
		MemoryStorageBackend& operator=(const MemoryStorageBackend&) = delete;
		MemoryStorageBackend(MemoryStorageBackend&&) = delete;
		MemoryStorageBackend& operator=(MemoryStorageBackend&&) = delete;

		void storeImpl(const BenchmarkResult& ro_Result) noexcept {
			if (HADES_UNLIKELY(m_Count >= MAX_STORED_RESULTS)) {
				HADES_ASSERT(false); // storage full - increase MAX_STORED_RESULTS
				return;
			}
			m_Results[m_Count++] = ro_Result;
		}

		HADES_NODISCARD_MSG("Cannot discard result count")
			uint32_t count() const noexcept {
			return m_Count;
		}

		HADES_NODISCARD_MSG("Cannot discard result")
			const BenchmarkResult& resultAt(uint32_t v_Index) const noexcept {
			HADES_ASSERT(v_Index < m_Count);
			return m_Results[v_Index];
		}

		// Returns a pointer to the contiguous result array.
		// Valid for [0, count()) entries.
		HADES_NODISCARD_MSG("Cannot discard results pointer")
			const BenchmarkResult* results() const noexcept {
			return m_Results;
		}

		void clear() noexcept {
			m_Count = 0;
		}

	private:
		BenchmarkResult m_Results[MAX_STORED_RESULTS];
		uint32_t        m_Count;
	};

	class HADES_RUNTIME_API JsonStorageBackend final
		: public IStorageBackend<JsonStorageBackend>
	{
	public:
		// Opens or creates the output file at p_Path in append mode.
		explicit JsonStorageBackend(const char* p_Path) noexcept
			: m_file(nullptr) {
			HADES_ASSERT(p_Path != nullptr);
			m_file = std::fopen(p_Path, "a");
		}

		~JsonStorageBackend() noexcept {
			if (m_file != nullptr) {
				HADES_UNUSED(std::fflush(m_file));
				HADES_UNUSED(std::fclose(m_file));
				m_file = nullptr;
			}
		}

		JsonStorageBackend(const JsonStorageBackend&) = delete;
		JsonStorageBackend& operator=(const JsonStorageBackend&) = delete;
		JsonStorageBackend(JsonStorageBackend&&) = delete;
		JsonStorageBackend& operator=(JsonStorageBackend&&) = delete;

		// -----------------------------------------------------------------------
		// CRTP implementation
		// -----------------------------------------------------------------------

		void storeImpl(const BenchmarkResult& ro_R) noexcept {
			if (HADES_UNLIKELY(m_file == nullptr))
				return;

			HADES_UNUSED(std::fprintf(m_file,
									  "{"
									  "\"meanThroughput\":%.9g,"
									  "\"stddev\":%.9g,"
									  "\"cv\":%.9g,"
									  "\"sliceCount\":%u,"
									  "\"finalSliceWallTime\":%.9g,"
									  "\"threadCount\":%u,"
									  "\"iterationsPerSlice\":%llu,"
									  "\"minSlices\":%u,"
									  "\"maxSlices\":%u,"
									  "\"cvThreshold\":%.9g,"
									  "\"warmupCount\":%u,"
									  "\"deterministic\":%s,"
									  "\"referenceHash\":%llu,"
									  "\"uniqueHashCount\":%u,"
									  "\"firstDivergenceSlice\":%u,"
									  "\"totalRunTime\":%.9g,"
									  "\"isGpuRun\":%s,"
									  "\"cpuWallTimeMean\":%.9g,"
									  "\"cpuWallTimeStddev\":%.9g,"
									  "\"deviceKernelTimeMean\":%.9g,"
									  "\"deviceKernelTimeStddev\":%.9g,"
									  "\"driverOverheadMean\":%.9g,"
									  "\"driverOverheadStddev\":%.9g,"
									  "\"pcieTransferTime\":%.9g"
									  "}\n",
									  ro_R.meanThroughput,
									  ro_R.stddev,
									  ro_R.cv,
									  ro_R.sliceCount,
									  ro_R.finalSliceWallTime,
									  ro_R.threadCount,
									  static_cast<unsigned long long>(ro_R.iterationsPerSlice),
									  ro_R.minSlices,
									  ro_R.maxSlices,
									  ro_R.cvThreshold,
									  ro_R.warmupCount,
									  ro_R.deterministic ? "true" : "false",
									  static_cast<unsigned long long>(ro_R.referenceHash),
									  ro_R.uniqueHashCount,
									  ro_R.firstDivergenceSlice,
									  ro_R.totalRunTime,
									  ro_R.isGpuRun ? "true" : "false",
									  ro_R.cpuWallTimeMean,
									  ro_R.cpuWallTimeStddev,
									  ro_R.deviceKernelTimeMean,
									  ro_R.deviceKernelTimeStddev,
									  ro_R.driverOverheadMean,
									  ro_R.driverOverheadStddev,
									  ro_R.pcieTransferTime
			));

			HADES_UNUSED(std::fflush(m_file));
		}

		HADES_NODISCARD_MSG("Cannot discard file open state")
			bool isOpen() const noexcept {
			return m_file != nullptr;
		}

	private:
		std::FILE* m_file;
	};
}
