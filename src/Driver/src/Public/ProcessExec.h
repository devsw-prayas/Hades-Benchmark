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

// Driver's own binary never links HadesEngine/SuiteDriver - it only ever
// shells out: to cmake (configure/build the Codegen-generated suite) and to
// the generated suite binary itself (the CTest-style subprocess model).
// std::system() is the whole implementation - a dev CLI tool doesn't need a
// hand-rolled CreateProcess/fork+exec wrapper.
namespace Hades::Driver {

	// Wraps v_Path in double quotes if it contains a space and isn't already
	// quoted - every path handed to std::system() needs this (repo paths in
	// this project routinely contain spaces, e.g. "Graphics Programming").
	std::string quotePath(const std::string& v_Path);

	// Runs v_Command via std::system() with stdio inherited from this process.
	// Returns the child's exit code (platform-native semantics - the same
	// value std::system() itself returns on this platform).
	int runProcess(const std::string& v_Command);

}
