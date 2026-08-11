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
#include <vector>

// Driver subcommand implementations. Each returns a process exit code:
// 0 = success, 1 = a step failed (build/parse/write error), 2 = bad usage
// (missing args, unknown --build backend). Driver's own binary links only
// Core's config-reading functions + Codegen's writer interface, never
// SuiteDriver/HadesEngine directly - these functions honor that by
// construction, since they never touch anything from SuiteDriver.h.
namespace Hades::Driver {

	int cmdInitSuite(const std::vector<std::string>& v_Args);
	int cmdFindSuite(const std::vector<std::string>& v_Args);
	int cmdRemoveSuite(const std::vector<std::string>& v_Args);
	int cmdNewTest(const std::vector<std::string>& v_Args);
	int cmdRun(const std::vector<std::string>& v_Args);
	int cmdValidate(const std::vector<std::string>& v_Args);

}
