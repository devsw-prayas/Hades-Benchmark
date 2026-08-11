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
#include <SuiteDriver.h>

namespace Hades::Runtime {

	SuiteDriver::SuiteDriver(const FixtureRegistry& ro_Registry, std::vector<SuiteTestEntry> v_Tests) noexcept
		: m_registry(ro_Registry)
		, m_tests(std::move(v_Tests)) {
	}

	void SuiteDriver::run(const std::vector<IHadesListener*>& ro_Listeners) const {
		for (const SuiteTestEntry& r_test : m_tests) {
			for (IHadesListener* p_listener : ro_Listeners) {
				p_listener->onTestStart(r_test.m_Config.m_Id);
			}

			const FixtureRegistry::FactoryFn* p_factory = m_registry.find(r_test.m_FixtureName);

			BenchmarkResult result{};
			if (p_factory == nullptr) {
				result.runFailed = true;
			} else {
				const std::unique_ptr<IFixtureVirtual> l_runner = (*p_factory)();
				result = l_runner->run(r_test.m_Config);
			}

			for (IHadesListener* p_listener : ro_Listeners) {
				p_listener->onTestComplete(r_test.m_Config.m_Id, result);
			}
		}

		for (IHadesListener* p_listener : ro_Listeners) {
			p_listener->onSuiteComplete();
		}
	}

}
