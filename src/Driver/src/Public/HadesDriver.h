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
#include <iostream>
#include <string>
#include <vector>
#include <memory>

// HADES_VERSION is supplied by the top-level CMakeLists.txt as a compile
// definition (--version prints the MAJOR.MINOR.REVISION triplet) - the
// fallback here only matters for stray standalone compiles (e.g. IDE syntax
// checking) outside the real build.
#ifndef HADES_VERSION
#define HADES_VERSION "0.0.0-dev"
#endif
