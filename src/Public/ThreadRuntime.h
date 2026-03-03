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

#include "Barrier.h"
#include "Queue.h"
#include "Configuration.h"
#include "Fixture.h"
#include "Adapter.h"
#include "BenchmarkResult.h"
#include "HadesHash.h"
#include "Validator.h"
#include "StatsAccumulator.h"

namespace Hades::Runtime {
    static constexpr uint32_t DEQUE_CAPACITY = 256;

    struct ThreadJob
    {
        void* p_Fixture;           // non-owning, type-erased IFixture<D,A>* - cast by worker
        uint64_t iterationsPerChunk;  // number of execute() calls this job performs
        uint32_t chunkIndex;          // which chunk this job represents (0-based)
    };

    template<typename F, typename A, typename H>
    class ThreadRuntime final
    {
        using fixture_ = F;
        using adapter_ = A;
        using accumulator_ = H;

    public:
        // Constructs the thread pool. Threads are spawned immediately and idle
        // on the barrier waiting for work.
        // v_ThreadCount = 0 -> use hardware_concurrency.
        explicit ThreadRuntime(uint32_t v_ThreadCount = 0) noexcept
            : m_threadCount(internalResolveThreadCount(v_ThreadCount))
            , m_shutdown(false)
            , m_jobsReady(false)
            , m_sliceComplete(false) {
            HADES_ASSERT(m_threadCount > 0);
            HADES_ASSERT(m_threadCount <= MAX_THREADS);

            internalSpawnThreads();
        }

        ~ThreadRuntime() noexcept {
            internalShutdown();
        }

        ThreadRuntime(const ThreadRuntime&) = delete;
        ThreadRuntime& operator=(const ThreadRuntime&) = delete;
        ThreadRuntime(ThreadRuntime&&) = delete;
        ThreadRuntime& operator=(ThreadRuntime&&) = delete;

        // Executes a full benchmark run for a single-threaded submit.
        // One thread steals the job; all others remain idle on the barrier.
        void submit(const Config& ro_Config) noexcept {
            internalRunLifecycle(ro_Config, 1);
        }

        // Executes a full benchmark run with parallel fan-out.
        // v_ChunkCount chunks pushed; barrier size = min(v_ChunkCount, m_threadCount).
        void submitParallel(const Config& ro_Config, uint32_t v_ChunkCount) noexcept {
            HADES_ASSERT(v_ChunkCount > 0);
            const uint32_t barrierSize = (v_ChunkCount < m_threadCount)
                ? v_ChunkCount : m_threadCount;
            internalRunLifecycle(ro_Config, barrierSize);
        }


        HADES_NODISCARD_MSG("Cannot discard benchmark result")
            const BenchmarkResult& result() const noexcept {
            return m_result;
        }

        HADES_NODISCARD_MSG("Cannot discard thread count")
            uint32_t threadCount() const noexcept {
            return m_threadCount;
        }

    private:

        static uint32_t internalResolveThreadCount(uint32_t v_Count) noexcept {
            if (v_Count == 0) {
                const uint32_t hw = static_cast<uint32_t>(std::thread::hardware_concurrency());
                return (hw > 0) ? hw : 1u;
            }
            return v_Count;
        }

        void internalSpawnThreads() noexcept {
            // Dispatch barrier: all threads wait here between runs
            m_dispatchBarrier.reset(m_threadCount);

            for (uint32_t i = 0; i < m_threadCount; ++i) {
                m_threads[i] = std::thread([this, i]() noexcept {
                    internalWorkerLoop(i);
                });
            }
        }

        void internalShutdown() noexcept {
            m_shutdown.store(true, std::memory_order_release);

            // Wake all threads through the dispatch barrier so they can observe shutdown
            m_dispatchBarrier.reset(m_threadCount);
            for (uint32_t i = 0; i < m_threadCount; ++i)
                m_dispatchBarrier.arrive();

            for (uint32_t i = 0; i < m_threadCount; ++i) {
                if (m_threads[i].joinable())
                    m_threads[i].join();
            }
        }


