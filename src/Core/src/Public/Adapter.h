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

#include <cstddef>
#include <cstdint>

#include "HadesCompiler.h"

namespace Hades::Runtime {

    // Lets FixtureRegistry's factory pass a type-erased adapter reference and recover it via static_cast - never used for dispatch.
    class IDeviceAdapterBase {
    public:
        virtual ~IDeviceAdapterBase() = default;
    };

    template<typename D>
    class IDeviceAdapter
    {
        using derived_ = D;   // E1 rule 2: template param aliased in PascalCase

        HADES_NODISCARD constexpr derived_& self() noexcept {
            return static_cast<derived_&>(*this);
        }

        HADES_NODISCARD constexpr const derived_& self() const noexcept {
            return static_cast<const derived_&>(*this);
        }

    public:
        // Blocks the calling thread until all previously submitted device work
        // on this adapter's stream has completed.
        void synchronize() noexcept {
            self().synchronizeImpl();
        }


        // Records a device-side timing event into the adapter's stream.
        // Event type is implementation-defined by derived_ (e.g. cudaEvent_t).
        template<typename E>
        void recordEvent(E& ro_Event) noexcept {
            self().recordEventImpl(ro_Event);
        }

        // Returns elapsed device time in milliseconds between two recorded events.
        template<typename E>
        HADES_NODISCARD_MSG("Cannot discard elapsed delta time") float elapsedTime(const E& ro_Start, const E& ro_End) const noexcept {
            return self().elapsedTimeImpl(ro_Start, ro_End);
        }

        // Copies v_Bytes bytes from device pointer p_Src to host pointer p_Dst.
        // Blocking on the adapter's stream.
        void memcpyToHost(void* p_Dst, const void* p_Src, std::size_t v_Bytes) noexcept {
            self().memcpyToHostImpl(p_Dst, p_Src, v_Bytes);
        }


        // Returns the underlying device stream handle (e.g. cudaStream_t).
        // Fixture calls this to get the stream for kernel launches.
        HADES_NODISCARD_MSG("Cannot discard stream handle for device") auto streamHandle() noexcept {
            return self().streamHandleImpl();
        }

    protected:
        IDeviceAdapter() = default;
        ~IDeviceAdapter() = default;

    public:
        IDeviceAdapter(const IDeviceAdapter&) = delete;
        IDeviceAdapter& operator=(const IDeviceAdapter&) = delete;
        IDeviceAdapter(IDeviceAdapter&&) = delete;
        IDeviceAdapter& operator=(IDeviceAdapter&&) = delete;
    };

}
