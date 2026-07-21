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
#include "HadesCodegen.h"
#include <HadesDiagnostics.h>

// Concrete Codegen implementation. scaffold()/resolve() are intentionally
// unimplemented here - the toml reader/writer and the generated main.cpp
// templater are separate follow-up work (Hades v3 Architecture doc SS6, steps
// 6). This step only wires the ABI-stable interface + factory boundary.
namespace Hades::Codegen {

	namespace {

		class HadesCodegenImpl final : public IHadesCodegen {
		public:
			bool scaffold(const Hades::Runtime::ScaffoldRequest& ro_Request,
			              const std::string& v_OutStubPath,
			              const std::string& v_OutTomlPath) override {
				HADES_UNUSED(ro_Request);
				HADES_UNUSED(v_OutStubPath);
				HADES_UNUSED(v_OutTomlPath);
				return false;
			}

			bool resolve(const Hades::Runtime::ResolveRequest& ro_Request,
			             const std::string& v_OutMainCppPath,
			             const std::string& v_OutBuildFilePath) override {
				HADES_UNUSED(ro_Request);
				HADES_UNUSED(v_OutMainCppPath);
				HADES_UNUSED(v_OutBuildFilePath);
				return false;
			}

			HADES_NODISCARD uint32_t abiVersion() const noexcept override {
				return 1;
			}
		};

	} // namespace

} // namespace Hades::Codegen

extern "C" HADES_CODEGEN_API Hades::Codegen::IHadesCodegen* createHadesCodegen() {
	return new Hades::Codegen::HadesCodegenImpl();
}

extern "C" HADES_CODEGEN_API void destroyHadesCodegen(Hades::Codegen::IHadesCodegen* p_instance) {
	delete p_instance;
}
