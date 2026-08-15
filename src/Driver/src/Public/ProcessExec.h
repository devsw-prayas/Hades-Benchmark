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

// Driver only ever shells out (cmake, generated suite binary) - never links HadesEngine/SuiteDriver directly.
namespace Hades::Driver {

	// Quotes v_Path if it has spaces and isn't already quoted - required before handing paths to std::system().
	std::string quotePath(const std::string& v_Path);

	// std::system() wrapper; returns the child's platform-native exit code.
	int runProcess(const std::string& v_Command);

	struct ProcessResult final {
		int  m_ExitCode = 0;
		bool m_TimedOut = false;
	};

	// Redirects child stdout/stderr to v_CaptureFilePath so it doesn't interleave with the isolate progress bar;
	// kills the child past v_TimeoutSeconds (0 = no timeout), leaving m_ExitCode at default since it's meaningless post-TerminateProcess.
	ProcessResult runProcessTimed(const std::string& v_Command, unsigned v_TimeoutSeconds, const std::string& v_CaptureFilePath);

}
