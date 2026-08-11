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
#include "HadesCompiler.h"
#include "TomlValue.h"

#include <string>
#include <vector>

namespace Hades::Runtime {
	enum class HADES_RUNTIME_API ChronoBackend : uint8_t {
		Rdtsc,         // RdtscChronoPoint    - lowest overhead, rdtsc/rdtscp
		SteadyClock,   // SteadyClockChronoPoint - portable, frequency-scaling immune
	};

	// kind = "performance" (default) or "correctness" in suite.toml - explicit,
	// never inferred from the fixture's C++ type.
	enum class HADES_RUNTIME_API TestKind : uint8_t {
		Performance,   // full lifecycle, CV-converging slice loop, Pass/Regressed/Degraded/Failed
		Correctness,   // execute() called exactly once, Pass/Failed only
	};

	struct HADES_RUNTIME_API Config final {
		std::string   m_Id;   // unique key - history lookup + DAG-free ordering label
		TestKind      m_Kind = TestKind::Performance;

		uint64_t      m_Iterations = 0;   // 0 = calibration-derived
		ChronoBackend m_ChronoBackend = ChronoBackend::SteadyClock;

		uint32_t m_WarmupCount = 0;   // user-provided; not calibration-derived

		double   m_CvThreshold = 0.0;   // coefficient of variation target
		uint32_t m_MinSlices = 0;     // minimum slice floor before stopping
		uint32_t m_MaxSlices = 0;     // hard slice cap regardless of CV
		uint32_t m_ConsecutiveDiscardCap = 0;     // consecutive jitter-contaminated slice limit
		uint32_t m_TotalDiscardCap = 0;     // total jitter-contaminated slice limit

		// Hotspot-only fields (IHotspotFixture, Phase 2). Ignored for plain fixtures.
		uint32_t m_HelperThreadCount = 0;    // total helper threads spawned for Phase 2
		double   m_ReadCriticality = 0.0;    // fraction of helpers assigned the read() role
		uint32_t m_FixedSliceCount = 0;      // exact Phase 2 slice count - no CV check
	};

	HADES_FORCEINLINE HADES_RUNTIME_API void setId(Config& ro_Config, std::string v_Id) {
		ro_Config.m_Id = std::move(v_Id);
	}

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setKind(Config& ro_Config, TestKind v_Kind) {
		ro_Config.m_Kind = v_Kind;
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

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setHelperThreadCount(Config& ro_Config, uint32_t v_Count) {
		ro_Config.m_HelperThreadCount = v_Count;
	}

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setReadCriticality(Config& ro_Config, double v_Criticality) {
		ro_Config.m_ReadCriticality = v_Criticality;
	}

	HADES_FORCEINLINE HADES_RUNTIME_API constexpr void setFixedSliceCount(Config& ro_Config, uint32_t v_Count) {
		ro_Config.m_FixedSliceCount = v_Count;
	}

	// One [[test]] entry from suite.toml: the fixture it targets plus its
	// resolved Config. `fixture` has no home in Config itself (Config is a
	// pure timing/lifecycle knob set) - this is what ResolveRequest::m_Tests
	// actually needs to drive Codegen's generated main.cpp registrations.
	struct HADES_RUNTIME_API SuiteTestEntry final {
		std::string m_FixtureName;
		Config      m_Config;
	};

	// Calls readToml(v_Path, "test", ...) and maps each resulting TomlTable
	// onto a SuiteTestEntry via Config's setXxx() functions. `id` and
	// `fixture` are required; `kind` defaults to Performance if absent;
	// unknown keys are reported as errors.
	HADES_NODISCARD_MSG("Parse result must be checked")
		HADES_RUNTIME_API bool readSuiteToml(const std::string& v_Path,
		                                     std::vector<SuiteTestEntry>& ro_OutTests,
		                                     std::vector<TomlParseError>& ro_OutErrors);

	HADES_NODISCARD_MSG("Write result must be checked")
		HADES_RUNTIME_API bool writeSuiteToml(const std::string& v_Path,
		                                      const std::vector<SuiteTestEntry>& ro_Tests,
		                                      bool v_Append);
}
