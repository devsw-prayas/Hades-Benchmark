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
#include "ProcessExec.h"

#include <cstdlib>

namespace Hades::Driver {

	std::string quotePath(const std::string& v_Path) {
		if (!v_Path.empty() && v_Path.front() == '"') {
			return v_Path;
		}
		return "\"" + v_Path + "\"";
	}

	int runProcess(const std::string& v_Command) {
		return std::system(v_Command.c_str());
	}

}
