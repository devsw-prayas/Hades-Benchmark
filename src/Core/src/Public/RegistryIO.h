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
#include "HadesCompiler.h"
#include "CodegenTypes.h"

#include <string>
#include <vector>

// registry.ini's grammar, defined here since Driver's resolve()/scaffold()
// calls have no other source for the FixtureEntry list. Flat ini: repeated
// "[FixtureName]" sections, each with
// exactly four required string keys mapping 1:1 onto FixtureEntry. Every
// value is a bare string (a C++ type name or path token) - unlike TomlIO's
// grammar, there is no typed value model to parse here.
namespace Hades::Runtime {

	struct RegistryIniError final {
		uint32_t    m_Line = 0;
		std::string m_Message;
	};

	// A bare C++ identifier - letters/digits/underscore, not digit-leading.
	HADES_NODISCARD_MSG("Validity check result must be checked")
		HADES_RUNTIME_API bool isIdentifier(const std::string& ro_Text);

	// One or more isIdentifier() segments joined by "::" - what registry.ini's
	// adapter/chrono/hash fields are expected to hold. Shared by readRegistryIni
	// (parse time) and Driver's new-test (scaffold time) so a bad type name is
	// rejected once, consistently, in both places it can first appear.
	HADES_NODISCARD_MSG("Validity check result must be checked")
		HADES_RUNTIME_API bool isCppTypeName(const std::string& ro_Text);

	// Accumulates every error found rather than bailing on the first (same
	// convention as readToml) - a future `validate` subcommand wants full
	// diagnostics in one pass.
	HADES_NODISCARD_MSG("Parse result must be checked")
		HADES_RUNTIME_API bool readRegistryIni(const std::string& v_Path,
		                      std::vector<FixtureEntry>& ro_OutFixtures,
		                      std::vector<RegistryIniError>& ro_OutErrors);

	// v_Append = true: opens in append mode and writes only ro_Fixtures, no
	// read or re-parse of any existing content - matching writeToml's
	// convention (Configuration.h's writeSuiteToml, TomlIO.h's writeToml).
	HADES_NODISCARD_MSG("Write result must be checked")
		HADES_RUNTIME_API bool writeRegistryIni(const std::string& v_Path,
		                       const std::vector<FixtureEntry>& ro_Fixtures,
		                       bool v_Append);

}
