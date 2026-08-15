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

// Owned by Core, not Codegen, so Driver/external consumers (e.g. Spectra's SVK) can build these without linking Codegen's DLL.
namespace Hades::Runtime {

	// Resolved down to the four template args HADES_REGISTER_FIXTURE needs - Codegen never inspects fixture C++ source.
	struct FixtureEntry final {
		std::string m_FixtureName;    // C++ class name, e.g. "RwLockContentionFixture"
		std::string m_HeaderPath;     // #include path for the fixture's own header
		std::string m_AdapterType;    // e.g. "CudaDeviceAdapter"
		std::string m_ChronoType;     // e.g. "RdtscChronoPoint"
		std::string m_HashType;       // e.g. "Fnv1aHashAccumulator"

		// Optional: lets a fixture link an external CMake target beyond Hades-Core; both empty = no extra dependency.
		std::string m_ExtraLinkDir;      // path to the external target's CMakeLists.txt directory
		std::string m_ExtraLinkTarget;   // its CMake target name
	};

	// Input to IHadesCodegen::scaffold() ("new-test"): one test being added to Driver's currently adopted suite.
	struct ScaffoldRequest final {
		std::string m_SuiteDir;      // hades-gen/ root
		std::string m_TestId;        // suite.toml [[test]] id - unique key
		FixtureEntry m_Fixture;
		TestKind     m_Kind = TestKind::Performance;
	};

	// Input to IHadesCodegen::resolve() ("init-suite"): every fixture + every test entry already read from suite.toml.
	struct ResolveRequest final {
		std::string                    m_SuiteDir;      // hades-gen/ root
		std::vector<FixtureEntry>      m_Fixtures;       // one #include + registration per entry
		std::vector<SuiteTestEntry>    m_Tests;          // declaration-ordered, from suite.toml
	};

}
