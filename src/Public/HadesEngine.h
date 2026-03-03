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
#include <HadesCompiler.h>
#include <HadesDiagnositcs.h>

#include "Configuration.h"
#include "BenchmarkResult.h"
#include "Storage.h"
#include "Queue.h"
#include "Barrier.h"

#include "ThreadRuntime.h"
#include "HadesHash.h"
#include "Validator.h"
#include "StatsAccumulator.h"

namespace Hades::Runtime {
	struct RunRequest
	{
		void* p_Fixture = nullptr;   // non-owning, type-erased IFixture<D,A>*
		Config config = {};
	};

#ifndef  HADES_ENGINE_QUEUE_CAPACITY
#define HADES_ENGINE_QUEUE_CAPACITY 64
#endif

	template<typename F, typename A, typename H, typename S>
	class HadesEngine final
	{
		using fixture_ = F;
		using adapter_ = A;
		using accumulator_ = H;
		using backend_ = S;

		using runtime_ = ThreadRuntime<fixture_, adapter_, accumulator_>;
		using queue_ = Queues::SpscQueue<RunRequest, HADES_ENGINE_QUEUE_CAPACITY>;

	public:
		// Constructs the engine and its internal ThreadRuntime.
		// ro_Storage is a reference to the user-provided storage backend -
		// must remain alive for the lifetime of HadesEngine.
		// v_ThreadCount = 0 -> use hardware_concurrency.
		explicit HadesEngine(backend_& ro_Storage, uint32_t v_ThreadCount = 0) noexcept
			: m_storage(ro_Storage)
			, m_runtime(v_ThreadCount)
			, m_shutdown(false)
			, m_totalRuns(0) {
		}

		~HadesEngine() = default;

		HadesEngine(const HadesEngine&) = delete;
		HadesEngine& operator=(const HadesEngine&) = delete;
		HadesEngine(HadesEngine&&) = delete;
		HadesEngine& operator=(HadesEngine&&) = delete;

		// Enqueues a benchmark run request.
		// p_Fixture must remain valid until the engine completes the run.
		// Returns false if the queue is full - caller must retry.
		HADES_NODISCARD_MSG("Cannot discard submit result - queue may be full")
			bool submit(void* p_Fixture, const Config& ro_Config) noexcept {
			HADES_ASSERT(p_Fixture != nullptr);

			RunRequest req;
			req.p_Fixture = p_Fixture;
			req.config = ro_Config;

			return m_queue.push(req);
		}

		// Drains the SPSC queue continuously, executing one run at a time.
		// Blocks between runs waiting for the next queue item.
		// Returns only when shutdown() has been called.
		void run() noexcept {
			while (HADES_LIKELY(!m_shutdown.load(std::memory_order_acquire))) {
				RunRequest req;

				if (!m_queue.pop(req)) {
					// Queue empty - spin-yield until next item or shutdown
					internalYield();
					continue;
				}

				internalExecuteRun(req);
			}

			// Drain any remaining queued items after shutdown signal
			internalDrainOnShutdown();
		}

		// Signals the engine to stop after completing any in-progress run.
		// run() will return after processing all remaining queued items.
		void shutdown() noexcept {
			m_shutdown.store(true, std::memory_order_release);
		}

		HADES_NODISCARD_MSG("Cannot discard total run count")
			uint64_t totalRuns() const noexcept {
			return m_totalRuns;
		}

		HADES_NODISCARD_MSG("Cannot discard queue empty state")
			bool queueEmpty() const noexcept {
			return m_queue.empty();
		}

		HADES_NODISCARD_MSG("Cannot discard shutdown state")
			bool isShutdown() const noexcept {
			return m_shutdown.load(std::memory_order_acquire);
		}

	private:

		void internalExecuteRun(const RunRequest& ro_Req) noexcept {
			HADES_ASSERT(ro_Req.p_Fixture != nullptr);

			const uint32_t chunkCount = internalResolveChunkCount(ro_Req.config);

			if (chunkCount <= 1)
				m_runtime.submit(ro_Req.config);
			else
				m_runtime.submitParallel(ro_Req.config, chunkCount);

			// Collect result and forward to storage backend
			const BenchmarkResult& r_Result = m_runtime.result();
			m_storage.store(r_Result);

			++m_totalRuns;
		}

		// Resolves how many parallel chunks to use for this run.
		// If m_ThreadCount == 1 or submitParallel not implied by config -> 1 chunk.
		// Otherwise -> threadCount chunks (one per thread).
		static uint32_t internalResolveChunkCount(const Config& ro_Config) noexcept {
			const uint32_t threadCount = ro_Config.m_ThreadCount;
			return (threadCount <= 1) ? 1u : threadCount;
		}

		// Spin-yields to avoid burning a full core while waiting for queue items.
		static void internalYield() noexcept {
#if HADES_COMPILER_MSVC
			_mm_pause();
#elif HADES_COMPILER_CLANG || HADES_COMPILER_GCC
			__builtin_ia32_pause();
#endif
		}

		// After shutdown is signaled, process any remaining queued items
		// before returning from run().
		void internalDrainOnShutdown() noexcept {
			RunRequest req;
			while (m_queue.pop(req))
				internalExecuteRun(req);
		}

		backend_& m_storage;     // non-owning reference - user manages lifetime
		runtime_           m_runtime;     // owns thread pool - lives for engine lifetime
		queue_             m_queue;       // SPSC queue - user pushes, engine drains
		std::atomic<bool>  m_shutdown;    // set by shutdown(), observed by run()
		uint64_t           m_totalRuns;   // total completed runs since construction
	};
}