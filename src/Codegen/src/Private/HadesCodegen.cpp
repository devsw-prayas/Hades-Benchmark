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
#include "HadesCodegen.h"
#include <CodegenInterface.h>
#include <HadesDiagnostics.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

// Concrete Codegen implementation. Write-only, stateless: every string built
// here comes straight from the request POD - never from re-reading suite.toml
// or registry.ini (that already happened in Core, before Driver built the
// request).
namespace Hades::Codegen {

	namespace {

		std::string escapeForCppLiteral(const std::string& ro_Text) {
			std::string l_out;
			l_out.reserve(ro_Text.size());
			for (const char l_char : ro_Text) {
				if (l_char == '"' || l_char == '\\') {
					l_out.push_back('\\');
				}
				l_out.push_back(l_char);
			}
			return l_out;
		}

		bool writeTextFile(const std::string& v_Path, const std::string& ro_Content) {
			std::ofstream l_file(v_Path, std::ios::binary | std::ios::trunc);
			if (!l_file.is_open()) {
				return false;
			}
			l_file.write(ro_Content.data(), static_cast<std::streamsize>(ro_Content.size()));
			return static_cast<bool>(l_file);
		}

		// Nothing structurally forces Fixtures/Tests to agree - a suite.toml
		// typo would otherwise only surface at generated-binary runtime (as
		// SuiteDriver's synthesized runFailed). Catching it at generation time
		// is cheap and gives a much earlier signal.
		bool validateFixtureReferences(const Hades::Runtime::ResolveRequest& ro_Request) {
			for (const Hades::Runtime::SuiteTestEntry& r_test : ro_Request.m_Tests) {
				const bool l_found = std::any_of(ro_Request.m_Fixtures.begin(), ro_Request.m_Fixtures.end(),
					[&r_test](const Hades::Runtime::FixtureEntry& r_fixture) {
						return r_fixture.m_FixtureName == r_test.m_FixtureName;
					});
				if (!l_found) {
					return false;
				}
			}
			return true;
		}

		// ScaffoldRequest carries no header path for AdapterType/ChronoType/
		// HashType (only the fixture's own m_HeaderPath) - the stub leaves an
		// explicit TODO rather than guessing an #include path.
		std::string buildFixtureStub(const Hades::Runtime::FixtureEntry& ro_Fixture) {
			const std::string& l_name = ro_Fixture.m_FixtureName;
			const std::string& l_adapter = ro_Fixture.m_AdapterType;

			std::string l_out;
			l_out += "#pragma once\n#include <Fixture.h>\n\n";
			l_out += "// TODO: #include the header declaring " + l_adapter + " before building.\n\n";
			l_out += "class " + l_name + " final\n";
			l_out += "\t: public Hades::Runtime::IFixture<" + l_name + ", " + l_adapter + "> {\n";
			l_out += "public:\n";
			l_out += "\texplicit " + l_name + "(" + l_adapter + "& ro_Adapter) noexcept\n";
			l_out += "\t\t: IFixture(ro_Adapter) {\n\t}\n\n";
			l_out += "\tvoid startupImpl() noexcept {\n\t\t// TODO: PCIe host->device transfers (GPU fixtures) belong here.\n\t}\n\n";
			l_out += "\tvoid executeImpl() noexcept {\n\t\t// TODO: the timed operation.\n\t}\n\n";
			l_out += "\tvoid resetImpl(" + l_adapter + "& ro_Adapter) noexcept {\n\t\t(void)ro_Adapter;\n\t\t// TODO: state reset between slice iterations.\n\t}\n\n";
			l_out += "\tvoid teardownImpl() noexcept {\n\t\t// TODO: teardown.\n\t}\n\n";
			l_out += "\tuint64_t getDeterminismHashImpl() noexcept {\n\t\t// TODO: return a semantically meaningful hash of output state.\n\t\treturn 0;\n\t}\n";
			l_out += "};\n";
			return l_out;
		}

		std::string buildIncludes(const Hades::Runtime::ResolveRequest& ro_Request) {
			std::string l_out;
			l_out += "#include <FixtureRegistry.h>\n";
			l_out += "#include <SuiteDriver.h>\n";
			l_out += "#include <HadesListeners.h>\n";
			l_out += "#include <Configuration.h>\n\n";
			for (const Hades::Runtime::FixtureEntry& r_fixture : ro_Request.m_Fixtures) {
				l_out += "#include \"" + escapeForCppLiteral(r_fixture.m_HeaderPath) + "\"\n";
			}
			l_out += "\n";
			return l_out;
		}

