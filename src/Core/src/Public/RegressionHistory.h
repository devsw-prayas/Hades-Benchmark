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

#include <string>
#include <unordered_map>

#include "StatsAccumulator.h"

namespace Hades::Runtime {

	// Snapshot of a test's historical throughput (and, for hotspot fixtures,
	// its historical divergence ratios) as seen by RegressionAnalyzer. A
	// sample count below the caller's min-history-floor means the comparison
	// isn't trusted yet - inconclusive, not a false regression on N=1.
	struct HistoryStats {
		double   throughputMean = 0.0;
		double   throughputStddev = 0.0;
		uint32_t throughputSampleCount = 0;

		double   readDivergenceRatioMean = 0.0;
		double   readDivergenceRatioStddev = 0.0;
		uint32_t readDivergenceSampleCount = 0;

		double   writeDivergenceRatioMean = 0.0;
		double   writeDivergenceRatioStddev = 0.0;
		uint32_t writeDivergenceSampleCount = 0;
	};

	template<typename D>
	class IRegressionHistoryBackend {
		using derived_ = D;

		HADES_NODISCARD constexpr derived_& self() noexcept {
			return static_cast<derived_&>(*this);
		}

	public:
		HADES_NODISCARD_MSG("Cannot discard history stats")
			HistoryStats fetch(const std::string& v_TestId) const {
			return static_cast<const derived_&>(*this).fetchImpl(v_TestId);
		}

		// Feeds one trusted (non-Failed) result into the rolling history.
		// v_HasDivergenceRatios gates whether the read/write streams are fed -
		// plain (non-hotspot) fixtures only ever update the throughput stream.
		void update(const std::string& v_TestId, double v_Throughput,
			bool v_HasDivergenceRatios, double v_ReadDivergenceRatio, double v_WriteDivergenceRatio) {
			self().updateImpl(v_TestId, v_Throughput, v_HasDivergenceRatios, v_ReadDivergenceRatio, v_WriteDivergenceRatio);
		}

	protected:
		IRegressionHistoryBackend() = default;
		~IRegressionHistoryBackend() = default;

	public:
		IRegressionHistoryBackend(const IRegressionHistoryBackend&) = delete;
		IRegressionHistoryBackend& operator=(const IRegressionHistoryBackend&) = delete;
		IRegressionHistoryBackend(IRegressionHistoryBackend&&) = delete;
		IRegressionHistoryBackend& operator=(IRegressionHistoryBackend&&) = delete;
	};

	// In-memory history backend - process lifetime only, no persistence.
	// A durable (e.g. JSON-file-backed) backend can implement the same CRTP
	// interface once a real on-disk history format is needed.
	class HADES_RUNTIME_API MemoryRegressionHistoryBackend final
		: public IRegressionHistoryBackend<MemoryRegressionHistoryBackend> {
	public:
		MemoryRegressionHistoryBackend() = default;
		~MemoryRegressionHistoryBackend() = default;

		MemoryRegressionHistoryBackend(const MemoryRegressionHistoryBackend&) = delete;
		MemoryRegressionHistoryBackend& operator=(const MemoryRegressionHistoryBackend&) = delete;
		MemoryRegressionHistoryBackend(MemoryRegressionHistoryBackend&&) = delete;
		MemoryRegressionHistoryBackend& operator=(MemoryRegressionHistoryBackend&&) = delete;

		HADES_NODISCARD_MSG("Cannot discard history stats")
			HistoryStats fetchImpl(const std::string& v_TestId) const {
			const auto it = m_entries.find(v_TestId);
			if (it == m_entries.end())
				return HistoryStats{};

			const Entry& entry = it->second;
			HistoryStats stats{};
			stats.throughputMean = entry.throughput.meanV;
			stats.throughputStddev = entry.throughput.stddev();
			stats.throughputSampleCount = entry.throughput.count;
			stats.readDivergenceRatioMean = entry.readDivergenceRatio.meanV;
			stats.readDivergenceRatioStddev = entry.readDivergenceRatio.stddev();
			stats.readDivergenceSampleCount = entry.readDivergenceRatio.count;
			stats.writeDivergenceRatioMean = entry.writeDivergenceRatio.meanV;
			stats.writeDivergenceRatioStddev = entry.writeDivergenceRatio.stddev();
			stats.writeDivergenceSampleCount = entry.writeDivergenceRatio.count;
			return stats;
		}

		void updateImpl(const std::string& v_TestId, double v_Throughput,
			bool v_HasDivergenceRatios, double v_ReadDivergenceRatio, double v_WriteDivergenceRatio) {
			Entry& entry = m_entries[v_TestId];
			entry.throughput.update(v_Throughput);
			if (v_HasDivergenceRatios) {
				entry.readDivergenceRatio.update(v_ReadDivergenceRatio);
				entry.writeDivergenceRatio.update(v_WriteDivergenceRatio);
			}
		}

	private:
		struct Entry {
			WelfordState throughput;
			WelfordState readDivergenceRatio;
			WelfordState writeDivergenceRatio;
		};

		std::unordered_map<std::string, Entry> m_entries;
	};

} // namespace Hades::Runtime
