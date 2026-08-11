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
#include "SuiteContext.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace Hades::Driver {

	namespace fs = std::filesystem;

	bool adoptSuite(const std::string& v_SuiteDir) {
		std::error_code l_ec;
		const fs::path l_absolute = fs::absolute(fs::path(v_SuiteDir), l_ec);
		if (l_ec) {
			std::cerr << "error: could not resolve suite path '" << v_SuiteDir << "': " << l_ec.message() << "\n";
			return false;
		}

		std::ofstream l_marker(SUITE_MARKER_FILE, std::ios::binary | std::ios::trunc);
		if (!l_marker.is_open()) {
			std::cerr << "error: could not write '" << SUITE_MARKER_FILE << "'\n";
			return false;
		}
		l_marker << l_absolute.generic_string() << "\n";
		return static_cast<bool>(l_marker);
	}

	bool loadCurrentSuite(std::string& ro_OutSuiteDir) {
		std::ifstream l_marker(SUITE_MARKER_FILE, std::ios::binary);
		if (!l_marker.is_open()) {
			std::cerr << "error: no suite adopted in this directory - run 'init-suite' or 'find-suite' first\n";
			return false;
		}

		std::ostringstream l_stream;
		l_stream << l_marker.rdbuf();
		std::string l_dir = l_stream.str();
		while (!l_dir.empty() && (l_dir.back() == '\n' || l_dir.back() == '\r')) {
			l_dir.pop_back();
		}

		if (l_dir.empty() || !fs::exists(fs::path(l_dir) / "suite.toml")) {
			std::cerr << "error: adopted suite '" << l_dir << "' no longer looks valid (missing suite.toml) - re-run 'find-suite'\n";
			return false;
		}

		ro_OutSuiteDir = l_dir;
		return true;
	}

	void clearSuiteIfCurrent(const std::string& v_SuiteDir) {
		std::error_code l_ec;
		const fs::path l_target = fs::absolute(fs::path(v_SuiteDir), l_ec);
		if (l_ec) {
			return;
		}

		std::ifstream l_marker(SUITE_MARKER_FILE, std::ios::binary);
		if (!l_marker.is_open()) {
			return;
		}
		std::ostringstream l_stream;
		l_stream << l_marker.rdbuf();
		l_marker.close();

		std::string l_current = l_stream.str();
		while (!l_current.empty() && (l_current.back() == '\n' || l_current.back() == '\r')) {
			l_current.pop_back();
		}

		if (l_current == l_target.generic_string()) {
			fs::remove(SUITE_MARKER_FILE, l_ec);
		}
	}

}
