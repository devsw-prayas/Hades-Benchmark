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
#include "Commands.h"
#include "ProcessExec.h"
#include "SuiteContext.h"

#include <Configuration.h>
#include <CodegenInterface.h>
#include <RegistryIO.h>

#include <filesystem>
#include <fstream>
#include <iostream>

#ifndef HADES_BENCHMARK_ROOT
#define HADES_BENCHMARK_ROOT ""
#endif

namespace Hades::Driver {

	namespace {

		namespace fs = std::filesystem;

		bool parseFlag(const std::string& v_Arg, const std::string& v_Name, std::string& ro_OutValue) {
			const std::string l_prefix = "--" + v_Name + "=";
			if (v_Arg.rfind(l_prefix, 0) == 0) {
				ro_OutValue = v_Arg.substr(l_prefix.size());
				return true;
			}
			return false;
		}

		std::string suiteExecutableName(const std::string& v_SuiteDir) {
			const std::string l_name = fs::path(v_SuiteDir).filename().string();
			return l_name.empty() ? "hades_suite" : l_name;
		}

		// Re-derives the generated main.cpp/CMakeLists.txt from the suite's
		// current registry.ini + suite.toml - the single source of truth
		// `run`/`validate` regenerate from immediately before building, so
		// `new-test` doesn't need to duplicate this logic itself.
		bool regenerateSuite(const std::string& v_SuiteDir, std::string& ro_OutError) {
			std::vector<Hades::Runtime::FixtureEntry> l_fixtures;
			std::vector<Hades::Runtime::RegistryIniError> l_regErrors;
			if (!Hades::Runtime::readRegistryIni(v_SuiteDir + "/registry.ini", l_fixtures, l_regErrors)) {
				ro_OutError = "registry.ini: " + (l_regErrors.empty() ? std::string("parse failed") : l_regErrors.front().m_Message);
				return false;
			}

			std::vector<Hades::Runtime::SuiteTestEntry> l_tests;
			std::vector<Hades::Runtime::TomlParseError> l_tomlErrors;
			if (!Hades::Runtime::readSuiteToml(v_SuiteDir + "/suite.toml", l_tests, l_tomlErrors)) {
				ro_OutError = "suite.toml: " + (l_tomlErrors.empty() ? std::string("parse failed") : l_tomlErrors.front().m_Message);
				return false;
			}

			Hades::Runtime::ResolveRequest l_request;
			l_request.m_SuiteDir = v_SuiteDir;
			l_request.m_Fixtures = std::move(l_fixtures);
			l_request.m_Tests = std::move(l_tests);

			std::error_code l_ec;
			fs::create_directories(v_SuiteDir + "/generated", l_ec);

			Hades::Codegen::IHadesCodegen* p_codegen = ::createHadesCodegen();
			const bool l_ok = p_codegen->resolve(l_request, v_SuiteDir + "/generated/main.cpp", v_SuiteDir + "/generated/CMakeLists.txt");
			::destroyHadesCodegen(p_codegen);

			if (!l_ok) {
				ro_OutError = "Codegen::resolve() failed - check every suite.toml [[test]] 'fixture' matches a registry.ini section";
				return false;
			}
			return true;
		}

		std::string findGeneratedExecutable(const std::string& v_BuildDir, const std::string& v_Name) {
			const std::string l_candidates[] = {
				v_BuildDir + "/" + v_Name + ".exe",
				v_BuildDir + "/" + v_Name,
				v_BuildDir + "/Debug/" + v_Name + ".exe",
				v_BuildDir + "/Release/" + v_Name + ".exe",
				v_BuildDir + "/RelWithDebInfo/" + v_Name + ".exe",
			};
			for (const std::string& r_candidate : l_candidates) {
				if (fs::exists(r_candidate)) {
					return r_candidate;
				}
			}
			return "";
		}

