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

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Minimal, hand-rolled TOML value model - only the subset the suite manifest
// grammar needs: flat scalar key = value pairs inside repeated
// [[array-of-tables]] blocks. No nested tables, no inline tables, no
// arrays-as-values, no multi-line strings.

namespace Hades::Runtime {

	enum class TomlType : uint8_t {
		String,
		Integer,
		Float,
		Boolean,
	};

	struct TomlValue final {
		TomlType    m_Type = TomlType::String;
		std::string m_StringValue;
		int64_t     m_IntValue   = 0;
		double      m_FloatValue = 0.0;
		bool        m_BoolValue  = false;
	};

	// Ordered, not hashed - table order round-trips through the writer,
	// matching the manifest's own "declaration order is semantic" rule.
	using TomlTable = std::vector<std::pair<std::string, TomlValue>>;

	struct TomlParseError final {
		uint32_t    m_Line = 0;
		std::string m_Message;
	};

}