		std::string buildRegisterFunction(const Hades::Runtime::ResolveRequest& ro_Request) {
			std::string l_out = "void registerAllFixtures(Hades::Runtime::FixtureRegistry& ro_Registry) {\n";
			for (const Hades::Runtime::FixtureEntry& r_fixture : ro_Request.m_Fixtures) {
				l_out += "\tHADES_REGISTER_FIXTURE(ro_Registry, " + r_fixture.m_FixtureName + ", " +
					r_fixture.m_AdapterType + ", " + r_fixture.m_ChronoType + ", " + r_fixture.m_HashType + ");\n";
			}
			l_out += "}\n\n";
			return l_out;
		}

		// Only non-default fields are emitted, matching writeSuiteToml's own
		// sparsity convention (Configuration.cpp's entryToTable) - 0 means
		// "calibration-derived", not a real override.
		std::string appendConfigAssignments(const Hades::Runtime::Config& ro_Config) {
			using Hades::Runtime::TestKind;
			using Hades::Runtime::ChronoBackend;

			std::string l_out;
			l_out += "\t\tl_entry.m_Config.m_Id = \"" + escapeForCppLiteral(ro_Config.m_Id) + "\";\n";
			l_out += std::string("\t\tl_entry.m_Config.m_Kind = Hades::Runtime::TestKind::") +
				(ro_Config.m_Kind == TestKind::Correctness ? "Correctness" : "Performance") + ";\n";

			if (ro_Config.m_Iterations != 0) {
				l_out += "\t\tl_entry.m_Config.m_Iterations = " + std::to_string(ro_Config.m_Iterations) + "ull;\n";
			}
			if (ro_Config.m_ChronoBackend == ChronoBackend::Rdtsc) {
				l_out += "\t\tl_entry.m_Config.m_ChronoBackend = Hades::Runtime::ChronoBackend::Rdtsc;\n";
			}
			if (ro_Config.m_WarmupCount != 0) {
				l_out += "\t\tl_entry.m_Config.m_WarmupCount = " + std::to_string(ro_Config.m_WarmupCount) + "u;\n";
			}
			if (ro_Config.m_CvThreshold != 0.0) {
				l_out += "\t\tl_entry.m_Config.m_CvThreshold = " + std::to_string(ro_Config.m_CvThreshold) + ";\n";
			}
			if (ro_Config.m_MinSlices != 0) {
				l_out += "\t\tl_entry.m_Config.m_MinSlices = " + std::to_string(ro_Config.m_MinSlices) + "u;\n";
			}
			if (ro_Config.m_MaxSlices != 0) {
				l_out += "\t\tl_entry.m_Config.m_MaxSlices = " + std::to_string(ro_Config.m_MaxSlices) + "u;\n";
			}
			if (ro_Config.m_ConsecutiveDiscardCap != 0) {
				l_out += "\t\tl_entry.m_Config.m_ConsecutiveDiscardCap = " + std::to_string(ro_Config.m_ConsecutiveDiscardCap) + "u;\n";
			}
			if (ro_Config.m_TotalDiscardCap != 0) {
				l_out += "\t\tl_entry.m_Config.m_TotalDiscardCap = " + std::to_string(ro_Config.m_TotalDiscardCap) + "u;\n";
			}
			if (ro_Config.m_HelperThreadCount != 0) {
				l_out += "\t\tl_entry.m_Config.m_HelperThreadCount = " + std::to_string(ro_Config.m_HelperThreadCount) + "u;\n";
			}
			if (ro_Config.m_ReadCriticality != 0.0) {
				l_out += "\t\tl_entry.m_Config.m_ReadCriticality = " + std::to_string(ro_Config.m_ReadCriticality) + ";\n";
			}
			if (ro_Config.m_FixedSliceCount != 0) {
				l_out += "\t\tl_entry.m_Config.m_FixedSliceCount = " + std::to_string(ro_Config.m_FixedSliceCount) + "u;\n";
			}
			return l_out;
		}

