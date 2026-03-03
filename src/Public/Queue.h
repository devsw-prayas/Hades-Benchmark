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

namespace Hades::Runtime::Queues {
	template<typename T, uint32_t Capacity>
	class ChaseLevDeque final
	{
		HADES_STATIC_ASSERT((Capacity& (Capacity - 1)) == 0, "ChaseLevDeque: Capacity must be a power of two");
		HADES_STATIC_ASSERT(Capacity > 0, "ChaseLevDeque: Capacity must be greater than zero");
		using value_type_ = T;

		static constexpr uint32_t MASK = Capacity - 1;

	public:
		ChaseLevDeque() noexcept
			: m_bottom(0)
			, m_top(0) {
		}

		~ChaseLevDeque() = default;

		ChaseLevDeque(const ChaseLevDeque&) = delete;
		ChaseLevDeque& operator=(const ChaseLevDeque&) = delete;
		ChaseLevDeque(ChaseLevDeque&&) = delete;
		ChaseLevDeque& operator=(ChaseLevDeque&&) = delete;
		enum class StealResult : uint8_t
		{
			Success,   // item stolen successfully
			Empty,     // deque was empty
			Abort,     // lost race with another thief or owner - retry
		};

		// Pushes an item onto the bottom of the deque.
		// Must only be called by the owner thread.
		// Returns false if the deque is full.
		HADES_NODISCARD_MSG("Cannot discard push result - deque may be full")
			bool push(value_type_ v_Item) noexcept {
			const uint32_t bottom = m_bottom.load(std::memory_order_relaxed);
			const uint32_t top = m_top.load(std::memory_order_acquire);

			if (HADES_UNLIKELY(bottom - top >= Capacity))
				return false;   // full

			m_buffer[bottom & MASK] = v_Item;

			// Release so thieves see the written item before the updated bottom
			std::atomic_thread_fence(std::memory_order_release);
			m_bottom.store(bottom + 1, std::memory_order_relaxed);
			return true;
		}

		// Pops an item from the bottom of the deque.
		// Must only be called by the owner thread.
		// Returns false if the deque is empty.
		bool pop(value_type_& ro_Item) noexcept {
			const uint32_t bottom = m_bottom.load(std::memory_order_relaxed) - 1;
			m_bottom.store(bottom, std::memory_order_relaxed);

			// seq_cst fence to synchronize with steal() on top
			std::atomic_thread_fence(std::memory_order_seq_cst);

			uint32_t top = m_top.load(std::memory_order_relaxed);

			if (HADES_LIKELY(top <= bottom)) {
				ro_Item = m_buffer[bottom & MASK];

				if (top == bottom) {
					// Last item - race with steal(); try to win by advancing top
					if (!m_top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
						// Lost the race - deque now empty
						m_bottom.store(bottom + 1, std::memory_order_relaxed);
						return false;
					}
					m_bottom.store(bottom + 1, std::memory_order_relaxed);
				}
				return true;
			}

			// Deque was empty before the decrement - restore bottom
			m_bottom.store(bottom + 1, std::memory_order_relaxed);
			return false;
		}

		// Attempts to steal an item from the top of the deque.
		// Returns StealResult to distinguish success, empty, and lost-race cases.
		StealResult steal(value_type_& ro_Item) noexcept {
			uint32_t top = m_top.load(std::memory_order_acquire);
			std::atomic_thread_fence(std::memory_order_seq_cst);
			const uint32_t bottom = m_bottom.load(std::memory_order_acquire);

			if (top >= bottom)
				return StealResult::Empty;

			ro_Item = m_buffer[top & MASK];

			if (!m_top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
				return StealResult::Abort;   // lost race with another thief or pop()

			return StealResult::Success;
		}

		HADES_NODISCARD_MSG("Cannot discard deque size") uint32_t approxSize() const noexcept {
			const uint32_t bottom = m_bottom.load(std::memory_order_relaxed);
			const uint32_t top = m_top.load(std::memory_order_relaxed);
			return (bottom >= top) ? (bottom - top) : 0;
		}

		HADES_NODISCARD_MSG("Cannot discard empty check") bool empty() const noexcept {
			return approxSize() == 0;
		}

	private:
		// Hot path - bottom and top on separate cache lines to avoid false sharing
		// between owner (writes bottom) and thieves (write top)
		alignas(64) std::atomic<uint32_t> m_bottom;
		alignas(64) std::atomic<uint32_t> m_top;

		// Circular buffer - separate cache line from the index atomics
		alignas(64) value_type_ m_buffer[Capacity];
	};

	template<typename T, uint32_t Capacity>
	class SpscQueue final {
		HADES_STATIC_ASSERT((Capacity& (Capacity - 1)) == 0, "SpscQueue: Capacity must be a power of two");
		HADES_STATIC_ASSERT(Capacity > 1, "SpscQueue: Capacity must be greater than one");

		using value_type_ = T;

		static constexpr uint32_t MASK = Capacity - 1;

	public:
		SpscQueue() noexcept
			: m_head(0)
			, m_tail(0) {
		}

		~SpscQueue() = default;

		SpscQueue(const SpscQueue&) = delete;
		SpscQueue& operator=(const SpscQueue&) = delete;
		SpscQueue(SpscQueue&&) = delete;
		SpscQueue& operator=(SpscQueue&&) = delete;

		// Attempts to push an item into the queue.
		// Returns false if the queue is full - caller must retry.
		// Must only be called from the producer thread.
		HADES_NODISCARD_MSG("Cannot discard push result - queue may be full")
			bool push(value_type_ v_Item) noexcept {
			const uint32_t tail = m_tail.load(std::memory_order_relaxed);
			const uint32_t nextTail = (tail + 1) & MASK;

			if (HADES_UNLIKELY(nextTail == m_head.load(std::memory_order_acquire)))
				return false;   // full

			m_buffer[tail] = v_Item;
			m_tail.store(nextTail, std::memory_order_release);
			return true;
		}

		// Attempts to pop an item from the queue into ro_Item.
		// Returns false if the queue is empty.
		// Must only be called from the consumer thread.
		bool pop(value_type_& ro_Item) noexcept {
			const uint32_t head = m_head.load(std::memory_order_relaxed);

			if (HADES_UNLIKELY(head == m_tail.load(std::memory_order_acquire)))
				return false;   // empty

			ro_Item = m_buffer[head];
			m_head.store((head + 1) & MASK, std::memory_order_release);
			return true;
		}

		HADES_NODISCARD_MSG("Cannot discard empty check") bool empty() const noexcept {
			return m_head.load(std::memory_order_acquire)
				== m_tail.load(std::memory_order_acquire);
		}

		HADES_NODISCARD_MSG("Cannot discard queue size") uint32_t approxSize() const noexcept {
			const uint32_t tail = m_tail.load(std::memory_order_acquire);
			const uint32_t head = m_head.load(std::memory_order_acquire);
			return (tail - head) & MASK;
		}

	private:
		// Producer writes tail, consumer reads it - separate cache lines
		// to prevent false sharing between the two threads.
		alignas(64) std::atomic<uint32_t> m_head;   // consumer advances
		alignas(64) std::atomic<uint32_t> m_tail;   // producer advances

		// Circular buffer - separate cache line from the index atomics
		alignas(64) value_type_ m_buffer[Capacity];
	};
}