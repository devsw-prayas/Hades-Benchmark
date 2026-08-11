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
*//*
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
#include "HadesCompiler.h"

namespace Hades::Runtime {

    template<typename D>
    class IChronoPoint{
        using derived_ = D;   // E1 rule 2: template param aliased in PascalCase

        HADES_NODISCARD constexpr derived_& self() noexcept {
            return static_cast<derived_&>(*this);
        }

        HADES_NODISCARD constexpr const derived_& self() const noexcept {
            return static_cast<const derived_&>(*this);
        }

    public:
        // Returns an opaque timestamp token. Type is implementation-defined.
        HADES_NODISCARD_MSG("Cannot discard current timestamp token") auto now() noexcept {
            return self().nowImpl();
        }

        // Returns elapsed time in nanoseconds between two timestamp tokens.
        // T is the opaque timestamp type produced by now() on the derived_ clock.
        template<typename T>
        HADES_NODISCARD_MSG("Cannot discard elapsed delta") uint64_t delta(T v_Begin, T v_End) const noexcept {
            return self().deltaImpl(v_Begin, v_End);
        }

    protected:
        IChronoPoint() = default;
        ~IChronoPoint() = default;

    public:
        IChronoPoint(const IChronoPoint&) = delete;
        IChronoPoint& operator=(const IChronoPoint&) = delete;
        IChronoPoint(IChronoPoint&&) = delete;
        IChronoPoint& operator=(IChronoPoint&&) = delete;
    };

}
