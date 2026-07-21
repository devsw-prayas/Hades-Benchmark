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

#include <cmath>
#include <string>

#include "BenchmarkResult.h"
#include "RegressionHistory.h"

namespace Hades::Runtime {

	// Exit code doubles as severity rank - Failed > Degraded > Regressed > Pass.
	// Malformed suite file / bad CLI args is a separate exit code 2, handled by
	// Driver, not here (highest aggregation priority, nothing downstream trustable).
	enum class TestOutcome : uint8_t {
		Pass = 0,
		Regressed = 1,
		Degraded = 3,
		Failed = 4,
	};

	// delta > k * historical_stddev - not Welch's t-test. Chosen for
	// consistency with StatsAccumulator's existing CV-based noise model
	// (avoids a second, statistically-independent-assumption-bearing framework).
	template<typename H>
	class RegressionAnalyzer final {
		using history_ = H;
		history_& m_history;
		double    m_kStddev;
		uint32_t  m_minHistoryFloor;

	public:
		explicit RegressionAnalyzer(history_& ro_History, double v_KStddev = 3.0, uint32_t v_MinHistoryFloor = 5) noexcept
			: m_history(ro_History)
			, m_kStddev(v_KStddev)
			, m_minHistoryFloor(v_MinHistoryFloor) {
		}

		~RegressionAnalyzer() = default;

		RegressionAnalyzer(const RegressionAnalyzer&) = delete;
		RegressionAnalyzer& operator=(const RegressionAnalyzer&) = delete;
		RegressionAnalyzer(RegressionAnalyzer&&) = delete;
		RegressionAnalyzer& operator=(RegressionAnalyzer&&) = delete;

		// Failed results (discard caps exceeded, non-deterministic output) are
		// not trustworthy samples - reported as Failed immediately, history is
		// left untouched so a bad run can't poison future comparisons.
		HADES_NODISCARD_MSG("Cannot discard test outcome")
			TestOutcome evaluate(const std::string& v_TestId, const BenchmarkResult& ro_Result) {
			if (ro_Result.runFailed || !ro_Result.deterministic)
				return TestOutcome::Failed;

			const HistoryStats history = m_history.fetch(v_TestId);
			TestOutcome outcome = TestOutcome::Pass;

			if (history.throughputSampleCount >= m_minHistoryFloor && history.throughputStddev > 0.0) {
				const double delta = std::abs(ro_Result.meanThroughput - history.throughputMean);
				if (delta > m_kStddev * history.throughputStddev)
					outcome = TestOutcome::Regressed;
			}

			if (ro_Result.isHotspotRun) {
				if (internalRatioDegraded(history.readDivergenceSampleCount, history.readDivergenceRatioMean,
					history.readDivergenceRatioStddev, ro_Result.readDivergenceRatio)
					|| internalRatioDegraded(history.writeDivergenceSampleCount, history.writeDivergenceRatioMean,
						history.writeDivergenceRatioStddev, ro_Result.writeDivergenceRatio)) {
					outcome = TestOutcome::Degraded;   // Degraded > Regressed - always wins if tripped
				}
			}

			m_history.update(v_TestId, ro_Result.meanThroughput,
				ro_Result.isHotspotRun, ro_Result.readDivergenceRatio, ro_Result.writeDivergenceRatio);

			return outcome;
		}

	private:
		HADES_NODISCARD_MSG("Cannot discard ratio degradation check")
			bool internalRatioDegraded(uint32_t v_SampleCount, double v_HistMean, double v_HistStddev, double v_CurrentRatio) const noexcept {
			if (v_SampleCount < m_minHistoryFloor || v_HistStddev <= 0.0)
				return false;
			return std::abs(v_CurrentRatio - v_HistMean) > m_kStddev * v_HistStddev;
		}
	};

} // namespace Hades::Runtime
