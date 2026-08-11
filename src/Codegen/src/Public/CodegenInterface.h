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
#include <HadesCodegen.h>
#include <CodegenTypes.h>

// Write-only, stateless, ABI-stable across an unknown set of future callers
// (Driver first, but also e.g. Spectra's SVK tooling directly). Never reads
// suite.toml/registry.ini itself - Driver/Core have already parsed those and
// hand over structured ScaffoldRequest/ResolveRequest data. Idempotent:
// identical input always produces identical output, no side effects beyond
// the requested writes.
//
// Pure-virtual + factory function rather than a directly-exported class so
// callers can't be assumed to share Codegen's build order. Once shipped,
// these virtual methods are never reordered or signature-altered. - only appended, or
// superseded by a future IHadesCodegen2. abiVersion() lets any caller detect
// what it is actually linked against.
namespace Hades::Codegen {

	class HADES_CODEGEN_API IHadesCodegen {
	public:
		virtual ~IHadesCodegen() = default;

		IHadesCodegen(const IHadesCodegen&) = delete;
		IHadesCodegen& operator=(const IHadesCodegen&) = delete;
		IHadesCodegen(IHadesCodegen&&) = delete;
		IHadesCodegen& operator=(IHadesCodegen&&) = delete;

		// Scaffolds one new test: fixture stub + suite.toml entry. Writes
		// v_OutStubPath (fixture header stub) and v_OutTomlPath (toml entry to
		// merge into suite.toml). Returns false on any write failure.
		virtual bool scaffold(const Hades::Runtime::ScaffoldRequest& ro_Request,
		                      const std::string& v_OutStubPath,
		                      const std::string& v_OutTomlPath) = 0;

		// Resolves the whole suite into a generated main.cpp (one #include +
		// HADES_REGISTER_FIXTURE per entry) plus a build file. Returns false on
		// any write failure.
		virtual bool resolve(const Hades::Runtime::ResolveRequest& ro_Request,
		                     const std::string& v_OutMainCppPath,
		                     const std::string& v_OutBuildFilePath) = 0;

		HADES_NODISCARD virtual uint32_t abiVersion() const noexcept = 0;

	protected:
		IHadesCodegen() = default;
	};

}

extern "C" HADES_CODEGEN_API Hades::Codegen::IHadesCodegen* createHadesCodegen();
extern "C" HADES_CODEGEN_API void destroyHadesCodegen(Hades::Codegen::IHadesCodegen* p_instance);
