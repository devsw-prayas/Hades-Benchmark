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
#include <cstdio>

// Minimal shared local PASS/FAIL framework for Hades-Core-Tests. EXPECT_TRUE
// logs and counts failures but keeps running so one bad assumption doesn't
// hide the rest of a test's report. Not a Hades fixture/benchmark harness -
// see TomlIOTests.cpp's header comment for why TEST_FORMAT.md's
// HadesEngine/ValidationFixture harness doesn't apply here.

namespace HadesTests {

	inline int& failureCount() {
		static int s_counter = 0;
		return s_counter;
	}

	inline void expectTrueImpl(bool v_Condition, const char* v_Expr, const char* v_File, int v_Line) {
		if (!v_Condition) {
			std::printf("  FAIL %s:%d: %s\n", v_File, v_Line, v_Expr);
			++failureCount();
		}
	}

}

#define EXPECT_TRUE(expr) ::HadesTests::expectTrueImpl((expr), #expr, __FILE__, __LINE__)
#define EXPECT_EQ(a, b)   ::HadesTests::expectTrueImpl((a) == (b), #a " == " #b, __FILE__, __LINE__)
