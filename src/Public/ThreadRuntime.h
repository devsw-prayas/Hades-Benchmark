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

#include "Barrier.h"
#include "Configuration.h"
#include "Fixture.h"
#include "Adapter.h"
#include "BenchmarkResult.h"
#include "HadesHash.h"
#include "Validator.h"
#include "HadesChrono.h"
#include "StatsAccumulator.h"

namespace Hades::Runtime {

    /**
     * @brief Internal tracking struct for a single work chunk within a slice.
     */
    struct ThreadJob {
        void*    p_Fixture;
        uint32_t chunkIndex;
        uint64_t iterationsPerChunk;
    };

    template<typename A, typename H>
    class ThreadRuntime final
    {
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
            , m_sliceComplete(false)
            , m_runStop(false)
            , m_sliceEpoch(0)
            , m_iterationsPerChunk(0) {
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
        void submit(const Config& ro_Config, const FixtureFactory& ro_Factory) noexcept {
            internalRunLifecycle(ro_Config, ro_Factory, 1);
        }

        // Executes a full benchmark run with parallel fan-out.
        // v_ChunkCount chunks pushed; barrier size = min(v_ChunkCount, m_threadCount).
        void submitParallel(const Config& ro_Config, const FixtureFactory& ro_Factory, uint32_t v_ChunkCount) noexcept {
            HADES_ASSERT(v_ChunkCount > 0);
            const uint32_t barrierSize = (v_ChunkCount < m_threadCount)
                ? v_ChunkCount : m_threadCount;
            internalRunLifecycle(ro_Config, ro_Factory, barrierSize);
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
            // Both barriers reset once here, before any worker can call arrive()
            m_dispatchBarrier.reset(m_threadCount + 1);
            m_setupBarrier.reset(m_threadCount);
            m_runCompletionBarrier.reset(m_threadCount + 1);
            for (uint32_t i = 0; i < m_threadCount; ++i) {
                m_threads[i] = std::thread([this, i]() noexcept {
                    internalWorkerLoop(i);
                });
            }
        }

        void internalShutdown() noexcept {
            m_shutdown.store(true, std::memory_order_release);
            m_runStop.store(true, std::memory_order_release);
            m_sliceEpoch.fetch_add(1, std::memory_order_release);

            // Coordinator arrives as its slot to release workers waiting at the barrier
            m_dispatchBarrier.arrive();

            for (uint32_t i = 0; i < m_threadCount; ++i) {
                if (m_threads[i].joinable())
                    m_threads[i].join();
            }
        }


        void internalRunLifecycle(const Config& ro_Config, const FixtureFactory& ro_Factory, uint32_t v_BarrierSize) noexcept {
            m_activeBarrierSize = v_BarrierSize;
            m_config = ro_Config;
            m_currentFactory = ro_Factory;
            m_runStop.store(false, std::memory_order_release);
            m_iterationsPerChunk.store(0, std::memory_order_release);

            // Coordinator arrives as its slot — barrier is cyclic, no reset needed.
            // Epoch advances only when all workers have also arrived, guaranteeing
            // setup is visible before any worker proceeds.
            m_dispatchBarrier.arrive();

            internalCoordinatorLoop(ro_Config, v_BarrierSize);
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

                stats.feedThreadTimes(m_perThreadTimes, m_activeBarrierSize);

                SliceTimings timings;
                timings.cpuWallTime = m_lastSliceWallTime;
                timings.deviceKernelTime = m_lastSliceKernelTime;
                timings.driverOverhead = m_lastSliceDriverOverhead;

                stats.feedSlice(timings);

                // Feed hashes into validator (collected by chunk 0 owner in worker loop)
                for (uint32_t t = 0; t < m_activeBarrierSize; ++t)
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

            m_runStop.store(true, std::memory_order_release);
            m_sliceEpoch.fetch_add(1, std::memory_order_release);
            m_runCompletionBarrier.arrive();
        }

        void internalDispatchSlice(uint32_t v_BarrierSize, uint64_t v_IterationsPerChunk) noexcept {
            m_sliceBarrier.reset(v_BarrierSize);
            m_iterationsPerChunk.store(v_IterationsPerChunk, std::memory_order_release);
            m_sliceComplete.store(false, std::memory_order_release);
            m_sliceEpoch.fetch_add(1, std::memory_order_release);
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
            // Each thread constructs its own adapter at launch
            adapter_  adapter;
            m_adapterSlots[v_ThreadIndex] = static_cast<void*>(&adapter);

            // Wait until all threads have registered their slots
            m_setupBarrier.arrive();

            while (true) {
                // Wait for coordinator to arrive (signals setup complete for this run)
                m_dispatchBarrier.arrive();

                if (HADES_UNLIKELY(m_shutdown.load(std::memory_order_acquire)))
                    return;


                // Create fixture for this run
                void* p_Fixture = m_currentFactory.create(m_adapterSlots[v_ThreadIndex]);
                m_fixtureSlots[v_ThreadIndex] = p_Fixture;

                // --- startup phase ---
                m_currentFactory.startup(p_Fixture);
                std::atomic_thread_fence(std::memory_order_seq_cst);

                // --- warmup phase ---
                const uint32_t warmupCount = m_config.m_WarmupCount;
                for (uint32_t w = 0; w < warmupCount; ++w) {
                    m_currentFactory.execute(p_Fixture);
                    adapter.synchronize();
                }

                // --- slice loop ---
                internalSliceLoop(v_ThreadIndex, p_Fixture, adapter);

                // --- teardown phase ---
                std::atomic_thread_fence(std::memory_order_seq_cst);
                m_currentFactory.teardown(p_Fixture);

                // Cleanup fixture
                m_currentFactory.destroy(p_Fixture);
                m_fixtureSlots[v_ThreadIndex] = nullptr;
                m_runCompletionBarrier.arrive();
            }
        }

        void internalSliceLoop(
            uint32_t v_ThreadIndex,
            void* p_Fixture,
            adapter_& ro_Adapter) noexcept {
            uint64_t observedSliceEpoch = m_sliceEpoch.load(std::memory_order_acquire);
            if (m_iterationsPerChunk.load(std::memory_order_acquire) > 0 && observedSliceEpoch > 0)
                --observedSliceEpoch;

            while (true) {
                while (true) {
                    if (m_shutdown.load(std::memory_order_acquire) || m_runStop.load(std::memory_order_acquire))
                        return;

                    const uint64_t sliceEpoch = m_sliceEpoch.load(std::memory_order_acquire);
                    if (sliceEpoch != observedSliceEpoch) {
                        observedSliceEpoch = sliceEpoch;
                        break;
                    }

#if HADES_COMPILER_MSVC
                    _mm_pause();
#elif HADES_COMPILER_CLANG || HADES_COMPILER_GCC
                    __builtin_ia32_pause();
#endif
                }

                if (v_ThreadIndex >= m_activeBarrierSize)
                    continue;

                ThreadJob job;
                job.p_Fixture = p_Fixture;
                job.chunkIndex = v_ThreadIndex;
                job.iterationsPerChunk = m_iterationsPerChunk.load(std::memory_order_acquire);

                HADES_ASSERT(job.iterationsPerChunk > 0);

                // --- Execute M iterations ---
                const uint64_t iters = job.iterationsPerChunk;

                // Record start
                Chrono::SteadyTimestamp steadyStart;
                Chrono::RdtscTimestamp rdtscStart;

                if (m_config.m_ChronoBackend == ChronoBackend::Rdtsc) {
                    rdtscStart = m_clockRdtsc.now();
                } else {
                    steadyStart = m_clockSteady.now();
                }

                ro_Adapter.recordEvent(m_startEvents[v_ThreadIndex]);

                for (uint64_t i = 0; i < iters; ++i) {
                    m_currentFactory.execute(p_Fixture);
                }

                ro_Adapter.recordEvent(m_endEvents[v_ThreadIndex]);
                ro_Adapter.synchronize();

                if (m_config.m_ChronoBackend == ChronoBackend::Rdtsc) {
                    const auto rdtscEnd = m_clockRdtsc.now();
                    m_perThreadTimes[v_ThreadIndex] = static_cast<double>(m_clockRdtsc.delta(rdtscStart, rdtscEnd)) * 1e-9;
                } else {
                    const auto steadyEnd = m_clockSteady.now();
                    m_perThreadTimes[v_ThreadIndex] = static_cast<double>(m_clockSteady.delta(steadyStart, steadyEnd)) * 1e-9;
                }

                m_perThreadKernelTimes[v_ThreadIndex] = static_cast<double>(
                    ro_Adapter.elapsedTime(m_startEvents[v_ThreadIndex], m_endEvents[v_ThreadIndex])) * 1e-3;
                m_perThreadDriverOverhead[v_ThreadIndex] =
                    m_perThreadTimes[v_ThreadIndex] - m_perThreadKernelTimes[v_ThreadIndex];

                // reset between iterations
                m_currentFactory.reset(p_Fixture, &ro_Adapter);
                ro_Adapter.synchronize();

                // --- BARRIER 1 - iteration completion ---
                m_sliceBarrier.arrive();

                // Chunk 0 responsibilities after BARRIER 1
                if (job.chunkIndex == 0) {
                    // Solidify: pull fixture output to host
                    ro_Adapter.synchronize();

                    // Collect per-thread hashes
                    for (uint32_t t = 0; t < m_activeBarrierSize; ++t) {
                        m_perThreadHashes[t] = m_currentFactory.hash(m_fixtureSlots[t]);
                    }

                    // Aggregate slice timings
                    double maxWall = 0.0;
                    double maxKernel = 0.0;
                    double maxDriver = 0.0;

                    for (uint32_t t = 0; t < m_activeBarrierSize; ++t) {
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

                if (job.chunkIndex == 0)
                    m_sliceComplete.store(true, std::memory_order_release);

                // Check if coordinator wants to stop
                if (m_shutdown.load(std::memory_order_acquire))
                    return;
            }
        }


        uint32_t           m_threadCount;
        uint32_t           m_activeBarrierSize = 0;
        Config             m_config = {};
        BenchmarkResult    m_result = {};

        std::atomic<bool>  m_shutdown;
        std::atomic<bool>  m_jobsReady;
        std::atomic<bool>  m_sliceComplete;
        std::atomic<bool>  m_runStop;
        std::atomic<uint64_t> m_sliceEpoch;
        std::atomic<uint64_t> m_iterationsPerChunk;

        // Barriers
        CyclicBarrier m_dispatchBarrier;   // used between runs - all threads idle here
        CyclicBarrier m_setupBarrier;      // used once at startup for slot registration
        CyclicBarrier m_sliceBarrier;      // used per slice (two arrives per slice)
        CyclicBarrier m_runCompletionBarrier; // used once per run to prevent next-run overlap

        // Current run context
        FixtureFactory m_currentFactory = {};

        // Per-thread slots - registered at thread launch, read by coordinator
        void* m_fixtureSlots[MAX_THREADS] = {};
        void* m_adapterSlots[MAX_THREADS] = {};

        // Per-thread timing scratch
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
        typename adapter_::event_type m_startEvents[MAX_THREADS] = {};
        typename adapter_::event_type m_endEvents[MAX_THREADS] = {};

        // Chrono backends
        Chrono::SteadyClockChronoPoint m_clockSteady;
        Chrono::RdtscChronoPoint       m_clockRdtsc;

        // Thread pool
        std::thread m_threads[MAX_THREADS];
    };

} // namespace Hades::Runtime
