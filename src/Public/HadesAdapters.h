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

#include "Adapter.h"

namespace Hades::Runtime {
    struct CpuEvent
    {
        std::chrono::steady_clock::time_point tp = {};
    };

    class HADES_RUNTIME_API NullDeviceAdapter final
        : public IDeviceAdapter<NullDeviceAdapter>{
    public:
        using event_type = CpuEvent;
        NullDeviceAdapter() = default;
        ~NullDeviceAdapter() = default;

        NullDeviceAdapter(const NullDeviceAdapter&) = delete;
        NullDeviceAdapter& operator=(const NullDeviceAdapter&) = delete;
        NullDeviceAdapter(NullDeviceAdapter&&) = delete;
        NullDeviceAdapter& operator=(NullDeviceAdapter&&) = delete;

        // CPU work is synchronous - nothing to wait for.
        HADES_FORCEINLINE void synchronizeImpl() noexcept {}

        // Snapshots the current steady_clock time into ro_Event.
        HADES_FORCEINLINE void recordEventImpl(CpuEvent& ro_Event) noexcept {
            ro_Event.tp = std::chrono::steady_clock::now();
        }

        // Returns elapsed time between two CpuEvents in milliseconds.
        HADES_NODISCARD_MSG("Cannot discard elapsed cpu event time")
            HADES_FORCEINLINE float elapsedTimeImpl(
                const CpuEvent& ro_Start,
                const CpuEvent& ro_End) const noexcept {
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                ro_End.tp - ro_Start.tp).count();
            return static_cast<float>(ns) * 1e-6f;   // ns -> ms
        }

        // Delegates to std::memcpy - no device transfer.
        HADES_FORCEINLINE void memcpyToHostImpl(
            void* p_Dst,
            const void* p_Src,
            std::size_t v_Bytes) noexcept {
            std::memcpy(p_Dst, p_Src, v_Bytes);
        }

        // No stream concept for CPU - returns nullptr.
        HADES_NODISCARD_MSG("Cannot discard cpu stream handle")
            HADES_FORCEINLINE void* streamHandleImpl() noexcept {
            return nullptr;
        }
    };

}


// CudaDeviceAdapter - stub (GPU, not yet implemented)
//
// These will be provided in a future phase once GPU support is added.
// Include guards below prevent accidental use before implementation.

#ifdef HADES_ENABLE_CUDA
HADES_STATIC_ASSERT(false,
              "CudaDeviceAdapter is not yet implemented. "
              "Do not define HADES_ENABLE_CUDA until  GPU implementation is complete.");
#endif
