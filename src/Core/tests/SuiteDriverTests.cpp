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
// Unit tests for SuiteDriver (Core/src/Public/SuiteDriver.h) - the
// orchestration class that loops over a suite's queued tests, drives each
// through FixtureRegistry + IFixtureVirtual, and fans results out to
// listeners. Uses a trivial hand-written IFixture<D,A> (no HADES_FIXTURE
// macro exists in code yet - only in the architecture doc's SS1.16
// pseudocode) registered against NullDeviceAdapter/SteadyClockChronoPoint/
// XorHashAccumulator, all of which already exist and are instantiable.

#include "TestFramework.h"

#include <Configuration.h>
#include <Fixture.h>
#include <FixtureRegistry.h>
#include <HadesAdapters.h>
#include <HadesChrono.h>
#include <HadesHash.h>
#include <HadesListeners.h>
#include <SuiteDriver.h>

#include <string>
#include <vector>

using namespace Hades::Runtime;

namespace {

	// executeImpl increments a counter; getDeterminismHashImpl returns a fixed
	// value so every slice hashes identically and the run is deterministic.
	class CounterFixture final : public IFixture<CounterFixture, NullDeviceAdapter> {
	public:
		explicit CounterFixture(NullDeviceAdapter& ro_Adapter) noexcept
			: IFixture(ro_Adapter)
			, m_counter(0) {
		}

		void startupImpl() noexcept {}
		void executeImpl() noexcept { ++m_counter; }
		void resetImpl(NullDeviceAdapter&) noexcept {}
		void teardownImpl() noexcept {}
		uint64_t getDeterminismHashImpl() noexcept { return 42; }

	private:
		uint64_t m_counter;
	};

	Config makeQuickConfig(const std::string& v_Id) {
		Config l_config;
		setId(l_config, v_Id);
		setWarmup(l_config, 0);
		setIterations(l_config, 1);
		setMinSlices(l_config, 1);
		setMaxSlices(l_config, 2);
		setCvThreshold(l_config, 1.0); // high threshold - converges immediately
		return l_config;
	}

	// Records the sequence of listener callbacks so tests can assert on
	// declaration order and the start->complete->suite-complete shape.
	class RecordingListener final : public IHadesListener {
	public:
		std::vector<std::string> m_events;
		std::vector<BenchmarkResult> m_results;

		void onTestStart(const std::string& v_Id) override {
			m_events.push_back("start:" + v_Id);
		}

		void onTestComplete(const std::string& v_Id, const BenchmarkResult& ro_Result) override {
			m_events.push_back("complete:" + v_Id);
			m_results.push_back(ro_Result);
		}

		void onSuiteComplete() override {
			m_events.push_back("suite-complete");
		}
	};

	// FixtureRegistry is non-copyable/non-movable, so this registers into an
	// existing instance rather than constructing-and-returning one by value.
	void registerCounterFixture(FixtureRegistry& ro_Registry) {
		HADES_REGISTER_FIXTURE(ro_Registry, CounterFixture, NullDeviceAdapter, Hades::Runtime::Chrono::SteadyClockChronoPoint, XorHashAccumulator);
	}

	void testRunsRegisteredFixture() {
		std::printf("testRunsRegisteredFixture\n");
		FixtureRegistry l_registry;
		registerCounterFixture(l_registry);

		std::vector<SuiteTestEntry> l_tests;
		SuiteTestEntry l_entry;
		l_entry.m_FixtureName = "CounterFixture";
		l_entry.m_Config = makeQuickConfig("counter_test");
		l_tests.push_back(l_entry);

		SuiteDriver l_driver(l_registry, l_tests);
		RecordingListener l_listener;
		l_driver.run({ &l_listener });

		EXPECT_EQ(l_listener.m_events.size(), 3u);
		if (l_listener.m_events.size() == 3) {
			EXPECT_EQ(l_listener.m_events[0], "start:counter_test");
			EXPECT_EQ(l_listener.m_events[1], "complete:counter_test");
			EXPECT_EQ(l_listener.m_events[2], "suite-complete");
		}

		EXPECT_EQ(l_listener.m_results.size(), 1u);
		if (l_listener.m_results.size() == 1) {
			EXPECT_TRUE(l_listener.m_results[0].deterministic);
			EXPECT_TRUE(!l_listener.m_results[0].runFailed);
			EXPECT_TRUE(l_listener.m_results[0].sliceCount >= 1);
		}
	}

	void testUnknownFixtureProducesRunFailed() {
		std::printf("testUnknownFixtureProducesRunFailed\n");
		FixtureRegistry l_registry;
		registerCounterFixture(l_registry);

		std::vector<SuiteTestEntry> l_tests;
		SuiteTestEntry l_entry;
		l_entry.m_FixtureName = "DoesNotExistFixture";
		l_entry.m_Config = makeQuickConfig("missing_test");
		l_tests.push_back(l_entry);

		SuiteDriver l_driver(l_registry, l_tests);
		RecordingListener l_listener;
		l_driver.run({ &l_listener });

		EXPECT_EQ(l_listener.m_results.size(), 1u);
		if (l_listener.m_results.size() == 1) {
			EXPECT_TRUE(l_listener.m_results[0].runFailed);
		}
	}

	void testDeclarationOrderPreserved() {
		std::printf("testDeclarationOrderPreserved\n");
		FixtureRegistry l_registry;
		registerCounterFixture(l_registry);

		std::vector<SuiteTestEntry> l_tests;
		SuiteTestEntry l_first;
		l_first.m_FixtureName = "CounterFixture";
		l_first.m_Config = makeQuickConfig("first_test");
		SuiteTestEntry l_second;
		l_second.m_FixtureName = "CounterFixture";
		l_second.m_Config = makeQuickConfig("second_test");
		l_tests.push_back(l_first);
		l_tests.push_back(l_second);

		SuiteDriver l_driver(l_registry, l_tests);
		RecordingListener l_listener;
		l_driver.run({ &l_listener });

		EXPECT_EQ(l_listener.m_events.size(), 5u);
		if (l_listener.m_events.size() == 5) {
			EXPECT_EQ(l_listener.m_events[0], "start:first_test");
			EXPECT_EQ(l_listener.m_events[1], "complete:first_test");
			EXPECT_EQ(l_listener.m_events[2], "start:second_test");
			EXPECT_EQ(l_listener.m_events[3], "complete:second_test");
			EXPECT_EQ(l_listener.m_events[4], "suite-complete");
		}
	}

} // namespace

void runSuiteDriverTests() {
	testRunsRegisteredFixture();
	testUnknownFixtureProducesRunFailed();
	testDeclarationOrderPreserved();
}