		// `run --build=cmake` is the only backend actually wired up - Codegen
		// only ever emits a CMakeLists.txt, and `direct`/`ninja` have no
		// generator counterpart yet.
		int runCMakeConfigure(const std::string& v_GenDir, const std::string& v_BuildDir) {
			const std::string l_cmd = "cmake -S " + quotePath(v_GenDir) +
				" -B " + quotePath(v_BuildDir) +
				" -DHADES_BENCHMARK_ROOT=" + quotePath(HADES_BENCHMARK_ROOT);
			return runProcess(l_cmd);
		}

		int runCMakeBuild(const std::string& v_BuildDir) {
			return runProcess("cmake --build " + quotePath(v_BuildDir) + " --config Debug");
		}

	}

	int cmdInitSuite(const std::vector<std::string>& v_Args) {
		if (v_Args.empty()) {
			std::cerr << "usage: init-suite <dir>\n";
			return 2;
		}
		const std::string l_dir = v_Args[0];

		std::error_code l_ec;
		fs::create_directories(l_dir + "/tests", l_ec);
		if (l_ec) {
			std::cerr << "error: could not create '" << l_dir << "': " << l_ec.message() << "\n";
			return 1;
		}

		const std::string l_suiteToml = l_dir + "/suite.toml";
		const std::string l_registryIni = l_dir + "/registry.ini";
		if (!fs::exists(l_suiteToml)) {
			std::ofstream(l_suiteToml, std::ios::binary) << "# Hades suite manifest\n";
		}
		if (!fs::exists(l_registryIni)) {
			std::ofstream(l_registryIni, std::ios::binary) << "# Hades fixture registry - one [FixtureName] section per fixture\n";
		}

		if (!adoptSuite(l_dir)) {
			return 1;
		}

		std::string l_error;
		if (!regenerateSuite(l_dir, l_error)) {
			std::cerr << "error: " << l_error << "\n";
			return 1;
		}

		std::cout << "initialized suite at '" << l_dir << "' and adopted it as the current suite\n";
		return 0;
	}

	int cmdFindSuite(const std::vector<std::string>& v_Args) {
		if (v_Args.empty()) {
			std::cerr << "usage: find-suite <dir>\n";
			return 2;
		}
		const std::string l_dir = v_Args[0];

		if (!fs::exists(fs::path(l_dir) / "suite.toml")) {
			std::cerr << "error: '" << l_dir << "' does not look like a hades-gen suite (missing suite.toml)\n";
			return 1;
		}
		if (!adoptSuite(l_dir)) {
			return 1;
		}

		std::cout << "adopted suite at '" << l_dir << "'\n";
		return 0;
	}

