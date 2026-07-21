#include <FixtureRegistry.h>

namespace Hades::Runtime {

	void FixtureRegistry::registerFixture(const std::string& v_Name, FactoryFn v_Factory) {
		HADES_ASSERT(v_Factory != nullptr);
		m_factories.emplace(v_Name, std::move(v_Factory));
	}

	const FixtureRegistry::FactoryFn* FixtureRegistry::find(const std::string& v_Name) const {
		const auto it = m_factories.find(v_Name);
		return (it != m_factories.end()) ? &it->second : nullptr;
	}

}
