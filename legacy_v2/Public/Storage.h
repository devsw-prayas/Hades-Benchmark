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

#include "BenchmarkResult.h"

namespace Hades::Runtime {
    template<typename D>
    class IStorageBackend {
        using derived_ = D;   

        HADES_NODISCARD constexpr derived_& self() noexcept {
            return static_cast<derived_&>(*this);
        }

        HADES_NODISCARD constexpr const derived_& self() const noexcept {
            return static_cast<const derived_&>(*this);
        }

    public:
        // Delivers exactly one completed BenchmarkResult to the backend.
        // Called by HadesEngine once per run, after all slices are finalized.
        void store(const BenchmarkResult& r_Result) noexcept {
            self().storeImpl(r_Result);
        }

    protected:
        IStorageBackend() = default;
        ~IStorageBackend() = default;

    public:
        IStorageBackend(const IStorageBackend&) = delete;
        IStorageBackend& operator=(const IStorageBackend&) = delete;
        IStorageBackend(IStorageBackend&&) = delete;
        IStorageBackend& operator=(IStorageBackend&&) = delete;
    };

} // namespace Hades
