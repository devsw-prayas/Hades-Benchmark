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
// Unit tests for HadesCodegenImpl::scaffold()/resolve() (Codegen/src/Private/
// HadesCodegen.cpp). Checks the generated main.cpp/CMakeLists.txt/fixture
// stub/toml fragment contain the expected literal text - does not attempt to
// actually compile the generated main.cpp (that's an integration-level
// concern for Driver's build backend instead).

#include "TestFramework.h"

#include <Configuration.h>
#include <CodegenInterface.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace Hades::Runtime;
using namespace Hades::Codegen;

namespace {

	std::string readWholeFile(const std::string& v_Path) {
		std::ifstream l_file(v_Path, std::ios::binary);
		std::ostringstream l_stream;
		l_stream << l_file.rdbuf();
		return l_stream.str();
	}

	bool contains(const std::string& ro_Haystack, const std::string& ro_Needle) {
		return ro_Haystack.find(ro_Needle) != std::string::npos;
	}

	FixtureEntry makeCounterFixtureEntry() {
		FixtureEntry l_fixture;
		l_fixture.m_FixtureName = "CounterFixture";
		l_fixture.m_HeaderPath = "CounterFixture.h";
		l_fixture.m_AdapterType = "NullDeviceAdapter";
		l_fixture.m_ChronoType = "Hades::Runtime::Chrono::SteadyClockChronoPoint";
		l_fixture.m_HashType = "XorHashAccumulator";
		return l_fixture;
	}

	void testResolveGeneratesMainCppAndBuildFile() {
		std::printf("testResolveGeneratesMainCppAndBuildFile\n");

		IHadesCodegen* p_codegen = createHadesCodegen();

		ResolveRequest l_request;
		l_request.m_SuiteDir = "hades-gen";
		l_request.m_Fixtures.push_back(makeCounterFixtureEntry());

		SuiteTestEntry l_test;
		l_test.m_FixtureName = "CounterFixture";
		l_test.m_Config.m_Id = "counter_test";
		l_test.m_Config.m_Kind = TestKind::Performance;
		l_test.m_Config.m_WarmupCount = 5;
		l_request.m_Tests.push_back(l_test);

		const std::string l_mainCppPath = "codegen_test_main.cpp";
		const std::string l_buildFilePath = "codegen_test_CMakeLists.txt";

		const bool l_ok = p_codegen->resolve(l_request, l_mainCppPath, l_buildFilePath);
		EXPECT_TRUE(l_ok);

		const std::string l_mainCpp = readWholeFile(l_mainCppPath);
		EXPECT_TRUE(contains(l_mainCpp, "#include \"CounterFixture.h\""));
		EXPECT_TRUE(contains(l_mainCpp,
			"HADES_REGISTER_FIXTURE(ro_Registry, CounterFixture, NullDeviceAdapter, "
			"Hades::Runtime::Chrono::SteadyClockChronoPoint, XorHashAccumulator)"));
		EXPECT_TRUE(contains(l_mainCpp, "l_entry.m_FixtureName = \"CounterFixture\""));
		EXPECT_TRUE(contains(l_mainCpp, "l_entry.m_Config.m_Id = \"counter_test\""));
		EXPECT_TRUE(contains(l_mainCpp, "l_entry.m_Config.m_WarmupCount = 5u"));
		EXPECT_TRUE(contains(l_mainCpp, "int main(int v_Argc, char** p_Argv)"));

		const std::string l_buildFile = readWholeFile(l_buildFilePath);
		EXPECT_TRUE(contains(l_buildFile, "project(hades-gen"));
		EXPECT_TRUE(contains(l_buildFile, "HADES_BENCHMARK_ROOT"));

		std::remove(l_mainCppPath.c_str());
		std::remove(l_buildFilePath.c_str());

		destroyHadesCodegen(p_codegen);
	}

	void testResolveRejectsUnknownFixtureReference() {
		std::printf("testResolveRejectsUnknownFixtureReference\n");

		IHadesCodegen* p_codegen = createHadesCodegen();

		ResolveRequest l_request;
		l_request.m_SuiteDir = "hades-gen";
		l_request.m_Fixtures.push_back(makeCounterFixtureEntry());

		SuiteTestEntry l_test;
		l_test.m_FixtureName = "DoesNotExistFixture";
		l_test.m_Config.m_Id = "missing_test";
		l_request.m_Tests.push_back(l_test);

		const std::string l_mainCppPath = "codegen_test_missing_main.cpp";
		const std::string l_buildFilePath = "codegen_test_missing_CMakeLists.txt";
		std::remove(l_mainCppPath.c_str());
		std::remove(l_buildFilePath.c_str());

		const bool l_ok = p_codegen->resolve(l_request, l_mainCppPath, l_buildFilePath);
		EXPECT_TRUE(!l_ok);

		destroyHadesCodegen(p_codegen);
	}

	void testScaffoldWritesStubAndTomlEntry() {
		std::printf("testScaffoldWritesStubAndTomlEntry\n");

		IHadesCodegen* p_codegen = createHadesCodegen();

		ScaffoldRequest l_request;
		l_request.m_SuiteDir = "hades-gen";
		l_request.m_TestId = "new_test";
		l_request.m_Fixture = makeCounterFixtureEntry();
		l_request.m_Kind = TestKind::Correctness;

		const std::string l_stubPath = "codegen_test_stub.h";
		const std::string l_tomlPath = "codegen_test_suite.toml";
		std::remove(l_tomlPath.c_str());

		const bool l_ok = p_codegen->scaffold(l_request, l_stubPath, l_tomlPath);
		EXPECT_TRUE(l_ok);

		const std::string l_stub = readWholeFile(l_stubPath);
		EXPECT_TRUE(contains(l_stub, "class CounterFixture final"));
		EXPECT_TRUE(contains(l_stub, "Hades::Runtime::IFixture<CounterFixture, NullDeviceAdapter>"));

		std::vector<SuiteTestEntry> l_tests;
		std::vector<TomlParseError> l_errors;
		const bool l_parsed = readSuiteToml(l_tomlPath, l_tests, l_errors);
		EXPECT_TRUE(l_parsed);
		EXPECT_EQ(l_tests.size(), 1u);
		if (l_tests.size() == 1) {
			EXPECT_EQ(l_tests[0].m_FixtureName, "CounterFixture");
			EXPECT_EQ(l_tests[0].m_Config.m_Id, "new_test");
			EXPECT_TRUE(l_tests[0].m_Config.m_Kind == TestKind::Correctness);
		}

		std::remove(l_stubPath.c_str());
		std::remove(l_tomlPath.c_str());

		destroyHadesCodegen(p_codegen);
	}

}

void runCodegenTests() {
	testResolveGeneratesMainCppAndBuildFile();
	testResolveRejectsUnknownFixtureReference();
	testScaffoldWritesStubAndTomlEntry();
}
