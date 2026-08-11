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
#include <RegistryIO.h>
#include <HadesDiagnostics.h>

#include <cctype>
#include <fstream>
#include <sstream>

namespace Hades::Runtime {

	bool isIdentifier(const std::string& ro_Text) {
		if (ro_Text.empty() || std::isdigit(static_cast<unsigned char>(ro_Text.front()))) {
			return false;
		}
		for (const char l_char : ro_Text) {
			if (!std::isalnum(static_cast<unsigned char>(l_char)) && l_char != '_') {
				return false;
			}
		}
		return true;
	}

	// Catches an empty field, a header path pasted into the wrong key, or a
	// stray character typo before it reaches the generated main.cpp - those
	// currently surface as a wall of MSVC template-instantiation errors
	// pointing at generated code the user never wrote. Cannot catch a name
	// that collides with an unrelated macro/symbol pulled in by some other
	// header - that's only knowable to the real compiler.
	bool isCppTypeName(const std::string& ro_Text) {
		if (ro_Text.empty()) {
			return false;
		}
		size_t l_begin = 0;
		while (true) {
			const size_t l_sep = ro_Text.find("::", l_begin);
			const std::string l_segment = ro_Text.substr(l_begin, l_sep - l_begin);
			if (!isIdentifier(l_segment)) {
				return false;
			}
			if (l_sep == std::string::npos) {
				return true;
			}
			l_begin = l_sep + 2;
		}
	}

	namespace {

		std::string trim(const std::string& ro_Text) {
			size_t l_begin = 0;
			size_t l_finish = ro_Text.size();
			while (l_begin < l_finish && std::isspace(static_cast<unsigned char>(ro_Text[l_begin]))) {
				++l_begin;
			}
			while (l_finish > l_begin && std::isspace(static_cast<unsigned char>(ro_Text[l_finish - 1]))) {
				--l_finish;
			}
			return ro_Text.substr(l_begin, l_finish - l_begin);
		}

		bool isSectionHeader(const std::string& ro_Line, std::string& ro_OutName) {
			if (ro_Line.size() < 2 || ro_Line.front() != '[' || ro_Line.back() != ']') {
				return false;
			}
			ro_OutName = trim(ro_Line.substr(1, ro_Line.size() - 2));
			return true;
		}

		bool splitKeyValue(const std::string& ro_Line, std::string& ro_OutKey, std::string& ro_OutValue) {
			const size_t l_eq = ro_Line.find('=');
			if (l_eq == std::string::npos) {
				return false;
			}
			ro_OutKey = trim(ro_Line.substr(0, l_eq));
			ro_OutValue = trim(ro_Line.substr(l_eq + 1));
			return true;
		}

		// Owns the in-progress section across the line loop, mirroring TomlIO's
		// Parser - readRegistryIni() itself stays a short read-then-drive call.
		class Parser final {
		public:
			Parser(std::vector<FixtureEntry>& ro_OutFixtures, std::vector<RegistryIniError>& ro_OutErrors) noexcept
				: m_outFixtures(ro_OutFixtures)
				, m_outErrors(ro_OutErrors) {
			}

			void handleLine(const std::string& ro_RawLine, uint32_t v_LineNo) {
				std::string l_line = trim(ro_RawLine);
				if (!l_line.empty() && l_line.back() == '\r') {
					l_line.pop_back();
				}
				if (l_line.empty() || l_line.front() == '#' || l_line.front() == ';') {
					return;
				}

				std::string l_sectionName;
				if (isSectionHeader(l_line, l_sectionName)) {
					finishSection(v_LineNo);
					m_current = FixtureEntry{};
					m_current.m_FixtureName = l_sectionName;
					m_inSection = true;
					return;
				}

				handleKeyValue(l_line, v_LineNo);
			}

			void finish(uint32_t v_LineNo) {
				finishSection(v_LineNo);
			}

		private:
			void handleKeyValue(const std::string& ro_Line, uint32_t v_LineNo) {
				std::string l_key, l_value;
				if (!splitKeyValue(ro_Line, l_key, l_value)) {
					m_outErrors.push_back({ v_LineNo, "expected 'key = value'" });
					return;
				}
				if (!m_inSection) {
					m_outErrors.push_back({ v_LineNo, "key '" + l_key + "' outside any [FixtureName] section" });
					return;
				}

				if (l_key == "header") {
					m_current.m_HeaderPath = l_value;
				} else if (l_key == "adapter") {
					m_current.m_AdapterType = l_value;
				} else if (l_key == "chrono") {
					m_current.m_ChronoType = l_value;
				} else if (l_key == "hash") {
					m_current.m_HashType = l_value;
				} else if (l_key == "extra_link_dir") {
					m_current.m_ExtraLinkDir = l_value;
				} else if (l_key == "extra_link_target") {
					m_current.m_ExtraLinkTarget = l_value;
				} else {
					m_outErrors.push_back({ v_LineNo, "[" + m_current.m_FixtureName + "]: unknown key '" + l_key + "'" });
				}
			}

