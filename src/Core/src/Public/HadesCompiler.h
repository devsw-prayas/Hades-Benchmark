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

// Internal header for compiler detection

#if defined(_MSC_VER)
#define HADES_COMPILER_MSVC 1
#else
#define HADES_COMPILER_MSVC 0
#endif

#if defined(__clang__)
#define HADES_COMPILER_CLANG 1
#else
#define HADES_COMPILER_CLANG 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define HADES_COMPILER_GCC 1
#else
#define HADES_COMPILER_GCC 0
#endif

#if HADES_COMPILER_MSVC
#define HADES_FORCEINLINE __forceinline
#define HADES_NOINLINE    __declspec(noinline)
#elif HADES_COMPILER_CLANG || HADES_COMPILER_GCC
#define HADES_FORCEINLINE inline __attribute__((always_inline))
#define HADES_NOINLINE    __attribute__((noinline))
#else
#define HADES_FORCEINLINE inline
#define HADES_NOINLINE
#endif

#define HADES_INLINE inline

#if HADES_COMPILER_CLANG || HADES_COMPILER_GCC
#define HADES_LIKELY(x)   __builtin_expect(!!(x), 1)
#define HADES_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define HADES_LIKELY(x)   (x)
#define HADES_UNLIKELY(x) (x)
#endif

#if HADES_COMPILER_MSVC
#define HADES_DEBUG_BREAK() __debugbreak()
#define HADES_TRAP()        __debugbreak()
#elif HADES_COMPILER_CLANG || HADES_COMPILER_GCC
#define HADES_DEBUG_BREAK() __builtin_trap()
#define HADES_TRAP()        __builtin_trap()
#else
#include <cstdlib>
#define HADES_DEBUG_BREAK() std::abort()
#define HADES_TRAP()        std::abort()
#endif

#if HADES_COMPILER_MSVC
#define HADES_UNREACHABLE() __assume(0)
#elif HADES_COMPILER_CLANG || HADES_COMPILER_GCC
#define HADES_UNREACHABLE() __builtin_unreachable()
#else
#define HADES_UNREACHABLE() HADES_TRAP()
#endif

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(nodiscard)
#define HADES_NODISCARD [[nodiscard]]
#if __cplusplus >= 202002L
#define HADES_NODISCARD_MSG(msg) [[nodiscard(msg)]]
#else
#define HADES_NODISCARD_MSG(msg) [[nodiscard]]
#endif
#else
#define HADES_NODISCARD
#define HADES_NODISCARD_MSG(msg)
#endif
#else
#define HADES_NODISCARD
#define HADES_NODISCARD_MSG(msg)
#endif

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(maybe_unused)
#define HADES_MAYBE_UNUSED [[maybe_unused]]
#else
#define HADES_MAYBE_UNUSED
#endif
#else
#define HADES_MAYBE_UNUSED
#endif

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(fallthrough)
#define HADES_FALLTHROUGH [[fallthrough]]
#else
#define HADES_FALLTHROUGH
#endif
#else
#define HADES_FALLTHROUGH
#endif

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(noreturn)
#define HADES_NORETURN [[noreturn]]
#else
#define HADES_NORETURN
#endif
#else
#define HADES_NORETURN
#endif

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(deprecated)
#define HADES_DEPRECATED [[deprecated]]
#define HADES_DEPRECATED_MSG(msg) [[deprecated(msg)]]
#else
#define HADES_DEPRECATED
#define HADES_DEPRECATED_MSG(msg)
#endif
#else
#define HADES_DEPRECATED
#define HADES_DEPRECATED_MSG(msg)
#endif

#if HADES_COMPILER_MSVC
#define HADES_RESTRICT __restrict
#elif HADES_COMPILER_CLANG || HADES_COMPILER_GCC
#define HADES_RESTRICT __restrict__
#else
#define HADES_RESTRICT
#endif
