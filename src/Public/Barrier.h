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
#include <HadesDiagnostics.h>

namespace Hades::Runtime {

#ifndef HADES_BARRIER_SPIN_COUNT
#define HADES_BARRIER_SPIN_COUNT 1024
#endif

    class HADES_RUNTIME_API CyclicBarrier final
    {
    public:
        // Constructs barrier with an initial size of 0.
        // Call reset() before first use to set the participant count.
        CyclicBarrier() noexcept
            : m_size(0)
            , m_arrived(0)
            , m_epoch(0) {
        }

        ~CyclicBarrier() = default;

        CyclicBarrier(const CyclicBarrier&) = delete;
        CyclicBarrier& operator=(const CyclicBarrier&) = delete;
        CyclicBarrier(CyclicBarrier&&) = delete;
        CyclicBarrier& operator=(CyclicBarrier&&) = delete;

        // Sets the participant count for the next cycle.
        // Must be called before arrive() by ThreadRuntime - not thread-safe with arrive().
        void reset(uint32_t v_Size) noexcept {
            HADES_ASSERT(v_Size > 0);
            m_size.store(v_Size, std::memory_order_relaxed);
            m_arrived.store(0, std::memory_order_relaxed);
        }

        // Called by each participating thread at a barrier point.
        // Spins for HADES_BARRIER_SPIN_COUNT iterations, then sleeps on condvar.
        // Last arriving thread increments epoch and wakes all waiters.
        // Acts as a full acquire/release memory fence for all participants.
        void arrive() noexcept {
            const uint64_t currentEpoch = m_epoch.load(std::memory_order_relaxed);

            const uint32_t arrived = m_arrived.fetch_add(1, std::memory_order_acq_rel) + 1;
            const uint32_t size = m_size.load(std::memory_order_relaxed);

            HADES_ASSERT(arrived <= size);

            if (arrived == size) {
                // Last thread - reset arrived counter, advance epoch, wake all waiters
                m_arrived.store(0, std::memory_order_relaxed);
                m_epoch.fetch_add(1, std::memory_order_release);

                {
                    // Lock required so the notify is not lost between the load
                    // of epoch and the condvar sleep in waiting threads.
                    std::unique_lock<std::mutex> lock(m_mutex);
                }
                m_cv.notify_all();
                return;
            }

            // Spin phase
            for (uint32_t i = 0; i < HADES_BARRIER_SPIN_COUNT; ++i) {
                if (m_epoch.load(std::memory_order_acquire) != currentEpoch)
                    return;

#if HADES_COMPILER_MSVC
                _mm_pause();
#elif HADES_COMPILER_CLANG || HADES_COMPILER_GCC
                __builtin_ia32_pause();
#endif
            }

            // Sleep phase - fall back to condvar if spin budget exhausted
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this, currentEpoch]() noexcept {
                return m_epoch.load(std::memory_order_acquire) != currentEpoch;
            });
        }

        HADES_NODISCARD_MSG("Cannot discard epoch value") uint64_t epoch() const noexcept {
            return m_epoch.load(std::memory_order_acquire);
        }

        HADES_NODISCARD_MSG("Cannot discard barrier size") uint32_t size() const noexcept {
            return m_size.load(std::memory_order_relaxed);
        }

    private:
        // Hot path members - keep on same cache line
        alignas(64) std::atomic<uint32_t> m_size;      // participant count for current cycle
        std::atomic<uint32_t> m_arrived;   // threads that have called arrive()
        std::atomic<uint64_t> m_epoch;     // incremented by last arriving thread

        // Cold path - condvar fallback (separate cache line to avoid false sharing)
        alignas(64) std::mutex              m_mutex;
        std::condition_variable m_cv;
    };

} // namespace Hades::Runtime
