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

#include <Hades.h>
#include <HadesCompiler.h>
#include <HadesDiagnostics.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Adapter.h"
#include "Configuration.h"
#include "BenchmarkResult.h"
#include "SequentialRunner.h"

namespace Hades::Runtime {

	// The one deliberate virtual shim in the entire architecture. Orchestration-
	// time dispatch only: SuiteDriver makes exactly one virtual call per test
	// (run()) - everything inside it (SequentialRunner's slice loop) is fully
	// static/CRTP with zero further virtual dispatch.
	class IFixtureVirtual {
	public:
		virtual ~IFixtureVirtual() = default;

		HADES_NODISCARD_MSG("Cannot discard benchmark result")
			virtual BenchmarkResult run(const Config& ro_Config) = 0;

		IFixtureVirtual(const IFixtureVirtual&) = delete;
		IFixtureVirtual& operator=(const IFixtureVirtual&) = delete;
		IFixtureVirtual(IFixtureVirtual&&) = delete;
		IFixtureVirtual& operator=(IFixtureVirtual&&) = delete;

	protected:
		IFixtureVirtual() = default;
	};

	// Owns the concrete fixture, holds a reference to the externally-owned
	// concrete adapter, and forwards run() to SequentialRunner<D,A,C,H> - a
	// fully static call, no virtual dispatch past this one boundary.
	template<typename D, typename A, typename C, typename H>
	class ConcreteFixtureRunner final : public IFixtureVirtual {
		using fixture_ = D;
		using adapter_ = A;

	public:
		explicit ConcreteFixtureRunner(adapter_& ro_Adapter) noexcept
			: m_adapter(ro_Adapter)
			, m_fixture(ro_Adapter) {
		}

		~ConcreteFixtureRunner() override = default;

		BenchmarkResult run(const Config& ro_Config) override {
			return SequentialRunner<D, A, C, H>::run(m_fixture, m_adapter, ro_Config, A::IS_GPU_RUN);
		}

	private:
		adapter_& m_adapter;
		fixture_  m_fixture;
	};

	// Explicit bootstrap registry - never static-init auto-registration
	// (init-order-across-TUs landmine). Fully precompiled: no CRTP constraint,
	// touches fixtures only through the IFixtureVirtual shim.
	class HADES_RUNTIME_API FixtureRegistry final {
	public:
		using FactoryFn = std::function<std::unique_ptr<IFixtureVirtual>(IDeviceAdapterBase&)>;

		FixtureRegistry() = default;
		~FixtureRegistry() = default;

		FixtureRegistry(const FixtureRegistry&) = delete;
		FixtureRegistry& operator=(const FixtureRegistry&) = delete;
		FixtureRegistry(FixtureRegistry&&) = delete;
		FixtureRegistry& operator=(FixtureRegistry&&) = delete;

		void registerFixture(const std::string& v_Name, FactoryFn v_Factory);

		HADES_NODISCARD_MSG("Cannot discard registry lookup result")
			const FactoryFn* find(const std::string& v_Name) const;

	private:
		std::unordered_map<std::string, FactoryFn> m_factories;
	};

	// AdapterType static_cast recovers the concrete adapter type from the
	// type-erased IDeviceAdapterBase& the caller passes at run time.
	// ChronoType/HashType are explicit here because SequentialRunner resolves
	// both at compile time - there is no runtime "backend" switch once a
	// fixture is registered.
	#define HADES_REGISTER_FIXTURE(Registry, Name, AdapterType, ChronoType, HashType) \
		(Registry).registerFixture(#Name, [](Hades::Runtime::IDeviceAdapterBase& ro_Adapter) { \
			return std::make_unique<Hades::Runtime::ConcreteFixtureRunner<Name, AdapterType, ChronoType, HashType>>( \
				static_cast<AdapterType&>(ro_Adapter)); \
		})

} // namespace Hades::Runtime
