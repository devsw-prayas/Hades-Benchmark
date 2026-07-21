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

#include <vector>

#include "Configuration.h"
#include "FixtureRegistry.h"
#include "HadesListeners.h"

namespace Hades::Runtime {

	// Precompiled, non-template - same bucket as FixtureRegistry/IHadesListener
	// (SS1.1). Loops over its queued tests in declaration order (no dependency
	// DAG - order of appearance is execution order, SS1.14), fanning each
	// completed result out to every listener. Touches fixtures only through
	// the IFixtureVirtual shim - never sees a concrete adapter/fixture type.
	class HADES_RUNTIME_API SuiteDriver final {
	public:
		SuiteDriver(const FixtureRegistry& ro_Registry, std::vector<SuiteTestEntry> v_Tests) noexcept;
		~SuiteDriver() = default;

		SuiteDriver(const SuiteDriver&) = delete;
		SuiteDriver& operator=(const SuiteDriver&) = delete;
		SuiteDriver(SuiteDriver&&) = delete;
		SuiteDriver& operator=(SuiteDriver&&) = delete;

		// An unknown fixture name produces a synthesized runFailed
		// BenchmarkResult rather than crashing the suite - one typo in
		// suite.toml shouldn't take down every other test.
		void run(const std::vector<IHadesListener*>& ro_Listeners) const;

	private:
		const FixtureRegistry&      m_registry;
		std::vector<SuiteTestEntry> m_tests;
	};

} // namespace Hades::Runtime
