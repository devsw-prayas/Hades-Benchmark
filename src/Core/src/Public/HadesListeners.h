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

#include <cstdio>
#include <string>

#include "BenchmarkResult.h"
#include "Storage.h"
#include "RegressionAnalyzer.h"

namespace Hades::Runtime {

	// Three independent taps on the same immutable completed result - none
	// feeds another. Modeled on gtest's TestEventListener, not lit's raw text
	// capture, since Hades already produces a typed BenchmarkResult.
	class IHadesListener {
	public:
		virtual ~IHadesListener() = default;

		virtual void onTestStart(const std::string& v_Id) = 0;
		virtual void onTestComplete(const std::string& v_Id, const BenchmarkResult& ro_Result) = 0;
		virtual void onSuiteComplete() = 0;

		IHadesListener(const IHadesListener&) = delete;
		IHadesListener& operator=(const IHadesListener&) = delete;
		IHadesListener(IHadesListener&&) = delete;
		IHadesListener& operator=(IHadesListener&&) = delete;

	protected:
		IHadesListener() = default;
	};

	// SuiteDriver is fully precompiled and cannot template on arbitrary
	// listener types (same constraint that forces IFixtureVirtual). Wraps a
	// plain duck-typed listener in the stable vtable at the call site -
	// instantiation is cheap, entirely outside the hot path.
	template<typename T>
	class ListenerSupport final : public IHadesListener {
		T& m_wrapped;
	public:
		explicit ListenerSupport(T& ro_Wrapped) noexcept : m_wrapped(ro_Wrapped) {}

		void onTestStart(const std::string& v_Id) override { m_wrapped.onTestStart(v_Id); }
		void onTestComplete(const std::string& v_Id, const BenchmarkResult& ro_Result) override {
			m_wrapped.onTestComplete(v_Id, ro_Result);
		}
		void onSuiteComplete() override { m_wrapped.onSuiteComplete(); }
	};

	// Human-readable pass/fail + summary stats to stdout. "Failed" here means
	// the run itself broke (discard caps exceeded or non-deterministic output) -
	// statistical regression severity (Regressed/Degraded) is RegressionListener's
	// concern, a separate tap on the same result, not decided here.
	class HADES_RUNTIME_API ConsoleListener final {
	public:
		void onTestStart(const std::string& v_Id) noexcept {
			std::printf("[ RUN      ] %s\n", v_Id.c_str());
		}

		void onTestComplete(const std::string& v_Id, const BenchmarkResult& ro_Result) noexcept {
			const bool passed = !ro_Result.runFailed && ro_Result.deterministic;
			std::printf("[ %s ] %s - %u slices, meanThroughput=%.3f, cv=%.4f\n",
				passed ? "PASSED" : "FAILED",
				v_Id.c_str(),
				ro_Result.sliceCount,
				ro_Result.meanThroughput,
				ro_Result.cv);

			if (!ro_Result.deterministic) {
				std::printf("    non-deterministic: %u unique hashes, first divergence at slice %u\n",
					ro_Result.uniqueHashCount, ro_Result.firstDivergenceSlice);
			}
		}

		void onSuiteComplete() noexcept {
			std::printf("[==========] suite complete\n");
		}
	};

	// Forwards each completed result to an IStorageBackend<D>. Storage is
	// the backend's own concern (memory, JSON, ...) - this listener is just
	// the wiring between the fan-out and store().
	template<typename D>
	class StorageListener final {
		using backend_ = IStorageBackend<D>;
		backend_& m_backend;

	public:
		explicit StorageListener(backend_& ro_Backend) noexcept : m_backend(ro_Backend) {}

		void onTestStart(const std::string&) noexcept {}

		void onTestComplete(const std::string&, const BenchmarkResult& ro_Result) noexcept {
			m_backend.store(ro_Result);
		}

		void onSuiteComplete() noexcept {}
	};

	// Forwards each completed result to RegressionAnalyzer::evaluate() and
	// tracks the worst outcome seen across the suite. Failed > Degraded >
	// Regressed > Pass - kept as distinct severities on purpose, per
	// RegressionAnalyzer; collapsing them here would force the caller to
	// re-derive the distinction from raw divergence ratios.
	template<typename H>
	class RegressionListener final {
		using analyzer_ = RegressionAnalyzer<H>;
		analyzer_& m_analyzer;
		TestOutcome m_worstOutcome = TestOutcome::Pass;

	public:
		explicit RegressionListener(analyzer_& ro_Analyzer) noexcept : m_analyzer(ro_Analyzer) {}

		void onTestStart(const std::string&) noexcept {}

		void onTestComplete(const std::string& v_Id, const BenchmarkResult& ro_Result) {
			const TestOutcome outcome = m_analyzer.evaluate(v_Id, ro_Result);
			if (static_cast<uint8_t>(outcome) > static_cast<uint8_t>(m_worstOutcome))
				m_worstOutcome = outcome;
		}

		void onSuiteComplete() noexcept {}

		HADES_NODISCARD_MSG("Cannot discard worst outcome")
			TestOutcome worstOutcome() const noexcept { return m_worstOutcome; }
	};

} // namespace Hades::Runtime
