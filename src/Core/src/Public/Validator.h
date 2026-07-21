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

#include "HadesHash.h"

namespace Hades::Runtime {
	static constexpr uint32_t MAX_UNIQUE_HASHES = 32;
	template<typename A>
	class DeterminismValidator final {
		using accumulator_ = A;

	public:
		explicit DeterminismValidator(accumulator_& ro_Accumulator) noexcept
			: m_Accumulator(ro_Accumulator)
			, m_referenceHash(0)
			, m_hasReference(false)
			, m_deterministic(true)
			, m_uniqueHashCount(0)
			, m_firstDivergenceSlice(0)
			, m_currentSlice(0) {
			for (uint32_t i = 0; i < MAX_UNIQUE_HASHES; ++i)
				m_uniqueHashes[i] = 0;
		}

		~DeterminismValidator() = default;

		DeterminismValidator(const DeterminismValidator&) = delete;
		DeterminismValidator& operator=(const DeterminismValidator&) = delete;
		DeterminismValidator(DeterminismValidator&&) = delete;
		DeterminismValidator& operator=(DeterminismValidator&&) = delete;

		// Feed one per-thread hash into the accumulator.
		// Must be called in fixed thread index order (0, 1, 2, ...).
		void feedThreadHash(uint64_t v_Hash) noexcept {
			m_Accumulator.accumulate(v_Hash);
		}

		// Finalizes the combined slice hash and validates against reference.
		// Call after all per-thread hashes have been fed via feedThreadHash().
		void validateSlice() noexcept {
			const uint64_t sliceHash = m_Accumulator.finalize();

			if (HADES_UNLIKELY(!m_hasReference)) {
				m_referenceHash = sliceHash;
				m_hasReference = true;
				internalTrackUniqueHash(sliceHash);
			} else if (HADES_UNLIKELY(sliceHash != m_referenceHash)) {
				if (m_deterministic) {
					// First divergence
					m_deterministic = false;
					m_firstDivergenceSlice = m_currentSlice;
				}
				internalTrackUniqueHash(sliceHash);
			}

			++m_currentSlice;
		}


		HADES_NODISCARD_MSG("Cannot discard determinism flag")
			bool isDeterministic() const noexcept {
			return m_deterministic;
		}

		HADES_NODISCARD_MSG("Cannot discard reference hash")
			uint64_t referenceHash() const noexcept {
			return m_referenceHash;
		}

		HADES_NODISCARD_MSG("Cannot discard unique hash count")
			uint32_t uniqueHashCount() const noexcept {
			return m_uniqueHashCount;
		}

		HADES_NODISCARD_MSG("Cannot discard first divergence slice")
			uint32_t firstDivergenceSlice() const noexcept {
			return m_firstDivergenceSlice;
		}

	private:
		// Tracks a newly observed hash if not already seen.
		// Capped at MAX_UNIQUE_HASHES - excess entries are silently dropped.
		void internalTrackUniqueHash(uint64_t v_Hash) noexcept {
			for (uint32_t i = 0; i < m_uniqueHashCount; ++i) {
				if (m_uniqueHashes[i] == v_Hash)
					return;   // already tracked
			}
			if (HADES_LIKELY(m_uniqueHashCount < MAX_UNIQUE_HASHES))
				m_uniqueHashes[m_uniqueHashCount++] = v_Hash;
		}

		accumulator_& m_Accumulator;

		uint64_t m_referenceHash;
		bool     m_hasReference;
		bool     m_deterministic;
		uint32_t m_uniqueHashCount;
		uint32_t m_firstDivergenceSlice;
		uint32_t m_currentSlice;

		uint64_t m_uniqueHashes[MAX_UNIQUE_HASHES];
	};
}