	int cmdNewTest(const std::vector<std::string>& v_Args) {
		if (v_Args.size() < 2) {
			std::cerr << "usage: new-test <fixture-name> <test-id> [--header=<path>] [--adapter=<type>] "
			             "[--chrono=<type>] [--hash=<type>] [--kind=performance|correctness] "
			             "[--extra-link-dir=<path>] [--extra-link-target=<cmake-target>]\n";
			return 2;
		}
		const std::string l_fixtureName = v_Args[0];
		const std::string l_testId = v_Args[1];

		std::string l_header, l_adapter, l_chrono, l_hash, l_kindStr = "performance";
		std::string l_extraLinkDir, l_extraLinkTarget;
		for (size_t l_i = 2; l_i < v_Args.size(); ++l_i) {
			if (parseFlag(v_Args[l_i], "header", l_header))  continue;
			if (parseFlag(v_Args[l_i], "adapter", l_adapter)) continue;
			if (parseFlag(v_Args[l_i], "chrono", l_chrono))  continue;
			if (parseFlag(v_Args[l_i], "hash", l_hash))      continue;
			if (parseFlag(v_Args[l_i], "kind", l_kindStr))   continue;
			if (parseFlag(v_Args[l_i], "extra-link-dir", l_extraLinkDir))       continue;
			if (parseFlag(v_Args[l_i], "extra-link-target", l_extraLinkTarget)) continue;
			std::cerr << "warning: unrecognized flag '" << v_Args[l_i] << "'\n";
		}

		std::string l_suiteDir;
		if (!loadCurrentSuite(l_suiteDir)) {
			return 1;
		}

		std::vector<Hades::Runtime::FixtureEntry> l_fixtures;
		std::vector<Hades::Runtime::RegistryIniError> l_regErrors;
		if (!Hades::Runtime::readRegistryIni(l_suiteDir + "/registry.ini", l_fixtures, l_regErrors)) {
			std::cerr << "error: could not read registry.ini\n";
			return 1;
		}

		const Hades::Runtime::FixtureEntry* p_existing = nullptr;
		for (const Hades::Runtime::FixtureEntry& r_fixture : l_fixtures) {
			if (r_fixture.m_FixtureName == l_fixtureName) {
				p_existing = &r_fixture;
				break;
			}
		}

		Hades::Runtime::FixtureEntry l_entry;
		const bool l_isNewFixture = (p_existing == nullptr);
		if (!l_isNewFixture) {
			l_entry = *p_existing;
		} else if (l_header.empty() || l_adapter.empty() || l_chrono.empty() || l_hash.empty()) {
			std::cerr << "error: fixture '" << l_fixtureName << "' is not yet in registry.ini - "
			             "--header, --adapter, --chrono and --hash are all required the first time\n";
			return 2;
		} else {
			if (!Hades::Runtime::isIdentifier(l_fixtureName)) {
				std::cerr << "error: '" << l_fixtureName << "' is not a valid C++ identifier - it becomes the generated fixture class name\n";
				return 2;
			}
			if (!Hades::Runtime::isCppTypeName(l_adapter)) {
				std::cerr << "error: --adapter='" << l_adapter << "' is not a valid (possibly ::-qualified) C++ type name\n";
				return 2;
			}
			if (!Hades::Runtime::isCppTypeName(l_chrono)) {
				std::cerr << "error: --chrono='" << l_chrono << "' is not a valid (possibly ::-qualified) C++ type name\n";
				return 2;
			}
			if (!Hades::Runtime::isCppTypeName(l_hash)) {
				std::cerr << "error: --hash='" << l_hash << "' is not a valid (possibly ::-qualified) C++ type name\n";
				return 2;
			}
			if (!l_extraLinkTarget.empty() && !Hades::Runtime::isIdentifier(l_extraLinkTarget)) {
				std::cerr << "error: --extra-link-target='" << l_extraLinkTarget << "' is not a valid CMake target identifier\n";
				return 2;
			}

			l_entry.m_FixtureName = l_fixtureName;
			l_entry.m_HeaderPath = l_header;
			l_entry.m_AdapterType = l_adapter;
			l_entry.m_ChronoType = l_chrono;
			l_entry.m_HashType = l_hash;
			l_entry.m_ExtraLinkDir = l_extraLinkDir;
			l_entry.m_ExtraLinkTarget = l_extraLinkTarget;
		}

		Hades::Runtime::ScaffoldRequest l_request;
		l_request.m_SuiteDir = l_suiteDir;
		l_request.m_TestId = l_testId;
		l_request.m_Fixture = l_entry;
		l_request.m_Kind = (l_kindStr == "correctness") ? Hades::Runtime::TestKind::Correctness : Hades::Runtime::TestKind::Performance;

		const std::string l_testDir = l_suiteDir + "/tests/" + l_testId;
		std::error_code l_ec;
		fs::create_directories(l_testDir, l_ec);
		const std::string l_stubPath = l_testDir + "/" + l_fixtureName + ".h";

		Hades::Codegen::IHadesCodegen* p_codegen = ::createHadesCodegen();
		const bool l_ok = p_codegen->scaffold(l_request, l_stubPath, l_suiteDir + "/suite.toml");
		::destroyHadesCodegen(p_codegen);

		if (!l_ok) {
			std::cerr << "error: scaffold() failed\n";
			return 1;
		}
		if (l_isNewFixture && !Hades::Runtime::writeRegistryIni(l_suiteDir + "/registry.ini", { l_entry }, /*v_Append=*/true)) {
			std::cerr << "error: could not append to registry.ini\n";
			return 1;
		}

		std::cout << "scaffolded test '" << l_testId << "' (" << l_fixtureName << ") -> " << l_stubPath << "\n";
		std::cout << "run 'run' to regenerate and build the suite\n";
		return 0;
	}

