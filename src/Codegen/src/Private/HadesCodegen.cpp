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