        void internalRunLifecycle(const Config& ro_Config, uint32_t v_BarrierSize) noexcept {
            m_activeBarrierSize = v_BarrierSize;
            m_config = ro_Config;

            // Signal threads to start this run
            m_jobsReady.store(true, std::memory_order_release);
            m_dispatchBarrier.reset(m_threadCount);

            // Owner (caller) thread acts as coordinator - does not participate in stealing
            // HadesEngine drives the outer loop; ThreadRuntime drives the inner slice loop
            internalCoordinatorLoop(ro_Config, v_BarrierSize);

            m_jobsReady.store(false, std::memory_order_release);
        }

        void internalCoordinatorLoop(const Config& ro_Config, uint32_t v_BarrierSize) noexcept {
            // Build StatsAccumulator and DeterminismValidator for this run
            accumulator_                       hashAcc;
            DeterminismValidator<accumulator_> validator(hashAcc);
            StatsAccumulator<adapter_>         stats(false /* isGpuRun: resolved per adapter */);

            const uint64_t iterationsPerChunk = (ro_Config.m_Iterations > 0)
                ? ro_Config.m_Iterations : 1000u;

            // Calibration slice
            internalDispatchSlice(v_BarrierSize, iterationsPerChunk);
            internalWaitSliceComplete();

            StoppingKnobs configKnobs;
            configKnobs.cvThreshold = ro_Config.m_CvThreshold;
            configKnobs.minSlices = ro_Config.m_MinSlices;
            configKnobs.maxSlices = ro_Config.m_MaxSlices;
            configKnobs.consecutiveDiscardCap = ro_Config.m_ConsecutiveDiscardCap;
            configKnobs.totalDiscardCap = ro_Config.m_TotalDiscardCap;

            stats.deriveKnobs(m_lastSliceWallTime, configKnobs);

            // Real measurement slices
            while (true) {
                internalDispatchSlice(v_BarrierSize, iterationsPerChunk);
                internalWaitSliceComplete();

                // Thread 0 responsibilities after BARRIER 1 (done inside worker loop):
                // solidify, feedThreadHash, validateSlice, feedSlice
                // Results are written into m_sliceTimings / m_perThreadTimes by workers

                stats.feedThreadTimes(m_perThreadTimes, m_threadCount);

                SliceTimings timings;
                timings.cpuWallTime = m_lastSliceWallTime;
                timings.deviceKernelTime = m_lastSliceKernelTime;
                timings.driverOverhead = m_lastSliceDriverOverhead;

                stats.feedSlice(timings);

                // Feed hashes into validator (collected by thread 0 in worker loop)
                for (uint32_t t = 0; t < m_threadCount; ++t)
                    validator.feedThreadHash(m_perThreadHashes[t]);
                validator.validateSlice();

                if (stats.shouldStop())
                    break;
            }

            // Assemble final result
            stats.assembleResult(m_result, iterationsPerChunk);

            m_result.deterministic = validator.isDeterministic();
            m_result.referenceHash = validator.referenceHash();
            m_result.uniqueHashCount = validator.uniqueHashCount();
            m_result.firstDivergenceSlice = validator.firstDivergenceSlice();
            m_result.threadCount = m_threadCount;
            m_result.iterationsPerSlice = iterationsPerChunk;
            m_result.warmupCount = ro_Config.m_WarmupCount;

            const StoppingKnobs& k = stats.knobs();
            m_result.minSlices = k.minSlices;
            m_result.maxSlices = k.maxSlices;
            m_result.cvThreshold = k.cvThreshold;
        }

        void internalDispatchSlice(uint32_t v_BarrierSize, uint64_t v_IterationsPerChunk) noexcept {
            // Push one job per participating thread into the Chase-Lev deque
            for (uint32_t i = 0; i < v_BarrierSize; ++i) {
                ThreadJob job;
                job.p_Fixture = m_fixtureSlots[i];   // per-thread fixture pointer
                job.chunkIndex = i;
                job.iterationsPerChunk = v_IterationsPerChunk;

                const bool pushed = m_deque.push(job);
                HADES_ASSERT(pushed);
                HADES_UNUSED(pushed);
            }

            m_sliceBarrier.reset(v_BarrierSize);
            m_sliceComplete.store(false, std::memory_order_release);
        }

