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
#include <HadesCompiler.h>

#if defined(_DEBUG) || defined(DEBUG)
#define HADES_BUILD_DEBUG 1
#define HADES_BUILD_RELEASE 0
#else
#define HADES_BUILD_DEBUG 0
#define HADES_BUILD_RELEASE 1
#endif

#if HADES_BUILD_DEBUG
#define HADES_ASSERT(expr)                        \
        do {                                            \
            if (!(expr)) {                             \
                HADES_DEBUG_BREAK();                 \
                HADES_TRAP();                        \
            }                                           \
        } while (0)
#else
#define HADES_ASSERT(expr) do { (void)sizeof(expr); } while (0)
#endif

#if HADES_BUILD_DEBUG
#define HADES_ASSUME(expr) HADES_ASSERT(expr)
#else
#if HADES_COMPILER_MSVC
#define HADES_ASSUME(expr) __assume(expr)
#else
#define HADES_ASSUME(expr) do { if (!(expr)) HADES_UNREACHABLE(); } while (0)
#endif
#endif

#if HADES_BUILD_DEBUG
#define HADES_DEBUG_ASSERT(expr) HADES_ASSERT(expr)
#define HADES_DEBUG_ASSUME(expr) HADES_ASSUME(expr)
#else
#define HADES_DEBUG_ASSERT(expr) do {} while (0)
#define HADES_DEBUG_ASSUME(expr) do {} while (0)
#endif

#define HADES_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
#define HADES_UNUSED(x) (void)(x)
