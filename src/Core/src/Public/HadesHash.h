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

namespace Hades::Runtime {
	template<typename D>
	class IHashAccumulator {
		using derived_ = D;

		HADES_NODISCARD constexpr derived_& self() noexcept {
			return static_cast<derived_&>(*this);
		}

		HADES_NODISCARD constexpr const derived_& self() const noexcept {
			return static_cast<const derived_&>(*this);
		}

	public:
		// Feed one per-thread hash into the accumulator.
		// Must be called in fixed thread index order (0, 1, 2, ...).
		void accumulate(uint64_t v_Hash) noexcept {
			self().accumulateImpl(v_Hash);
		}

		// Returns the combined hash for this slice and resets internal state.
		HADES_NODISCARD_MSG("Cannot discard finalized combined hash")
			uint64_t finalize() noexcept {
			return self().finalizeImpl();
		}

		// Resets accumulator state for the next slice.
		void reset() noexcept {
			self().resetImpl();
		}

	protected:
		IHashAccumulator() = default;
		~IHashAccumulator() = default;

	public:
		IHashAccumulator(const IHashAccumulator&) = delete;
		IHashAccumulator& operator=(const IHashAccumulator&) = delete;
		IHashAccumulator(IHashAccumulator&&) = delete;
		IHashAccumulator& operator=(IHashAccumulator&&) = delete;
	};

	class XorHashAccumulator final : public IHashAccumulator<XorHashAccumulator>
	{
	public:
		XorHashAccumulator() noexcept
			: m_state(0) {
		}

		~XorHashAccumulator() = default;

		void accumulateImpl(uint64_t v_Hash) noexcept {
			m_state ^= v_Hash;
		}

		HADES_NODISCARD_MSG("Cannot discard finalized combined hash")
			uint64_t finalizeImpl() noexcept {
			const uint64_t result = m_state;
			m_state = 0;
			return result;
		}

		void resetImpl() noexcept {
			m_state = 0;
		}

	private:
		uint64_t m_state;
	};


	class FnvChainHashAccumulator final
		: public IHashAccumulator<FnvChainHashAccumulator>
	{
		static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
		static constexpr uint64_t FNV_PRIME = 1099511628211ULL;

	public:
		FnvChainHashAccumulator() noexcept
			: m_state(FNV_OFFSET_BASIS) {
		}

		~FnvChainHashAccumulator() = default;

		void accumulateImpl(uint64_t v_Hash) noexcept {
			// Mix each byte of v_Hash into the FNV-1a chain
			const uint8_t* p_Bytes = reinterpret_cast<const uint8_t*>(&v_Hash);
			for (uint32_t i = 0; i < sizeof(uint64_t); ++i) {
				m_state ^= static_cast<uint64_t>(p_Bytes[i]);
				m_state *= FNV_PRIME;
			}
		}

		HADES_NODISCARD_MSG("Cannot discard finalized combined hash")
			uint64_t finalizeImpl() noexcept {
			const uint64_t result = m_state;
			m_state = FNV_OFFSET_BASIS;
			return result;
		}

		void resetImpl() noexcept {
			m_state = FNV_OFFSET_BASIS;
		}

	private:
		uint64_t m_state;
	};
} 