        void internalWaitSliceComplete() noexcept {
            // Spin until thread 0 signals slice complete after BARRIER 2
            while (!m_sliceComplete.load(std::memory_order_acquire)) {
#if HADES_COMPILER_MSVC
                _mm_pause();
#elif HADES_COMPILER_CLANG || HADES_COMPILER_GCC
                __builtin_ia32_pause();
#endif
            }
        }

        void internalWorkerLoop(uint32_t v_ThreadIndex) noexcept {
            // Each thread constructs its own adapter and fixture at launch
            adapter_  adapter;
            fixture_  fixture(adapter);

            // Register this thread's fixture pointer for the coordinator
            m_fixtureSlots[v_ThreadIndex] = static_cast<void*>(&fixture);
            m_adapterSlots[v_ThreadIndex] = static_cast<void*>(&adapter);

            // Wait until all threads have registered their slots
            m_setupBarrier.reset(m_threadCount);
            m_setupBarrier.arrive();

            while (true) {
                // Wait for dispatch signal from coordinator
                m_dispatchBarrier.arrive();

                if (HADES_UNLIKELY(m_shutdown.load(std::memory_order_acquire)))
                    return;

                // --- startup phase ---
                fixture.startup();
                std::atomic_thread_fence(std::memory_order_seq_cst);

                // --- warmup phase ---
                const uint32_t warmupCount = m_config.m_WarmupCount;
                for (uint32_t w = 0; w < warmupCount; ++w) {
                    fixture.execute();
                    adapter.synchronize();
                }

                // --- slice loop ---
                internalSliceLoop(v_ThreadIndex, fixture, adapter);

                // --- teardown phase ---
                std::atomic_thread_fence(std::memory_order_seq_cst);
                fixture.teardown();
            }
        }

        void internalSliceLoop(
            uint32_t v_ThreadIndex,
            fixture_& ro_Fixture,
            adapter_& ro_Adapter) noexcept {
            while (true) {
                // Try to steal a job
                ThreadJob job;
                bool      hasJob = false;

                while (!hasJob) {
                    using StealResult = Queues::ChaseLevDeque<ThreadJob, DEQUE_CAPACITY>::StealResult;
                    const StealResult result = m_deque.steal(job);

                    if (result == StealResult::Success) {
                        hasJob = true;
                    } else if (result == StealResult::Empty) {
                        // No job for this thread this slice - go to barrier directly
                        break;
                    }
                    // StealResult::Abort -> retry
                }

                if (!hasJob) {
                    // Thread not participating in this slice - still must arrive at barriers
                    // if it is within the active barrier size
                    return;
                }

                // --- Execute M iterations ---
                const uint64_t iters = job.iterationsPerChunk;

                // Record start
                m_perThreadStartTimes[v_ThreadIndex] =
                    static_cast<double>(internalCurrentTimeNs()) * 1e-9;

                ro_Adapter.recordEvent(m_startEvents[v_ThreadIndex]);

                for (uint64_t i = 0; i < iters; ++i) {
                    ro_Fixture.execute();
                }

                ro_Adapter.recordEvent(m_endEvents[v_ThreadIndex]);
                ro_Adapter.synchronize();

                const double endTime =
                    static_cast<double>(internalCurrentTimeNs()) * 1e-9;

                m_perThreadTimes[v_ThreadIndex] = endTime - m_perThreadStartTimes[v_ThreadIndex];
                m_perThreadKernelTimes[v_ThreadIndex] = static_cast<double>(
                    ro_Adapter.elapsedTime(m_startEvents[v_ThreadIndex], m_endEvents[v_ThreadIndex])) * 1e-3;
                m_perThreadDriverOverhead[v_ThreadIndex] =
                    m_perThreadTimes[v_ThreadIndex] - m_perThreadKernelTimes[v_ThreadIndex];

                // reset between iterations
                ro_Fixture.reset(ro_Adapter);
                ro_Adapter.synchronize();

                // --- BARRIER 1 - iteration completion ---
                m_sliceBarrier.arrive();

                // Thread 0 responsibilities after BARRIER 1
                if (v_ThreadIndex == 0) {
                    // Solidify: pull fixture output to host
                    ro_Adapter.synchronize();

                    // Collect per-thread hashes
                    for (uint32_t t = 0; t < m_threadCount; ++t) {
                        auto* p_Fixture = static_cast<fixture_*>(m_fixtureSlots[t]);
                        m_perThreadHashes[t] = p_Fixture->getDeterminismHash();
                    }

                    // Aggregate slice timings
                    double maxWall = 0.0;
                    double maxKernel = 0.0;
                    double maxDriver = 0.0;

                    for (uint32_t t = 0; t < m_threadCount; ++t) {
                        if (m_perThreadTimes[t] > maxWall)   maxWall = m_perThreadTimes[t];
                        if (m_perThreadKernelTimes[t] > maxKernel) maxKernel = m_perThreadKernelTimes[t];
                        if (m_perThreadDriverOverhead[t] > maxDriver) maxDriver = m_perThreadDriverOverhead[t];
                    }

                    m_lastSliceWallTime = maxWall;
                    m_lastSliceKernelTime = maxKernel;
                    m_lastSliceDriverOverhead = maxDriver;
                }

                // --- BARRIER 2 - post-validation ---
                m_sliceBarrier.arrive();

                if (v_ThreadIndex == 0)
                    m_sliceComplete.store(true, std::memory_order_release);

                // Check if coordinator wants to stop
                if (m_shutdown.load(std::memory_order_acquire))
                    return;
            }
        }