	int cmdRun(const std::vector<std::string>& v_Args) {
		std::string l_build = "cmake", l_fbt, l_format = "console";
		for (const std::string& r_arg : v_Args) {
			std::string l_value;
			if (parseFlag(r_arg, "build", l_value))  { l_build = l_value;  continue; }
			if (parseFlag(r_arg, "fbt", l_value))    { l_fbt = l_value;   continue; }
			if (parseFlag(r_arg, "format", l_value)) { l_format = l_value; continue; }
			std::cerr << "warning: unrecognized flag '" << r_arg << "'\n";
		}
		if (l_build != "cmake") {
			std::cerr << "error: --build=" << l_build << " is not yet implemented (only 'cmake' is supported)\n";
			return 2;
		}

		std::string l_suiteDir;
		if (!loadCurrentSuite(l_suiteDir)) {
			return 1;
		}

		std::string l_error;
		if (!regenerateSuite(l_suiteDir, l_error)) {
			std::cerr << "error: " << l_error << "\n";
			return 1;
		}

		const std::string l_genDir = l_suiteDir + "/generated";
		const std::string l_buildDir = l_genDir + "/build";
		if (runCMakeConfigure(l_genDir, l_buildDir) != 0) {
			std::cerr << "error: cmake configure failed\n";
			return 1;
		}
		if (runCMakeBuild(l_buildDir) != 0) {
			std::cerr << "error: cmake build failed\n";
			return 1;
		}

		const std::string l_exePath = findGeneratedExecutable(l_buildDir, suiteExecutableName(l_suiteDir));
		if (l_exePath.empty()) {
			std::cerr << "error: could not locate the built suite executable under '" << l_buildDir << "'\n";
			return 1;
		}

		std::string l_cmd = quotePath(l_exePath);
		if (!l_fbt.empty()) {
			l_cmd += " --fbt=" + l_fbt;
		}
		l_cmd += " --format=" + l_format;
		return runProcess(l_cmd);
	}

	int cmdValidate(const std::vector<std::string>& v_Args) {
		std::string l_build = "cmake";
		for (const std::string& r_arg : v_Args) {
			std::string l_value;
			if (parseFlag(r_arg, "build", l_value)) {
				l_build = l_value;
			}
		}
		if (l_build != "cmake") {
			std::cerr << "error: --build=" << l_build << " is not yet implemented (only 'cmake' is supported)\n";
			return 2;
		}

		std::string l_suiteDir;
		if (!loadCurrentSuite(l_suiteDir)) {
			return 1;
		}

		std::string l_error;
		if (!regenerateSuite(l_suiteDir, l_error)) {
			std::cerr << "error: " << l_error << "\n";
			return 1;
		}

		const std::string l_genDir = l_suiteDir + "/generated";
		const std::string l_tempBuildDir = l_genDir + "/.validate_tmp";
		const std::string l_runBuildDir = l_genDir + "/build";

		std::error_code l_ec;
		fs::remove_all(l_tempBuildDir, l_ec);

		if (runCMakeConfigure(l_genDir, l_tempBuildDir) != 0 || runCMakeBuild(l_tempBuildDir) != 0) {
			std::cerr << "validate: build failed, cleaning up\n";
			fs::remove_all(l_tempBuildDir, l_ec);
			return 1;
		}

		fs::remove_all(l_runBuildDir, l_ec);
		fs::rename(l_tempBuildDir, l_runBuildDir, l_ec);
		if (l_ec) {
			std::cerr << "validate: build succeeded but promotion into run's cache failed: " << l_ec.message() << "\n";
			return 1;
		}

		std::cout << "validate: build integrity OK, promoted into run's cache\n";
		return 0;
	}

}
