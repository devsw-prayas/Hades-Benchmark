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

// registry.ini grammar: repeated "[FixtureName]" sections, four required bare-string keys each, no typed value model.
namespace Hades::Runtime {

	struct RegistryIniError final {
		uint32_t    m_Line = 0;
		std::string m_Message;
	};

	// A bare C++ identifier - letters/digits/underscore, not digit-leading.
	HADES_NODISCARD_MSG("Validity check result must be checked")
		HADES_RUNTIME_API bool isIdentifier(const std::string& ro_Text);

	// isIdentifier() segments joined by "::"; shared by parse-time and scaffold-time so a bad type name is rejected consistently.
	HADES_NODISCARD_MSG("Validity check result must be checked")
		HADES_RUNTIME_API bool isCppTypeName(const std::string& ro_Text);

	// Accumulates every error instead of bailing on the first, for a future `validate` subcommand's full diagnostics.
	HADES_NODISCARD_MSG("Parse result must be checked")
		HADES_RUNTIME_API bool readRegistryIni(const std::string& v_Path,
		                      std::vector<FixtureEntry>& ro_OutFixtures,
		                      std::vector<RegistryIniError>& ro_OutErrors);

	// v_Append = true writes only ro_Fixtures in append mode, no read/re-parse of existing content.
	HADES_NODISCARD_MSG("Write result must be checked")
		HADES_RUNTIME_API bool writeRegistryIni(const std::string& v_Path,
		                       const std::vector<FixtureEntry>& ro_Fixtures,
		                       bool v_Append);

}