			void finishSection(uint32_t v_LineNo) {
				if (!m_inSection) {
					return;
				}
				m_inSection = false;

				std::string l_missing;
				if (m_current.m_HeaderPath.empty())  l_missing += " header";
				if (m_current.m_AdapterType.empty()) l_missing += " adapter";
				if (m_current.m_ChronoType.empty())  l_missing += " chrono";
				if (m_current.m_HashType.empty())    l_missing += " hash";

				if (!l_missing.empty()) {
					m_outErrors.push_back({ v_LineNo, "[" + m_current.m_FixtureName + "]: missing required key(s):" + l_missing });
					return;
				}

				if (!isIdentifier(m_current.m_FixtureName)) {
					m_outErrors.push_back({ v_LineNo, "[" + m_current.m_FixtureName + "]: section name is not a valid C++ identifier - it becomes the generated fixture class name" });
					return;
				}
				if (!isCppTypeName(m_current.m_AdapterType)) {
					m_outErrors.push_back({ v_LineNo, "[" + m_current.m_FixtureName + "]: adapter '" + m_current.m_AdapterType + "' is not a valid (possibly ::-qualified) C++ type name" });
					return;
				}
				if (!isCppTypeName(m_current.m_ChronoType)) {
					m_outErrors.push_back({ v_LineNo, "[" + m_current.m_FixtureName + "]: chrono '" + m_current.m_ChronoType + "' is not a valid (possibly ::-qualified) C++ type name" });
					return;
				}
				if (!isCppTypeName(m_current.m_HashType)) {
					m_outErrors.push_back({ v_LineNo, "[" + m_current.m_FixtureName + "]: hash '" + m_current.m_HashType + "' is not a valid (possibly ::-qualified) C++ type name" });
					return;
				}
				if (!m_current.m_ExtraLinkTarget.empty() && !isIdentifier(m_current.m_ExtraLinkTarget)) {
					m_outErrors.push_back({ v_LineNo, "[" + m_current.m_FixtureName + "]: extra_link_target '" + m_current.m_ExtraLinkTarget + "' is not a valid CMake target identifier" });
					return;
				}

				m_outFixtures.push_back(m_current);
			}

			std::vector<FixtureEntry>&      m_outFixtures;
			std::vector<RegistryIniError>&  m_outErrors;
			FixtureEntry m_current;
			bool m_inSection = false;
		};

	}

	bool readRegistryIni(const std::string& v_Path, std::vector<FixtureEntry>& ro_OutFixtures,
	                      std::vector<RegistryIniError>& ro_OutErrors) {
		ro_OutFixtures.clear();
		ro_OutErrors.clear();

		std::ifstream l_file(v_Path, std::ios::binary);
		if (!l_file.is_open()) {
			ro_OutErrors.push_back({ 0, "could not open file: " + v_Path });
			return false;
		}

		Parser l_parser(ro_OutFixtures, ro_OutErrors);
		std::string l_line;
		uint32_t l_lineNo = 0;
		while (std::getline(l_file, l_line)) {
			++l_lineNo;
			l_parser.handleLine(l_line, l_lineNo);
		}
		l_parser.finish(l_lineNo);

		return ro_OutErrors.empty();
	}

	bool writeRegistryIni(const std::string& v_Path, const std::vector<FixtureEntry>& ro_Fixtures, bool v_Append) {
		std::ostringstream l_stream;
		for (const FixtureEntry& r_fixture : ro_Fixtures) {
			l_stream << '[' << r_fixture.m_FixtureName << "]\n";
			l_stream << "header = " << r_fixture.m_HeaderPath << '\n';
			l_stream << "adapter = " << r_fixture.m_AdapterType << '\n';
			l_stream << "chrono = " << r_fixture.m_ChronoType << '\n';
			l_stream << "hash = " << r_fixture.m_HashType << '\n';
			if (!r_fixture.m_ExtraLinkDir.empty()) {
				l_stream << "extra_link_dir = " << r_fixture.m_ExtraLinkDir << '\n';
			}
			if (!r_fixture.m_ExtraLinkTarget.empty()) {
				l_stream << "extra_link_target = " << r_fixture.m_ExtraLinkTarget << '\n';
			}
			l_stream << '\n';
		}

		std::ofstream l_file(v_Path, v_Append ? (std::ios::binary | std::ios::app) : (std::ios::binary | std::ios::trunc));
		if (!l_file.is_open()) {
			return false;
		}
		const std::string l_content = l_stream.str();
		l_file.write(l_content.data(), static_cast<std::streamsize>(l_content.size()));
		return static_cast<bool>(l_file);
	}

}
