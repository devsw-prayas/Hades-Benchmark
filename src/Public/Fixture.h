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
#include <Hades.h>

#include "HadesCompiler.h"

namespace Hades::Runtime {
	template<typename D, typename A>
	class IFixture
	{
		using derived_ = D;
		using adapter_ = A;

		HADES_NODISCARD constexpr derived_& self() noexcept {
			return static_cast<derived_&>(*this);
		}

		HADES_NODISCARD constexpr const derived_& self() const noexcept {
			return static_cast<const derived_&>(*this);
		}

	protected:
		adapter_& m_Adapter;   // C2: private member, m_ prefix, PascalCase after prefix

	public:
		explicit IFixture(adapter_& ro_Adapter) noexcept
			: m_Adapter(ro_Adapter) {
		}
		// Called once at run start. PCIe host -> device transfers must occur here
		// for GPU fixtures. Transfer time is captured in BenchmarkResult.
		void startup() noexcept {
			self().startupImpl();
		}

		// Called every iteration. Fixture owns the kernel launch for GPU fixtures.
		void execute() noexcept {
			self().executeImpl();
		}

		// Called between iterations within a slice. adapter_ passed for GPU reset
		// synchronization. Followed by adapter.synchronize() in the engine.
		void reset(adapter_& ro_Adapter) noexcept {
			self().resetImpl(ro_Adapter);
		}

		// Called once at run end.
		void teardown() noexcept {
			self().teardownImpl();
		}

		// Returns a semantically meaningful, deterministic hash of the fixture's
		// output state. Called by thread 0 after solidify(), after BARRIER 1.
		// Fixture defines what constitutes correctness.
		HADES_NODISCARD_MSG("Cannot discard determinism hash") uint64_t getDeterminismHash() noexcept {
			return self().getDeterminismHashImpl();
		}

		IFixture(const IFixture&) = delete;
		IFixture& operator=(const IFixture&) = delete;
		IFixture(IFixture&&) = delete;
		IFixture& operator=(IFixture&&) = delete;

	protected:
		~IFixture() = default;
	};
}