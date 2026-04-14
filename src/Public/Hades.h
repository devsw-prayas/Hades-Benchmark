/*
* Copyright (c) 2026 StormWeaver
*
* This file is part of Hades Benchmark
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
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#pragma once
#include "HadesCompiler.h"

#if defined(HADES_SHARED)
#if HADES_COMPILER_MSVC
#if defined(HADES_BUILDING_RUNTIME)
#define HADES_RUNTIME_API __declspec(dllexport)
#else
#define HADES_RUNTIME_API __declspec(dllimport)
#endif
#elif HADES_COMPILER_CLANG || HADES_COMPILER_GCC
#define HADES_RUNTIME_API __attribute__((visibility("default")))
#else
#define HADES_RUNTIME_API
#endif

#else
// Static build -> no import/export
#define HADES_RUNTIME_API __declspec(dllexport)
#endif

#include <cstdint>
#include <type_traits>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <cstddef>
#include <memory>
#include <thread>
#include <functional>
#include <cstring>
#include <cmath>
#include <cstdio>

namespace Hades::Runtime {

	/**
	 * @brief Type-blind factory for managing IFixture lifecycle within ThreadRuntime.
	 * Erasure allows HadesEngine to execute arbitrary fixture types.
	 */
	struct FixtureFactory {
		using CreateFn   = void*    (*)(void* p_Adapter);
		using DestroyFn  = void     (*)(void* p_Fixture);
		using StartupFn  = void     (*)(void* p_Fixture);
		using ExecuteFn  = void     (*)(void* p_Fixture);
		using ResetFn    = void     (*)(void* p_Fixture, void* p_Adapter);
		using TeardownFn = void     (*)(void* p_Fixture);
		using HashFn     = uint64_t (*)(void* p_Fixture);

		CreateFn   create   = nullptr;
		DestroyFn  destroy  = nullptr;
		StartupFn  startup  = nullptr;
		ExecuteFn  execute  = nullptr;
		ResetFn    reset    = nullptr;
		TeardownFn teardown = nullptr;
		HashFn     hash     = nullptr;

		HADES_NODISCARD constexpr bool isValid() const noexcept {
			return create && destroy && execute;
		}
	};

	/**
	 * @brief Generates a type-erased Factory for a concrete Fixture/Adapter pair.
	 */
	template<typename F, typename A>
	static HADES_NODISCARD FixtureFactory makeFactory() noexcept {
		FixtureFactory f;
		f.create   = [](void* a) -> void* {
			return new F(*static_cast<A*>(a));
		};
		f.destroy  = [](void* p) {
			delete static_cast<F*>(p);
		};
		f.startup  = [](void* p) {
			static_cast<F*>(p)->startup();
		};
		f.execute  = [](void* p) {
			static_cast<F*>(p)->execute();
		};
		f.reset    = [](void* p, void* a) {
			static_cast<F*>(p)->reset(*static_cast<A*>(a));
		};
		f.teardown = [](void* p) {
			static_cast<F*>(p)->teardown();
		};
		f.hash     = [](void* p) -> uint64_t {
			return static_cast<F*>(p)->getDeterminismHash();
		};
		return f;
	}
}