		std::string buildSuiteTestsFunction(const Hades::Runtime::ResolveRequest& ro_Request) {
			std::string l_out = "std::vector<Hades::Runtime::SuiteTestEntry> buildSuiteTests() {\n";
			l_out += "\tstd::vector<Hades::Runtime::SuiteTestEntry> l_tests;\n";
			for (const Hades::Runtime::SuiteTestEntry& r_test : ro_Request.m_Tests) {
				l_out += "\t{\n";
				l_out += "\t\tHades::Runtime::SuiteTestEntry l_entry;\n";
				l_out += "\t\tl_entry.m_FixtureName = \"" + escapeForCppLiteral(r_test.m_FixtureName) + "\";\n";
				l_out += appendConfigAssignments(r_test.m_Config);
				l_out += "\t\tl_tests.push_back(std::move(l_entry));\n";
				l_out += "\t}\n";
			}
			l_out += "\treturn l_tests;\n}\n\n";
			return l_out;
		}

		// ConsoleListener is always wired (no extra backend needed). Storage/
		// Regression listeners need a concrete IStorageBackend/RegressionHistory
		// backend the ResolveRequest has no way to name yet - deliberately not
		// templated in until Core grows a durable (e.g. JSON) backend selection
		// story. ExitCodeListener is the minimal stand-in that still gives
		// Driver's subprocess model a real pass/fail exit code in the meantime.
		// JsonListener/wildcardMatch exist so Driver's `run --fbt=`/`--format=`
		// flags (forwarded verbatim as argv to this binary) have something to
		// actually act on without any extra Core dependency.
		constexpr const char* MAIN_BODY_TEMPLATE = R"(namespace {

	// Simple '*'-glob matcher (greedy with backtracking) - good enough for
	// --fbt test-id filters, not a general regex.
	bool wildcardMatch(const std::string& v_Pattern, const std::string& v_Text) {
		size_t l_p = 0, l_t = 0, l_star = std::string::npos, l_mark = 0;
		while (l_t < v_Text.size()) {
			if (l_p < v_Pattern.size() && v_Pattern[l_p] == v_Text[l_t]) {
				++l_p; ++l_t;
			} else if (l_p < v_Pattern.size() && v_Pattern[l_p] == '*') {
				l_star = l_p++;
				l_mark = l_t;
			} else if (l_star != std::string::npos) {
				l_p = l_star + 1;
				l_t = ++l_mark;
			} else {
				return false;
			}
		}
		while (l_p < v_Pattern.size() && v_Pattern[l_p] == '*') {
			++l_p;
		}
		return l_p == v_Pattern.size();
	}

	struct ExitCodeListener final {
		bool m_anyFailed = false;

		void onTestStart(const std::string&) noexcept {}

		void onTestComplete(const std::string&, const Hades::Runtime::BenchmarkResult& ro_Result) noexcept {
			if (ro_Result.runFailed || !ro_Result.deterministic) {
				m_anyFailed = true;
			}
		}

		void onSuiteComplete() noexcept {}
	};

	// Minimal --format=json tap - prints a JSON array of completed results.
	// Array brackets are printed by main() around driver.run(), not here,
	// since onSuiteComplete() has no "is this the last listener" signal.
	struct JsonListener final {
		bool m_first = true;

		void onTestStart(const std::string&) noexcept {}

		void onTestComplete(const std::string& v_Id, const Hades::Runtime::BenchmarkResult& ro_Result) noexcept {
			if (!m_first) {
				std::printf(",\n");
			}
			m_first = false;
			const bool l_passed = !ro_Result.runFailed && ro_Result.deterministic;
			std::printf("  {\"id\": \"%s\", \"passed\": %s, \"meanThroughput\": %.6f, \"cv\": %.6f, \"sliceCount\": %u}",
				v_Id.c_str(), l_passed ? "true" : "false",
				ro_Result.meanThroughput, ro_Result.cv, ro_Result.sliceCount);
		}

