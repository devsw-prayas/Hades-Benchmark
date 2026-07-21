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
#include "TomlValue.h"

#include <string>
#include <vector>

namespace Hades::Runtime {

	// Reads every [[v_ArrayName]] block in v_Path into ro_OutTables, in file
	// order. Accumulates every error found rather than bailing on the first
	// (a future `validate` subcommand wants full diagnostics in one pass).
	// Returns false iff ro_OutErrors ends up non-empty; ro_OutTables is still
	// populated with whatever parsed successfully either way.
	HADES_NODISCARD_MSG("Parse result must be checked")
		bool readToml(const std::string& v_Path,
		              const std::string& v_ArrayName,
		              std::vector<TomlTable>& ro_OutTables,
		              std::vector<TomlParseError>& ro_OutErrors);

	// v_Append = true: opens in append mode and writes only ro_Tables, no read
	// or re-parse of any existing content. v_Append = false: full rewrite.
	HADES_NODISCARD_MSG("Write result must be checked")
		bool writeToml(const std::string& v_Path,
		               const std::string& v_ArrayName,
		               const std::vector<TomlTable>& ro_Tables,
		               bool v_Append);

	// Table lookup helpers - pointer return (nullptr = missing or wrong type),
	// matching FixtureRegistry::find()'s existing pattern.
	HADES_NODISCARD const std::string* findTomlString(const TomlTable& ro_Table, const std::string& v_Key);
	HADES_NODISCARD const int64_t*     findTomlInt(const TomlTable& ro_Table, const std::string& v_Key);
	HADES_NODISCARD const double*      findTomlFloat(const TomlTable& ro_Table, const std::string& v_Key);
	HADES_NODISCARD const bool*        findTomlBool(const TomlTable& ro_Table, const std::string& v_Key);

} // namespace Hades::Runtime
