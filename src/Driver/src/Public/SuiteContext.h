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
#include <string>

// "Adopting" a suite (init-suite/find-suite) writes a small marker file
// (".hades-suite", one absolute path, in the invocation cwd) that later
// commands (new-test/run/validate) read back as their implicit suite.
namespace Hades::Driver {

	inline constexpr const char* SUITE_MARKER_FILE = ".hades-suite";

	// Writes SUITE_MARKER_FILE in the current working directory, pointing at
	// the absolute, normalized form of v_SuiteDir. Returns false on write failure.
	bool adoptSuite(const std::string& v_SuiteDir);

	// Reads SUITE_MARKER_FILE from cwd and verifies <dir>/suite.toml still
	// exists there. Returns false (with a message printed to stderr) if no
	// suite has been adopted yet, or the adopted directory no longer looks
	// like a valid suite.
	bool loadCurrentSuite(std::string& ro_OutSuiteDir);

}