		void onSuiteComplete() noexcept {}
	};

}

int main(int v_Argc, char** p_Argv) {
	std::string l_fbtPattern;
	std::string l_format = "console";
	for (int l_i = 1; l_i < v_Argc; ++l_i) {
		const std::string l_arg = p_Argv[l_i];
		if (l_arg.rfind("--fbt=", 0) == 0) {
			l_fbtPattern = l_arg.substr(6);
		} else if (l_arg.rfind("--format=", 0) == 0) {
			l_format = l_arg.substr(9);
		}
	}

	Hades::Runtime::FixtureRegistry l_registry;
	registerAllFixtures(l_registry);

	std::vector<Hades::Runtime::SuiteTestEntry> l_tests;
	for (Hades::Runtime::SuiteTestEntry& r_test : buildSuiteTests()) {
		if (l_fbtPattern.empty() || wildcardMatch(l_fbtPattern, r_test.m_Config.m_Id)) {
			l_tests.push_back(std::move(r_test));
		}
	}

	Hades::Runtime::SuiteDriver l_driver(l_registry, std::move(l_tests));

	Hades::Runtime::ConsoleListener l_console;
	Hades::Runtime::ListenerSupport<Hades::Runtime::ConsoleListener> l_consoleTap(l_console);
	JsonListener l_json;
	Hades::Runtime::ListenerSupport<JsonListener> l_jsonTap(l_json);

	ExitCodeListener l_exitTracker;
	Hades::Runtime::ListenerSupport<ExitCodeListener> l_exitTap(l_exitTracker);

	const bool l_useJson = (l_format == "json");
	std::vector<Hades::Runtime::IHadesListener*> l_listeners;
	l_listeners.push_back(l_useJson ? static_cast<Hades::Runtime::IHadesListener*>(&l_jsonTap)
	                                 : static_cast<Hades::Runtime::IHadesListener*>(&l_consoleTap));
	l_listeners.push_back(&l_exitTap);

	if (l_useJson) {
		std::printf("[\n");
	}
	l_driver.run(l_listeners);
	if (l_useJson) {
		std::printf("\n]\n");
	}

	return l_exitTracker.m_anyFailed ? 1 : 0;
}
)";

		std::string buildMainCpp(const Hades::Runtime::ResolveRequest& ro_Request) {
			std::string l_out;
			l_out += "// GENERATED by Hades Codegen -- do not edit by hand.\n";
			l_out += "// Regenerate via IHadesCodegen::resolve() after changing suite.toml or\n";
			l_out += "// registry.ini.\n\n";
			l_out += "#include <cstdio>\n#include <string>\n#include <vector>\n\n";
			l_out += buildIncludes(ro_Request);
			l_out += buildRegisterFunction(ro_Request);
			l_out += buildSuiteTestsFunction(ro_Request);
			l_out += MAIN_BODY_TEMPLATE;
			return l_out;
		}

		std::string suiteExecutableName(const std::string& v_SuiteDir) {
			std::string l_path = v_SuiteDir;
			std::replace(l_path.begin(), l_path.end(), '\\', '/');
			while (!l_path.empty() && l_path.back() == '/') {
				l_path.pop_back();
			}
			const size_t l_slash = l_path.find_last_of('/');
			std::string l_name = (l_slash == std::string::npos) ? l_path : l_path.substr(l_slash + 1);
			return l_name.empty() ? "hades_suite" : l_name;
		}

		// Unique (dir, target) pairs across every fixture - two fixtures naming
		// the same external module must not add_subdirectory it twice (CMake
		// errors on a duplicate target name).
		std::vector<std::pair<std::string, std::string>> collectExtraLinkTargets(const Hades::Runtime::ResolveRequest& ro_Request) {
			std::vector<std::pair<std::string, std::string>> l_out;
			for (const Hades::Runtime::FixtureEntry& r_fixture : ro_Request.m_Fixtures) {
				if (r_fixture.m_ExtraLinkDir.empty() || r_fixture.m_ExtraLinkTarget.empty()) {
					continue;
				}
				const auto l_matches = [&r_fixture](const std::pair<std::string, std::string>& ro_Pair) {
					return ro_Pair.second == r_fixture.m_ExtraLinkTarget;
				};
				if (std::find_if(l_out.begin(), l_out.end(), l_matches) == l_out.end()) {
					l_out.emplace_back(r_fixture.m_ExtraLinkDir, r_fixture.m_ExtraLinkTarget);
				}
			}
			return l_out;
		}

		// Extra external targets (registry.ini's extra_link_dir/extra_link_target,
		// see CodegenTypes.h) are add_subdirectory'd, linked, and - if built
		// SHARED - copied next to the suite executable post-build,
		// since nothing else would put their DLL on the generated binary's path.
		std::string buildCMakeLists(const Hades::Runtime::ResolveRequest& ro_Request) {
			const std::string l_name = suiteExecutableName(ro_Request.m_SuiteDir);
			const std::vector<std::pair<std::string, std::string>> l_extraTargets = collectExtraLinkTargets(ro_Request);

			std::string l_out;
			l_out += "# GENERATED by Hades Codegen -- do not edit by hand.\n";
			l_out += "cmake_minimum_required(VERSION 3.20)\n";
			l_out += "project(" + l_name + " LANGUAGES CXX)\n\n";
			l_out += "set(CMAKE_CXX_STANDARD 20)\n";
			l_out += "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n";
			l_out += "# Driver's cmake build backend must supply HADES_BENCHMARK_ROOT (path to\n";
			l_out += "# the Hades-Benchmark repo root) so this suite can link against Hades-Core.\n";
			l_out += "if(NOT DEFINED HADES_BENCHMARK_ROOT)\n";
			l_out += "\tmessage(FATAL_ERROR \"HADES_BENCHMARK_ROOT must be set (path to Hades-Benchmark repo root)\")\n";
			l_out += "endif()\n\n";
			l_out += "add_subdirectory(${HADES_BENCHMARK_ROOT}/src/Core ${CMAKE_CURRENT_BINARY_DIR}/hades-core)\n";

			for (size_t l_i = 0; l_i < l_extraTargets.size(); ++l_i) {
				// Quoted: unlike ${HADES_BENCHMARK_ROOT}/src/Core above (one
				// whitespace-free token in source even though it expands to a
				// path with spaces), this path is a literal string here and CMake
				// splits unquoted arguments on the literal source text.
				l_out += "add_subdirectory(\"" + l_extraTargets[l_i].first + "\" \"${CMAKE_CURRENT_BINARY_DIR}/extra-" + std::to_string(l_i) + "\")\n";
			}
			l_out += "\n";

			l_out += "add_executable(" + l_name + " main.cpp)\n";
			l_out += "target_link_libraries(" + l_name + " PRIVATE Hades-Core";
			for (const auto& r_target : l_extraTargets) {
				l_out += " " + r_target.second;
			}
			l_out += ")\n";

			// Hades-Core is unconditionally SHARED, so its DLL needs the same
			// post-build copy as any extra_link_target - CMake's generator
			// expressions resolve $<TARGET_FILE:...> to the matching .lib for a
			// STATIC target, so this stays a harmless no-op if Hades-Core ever
			// reverts to STATIC.
			l_out += "add_custom_command(TARGET " + l_name + " POST_BUILD\n";
			l_out += "\tCOMMAND ${CMAKE_COMMAND} -E copy_if_different\n";
			l_out += "\t\t$<TARGET_FILE:Hades-Core> $<TARGET_FILE_DIR:" + l_name + ">\n";
			l_out += "\tCOMMAND_EXPAND_LISTS)\n";

			for (const auto& r_target : l_extraTargets) {
				l_out += "add_custom_command(TARGET " + l_name + " POST_BUILD\n";
				l_out += "\tCOMMAND ${CMAKE_COMMAND} -E copy_if_different\n";
				l_out += "\t\t$<TARGET_FILE:" + r_target.second + "> $<TARGET_FILE_DIR:" + l_name + ">\n";
				l_out += "\tCOMMAND_EXPAND_LISTS)\n";
			}
			return l_out;
		}

		class HadesCodegenImpl final : public IHadesCodegen {
		public:
			bool scaffold(const Hades::Runtime::ScaffoldRequest& ro_Request,
			              const std::string& v_OutStubPath,
			              const std::string& v_OutTomlPath) override {
				if (!writeTextFile(v_OutStubPath, buildFixtureStub(ro_Request.m_Fixture))) {
					return false;
				}

				Hades::Runtime::SuiteTestEntry l_entry;
				l_entry.m_FixtureName = ro_Request.m_Fixture.m_FixtureName;
				l_entry.m_Config.m_Id = ro_Request.m_TestId;
				l_entry.m_Config.m_Kind = ro_Request.m_Kind;

				return Hades::Runtime::writeSuiteToml(v_OutTomlPath, { l_entry }, /*v_Append=*/true);
			}

			bool resolve(const Hades::Runtime::ResolveRequest& ro_Request,
			             const std::string& v_OutMainCppPath,
			             const std::string& v_OutBuildFilePath) override {
				if (!validateFixtureReferences(ro_Request)) {
					return false;
				}
				if (!writeTextFile(v_OutMainCppPath, buildMainCpp(ro_Request))) {
					return false;
				}
				return writeTextFile(v_OutBuildFilePath, buildCMakeLists(ro_Request));
			}

			HADES_NODISCARD uint32_t abiVersion() const noexcept override {
				return 1;
			}
		};

	}

}

extern "C" HADES_CODEGEN_API Hades::Codegen::IHadesCodegen* createHadesCodegen() {
	return new Hades::Codegen::HadesCodegenImpl();
}

extern "C" HADES_CODEGEN_API void destroyHadesCodegen(Hades::Codegen::IHadesCodegen* p_instance) {
	delete p_instance;
}
