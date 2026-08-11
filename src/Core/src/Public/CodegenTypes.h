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
#include "Configuration.h"

#include <string>
#include <vector>

// Shared, ABI-neutral request POD types passed across the Codegen boundary.
// Owned by Core (not Codegen) so that Driver and any external consumer (e.g.
// Spectra's SVK) can construct these without linking Codegen's DLL first.

namespace Hades::Runtime {

	// One registered fixture, resolved down to the four template arguments
	// HADES_REGISTER_FIXTURE needs - Codegen never inspects fixture C++ source,
	// it only ever templates these strings into the generated main.cpp.
	struct FixtureEntry final {
		std::string m_FixtureName;    // C++ class name, e.g. "RwLockContentionFixture"
		std::string m_HeaderPath;     // #include path for the fixture's own header
		std::string m_AdapterType;    // e.g. "CudaDeviceAdapter"
		std::string m_ChronoType;     // e.g. "RdtscChronoPoint"
		std::string m_HashType;       // e.g. "Fnv1aHashAccumulator"

		// Optional: lets a fixture benchmark an external CMake target outside
		// Hades-Benchmark itself (e.g. one of Spectra's own modules) without
		// Codegen needing to know anything about it beyond a path + target
		// name. Both empty (the common case) = no extra dependency. Not a doc
		// concept - added because nothing else lets the generated suite link
		// anything but Hades-Core.
		std::string m_ExtraLinkDir;      // path to the external target's CMakeLists.txt directory
		std::string m_ExtraLinkTarget;   // its CMake target name
	};

	// Input to IHadesCodegen::scaffold() - "new-test" subcommand. Describes a
	// single test being added to the suite currently adopted by Driver.
	struct ScaffoldRequest final {
		std::string m_SuiteDir;      // hades-gen/ root
		std::string m_TestId;        // suite.toml [[test]] id - unique key
		FixtureEntry m_Fixture;
		TestKind     m_Kind = TestKind::Performance;
	};

	// Input to IHadesCodegen::resolve() - "init-suite" subcommand. Describes the
	// whole suite: every registered fixture plus every test entry already read
	// out of suite.toml by Core (Codegen never parses toml itself).
	struct ResolveRequest final {
		std::string                    m_SuiteDir;      // hades-gen/ root
		std::vector<FixtureEntry>      m_Fixtures;       // one #include + registration per entry
		std::vector<SuiteTestEntry>    m_Tests;          // declaration-ordered, from suite.toml
	};

}