        // Portable monotonic timestamp in nanoseconds
        static uint64_t internalCurrentTimeNs() noexcept {
            return static_cast<uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count());
        }

        uint32_t           m_threadCount;
        uint32_t           m_activeBarrierSize = 0;
        Config             m_config = {};
        BenchmarkResult    m_result = {};

        std::atomic<bool>  m_shutdown;
        std::atomic<bool>  m_jobsReady;
        std::atomic<bool>  m_sliceComplete;

        // Barriers
        CyclicBarrier m_dispatchBarrier;   // used between runs - all threads idle here
        CyclicBarrier m_setupBarrier;      // used once at startup for slot registration
        CyclicBarrier m_sliceBarrier;      // used per slice (two arrives per slice)

        // Chase-Lev deque - jobs pushed by coordinator, stolen by workers
        Queues::ChaseLevDeque<ThreadJob, DEQUE_CAPACITY> m_deque;

        // Per-thread slots - registered at thread launch, read by coordinator
        void* m_fixtureSlots[MAX_THREADS] = {};
        void* m_adapterSlots[MAX_THREADS] = {};

        // Per-thread timing scratch
        double m_perThreadStartTimes[MAX_THREADS] = {};
        double m_perThreadTimes[MAX_THREADS] = {};
        double m_perThreadKernelTimes[MAX_THREADS] = {};
        double m_perThreadDriverOverhead[MAX_THREADS] = {};
        uint64_t m_perThreadHashes[MAX_THREADS] = {};

        // Aggregated slice results written by thread 0 after BARRIER 1
        double m_lastSliceWallTime = 0.0;
        double m_lastSliceKernelTime = 0.0;
        double m_lastSliceDriverOverhead = 0.0;

        // Dummy event placeholders - concrete adapter types provide real events
        // These are placeholder typed as int; actual event type is adapter_-defined
        // and accessed via recordEvent<E>() template. Adapter owns real events.
        int m_startEvents[MAX_THREADS] = {};
        int m_endEvents[MAX_THREADS] = {};

        // Thread pool
        std::thread m_threads[MAX_THREADS];
    };

} // namespace Hades::Runtime