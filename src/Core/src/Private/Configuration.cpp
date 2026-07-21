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
#include <Configuration.h>
#include <TomlIO.h>
#include <HadesDiagnostics.h>

namespace Hades::Runtime {

	namespace {

		constexpr const char* KNOWN_KEYS[] = {
			"id", "fixture", "kind", "warmup_count", "cv_threshold",
			"helper_thread_count", "read_criticality", "fixed_slice_count",
		};

		bool isKnownKey(const std::string& v_Key) {
			for (const char* p_known : KNOWN_KEYS) {
				if (v_Key == p_known) {
					return true;
				}
			}
			return false;
		}

		void reportUnknownKeys(const TomlTable& ro_Table, size_t v_EntryIndex,
		                        std::vector<TomlParseError>& ro_OutErrors) {
			for (const auto& r_pair : ro_Table) {
				if (!isKnownKey(r_pair.first)) {
					ro_OutErrors.push_back({ 0, "[[test]] entry " + std::to_string(v_EntryIndex) +
					                          ": unknown key '" + r_pair.first + "'" });
				}
			}
		}

		bool mapTableToEntry(const TomlTable& ro_Table, size_t v_EntryIndex,
		                      SuiteTestEntry& ro_OutEntry, std::vector<TomlParseError>& ro_OutErrors) {
			const std::string* p_id = findTomlString(ro_Table, "id");
			const std::string* p_fixture = findTomlString(ro_Table, "fixture");
			if (p_id == nullptr) {
				ro_OutErrors.push_back({ 0, "[[test]] entry " + std::to_string(v_EntryIndex) + ": missing required key 'id'" });
			}
			if (p_fixture == nullptr) {
				ro_OutErrors.push_back({ 0, "[[test]] entry " + std::to_string(v_EntryIndex) + ": missing required key 'fixture'" });
			}
			if (p_id == nullptr || p_fixture == nullptr) {
				return false;
			}

			setId(ro_OutEntry.m_Config, *p_id);
			ro_OutEntry.m_FixtureName = *p_fixture;

			if (const std::string* p_kind = findTomlString(ro_Table, "kind")) {
				if (*p_kind == "performance") {
					setKind(ro_OutEntry.m_Config, TestKind::Performance);
				} else if (*p_kind == "correctness") {
					setKind(ro_OutEntry.m_Config, TestKind::Correctness);
				} else {
					ro_OutErrors.push_back({ 0, "[[test]] entry " + std::to_string(v_EntryIndex) +
					                          ": invalid kind '" + *p_kind + "' (expected 'performance' or 'correctness')" });
					return false;
				}
			}

			if (const int64_t* p_warmup = findTomlInt(ro_Table, "warmup_count")) {
				setWarmup(ro_OutEntry.m_Config, static_cast<uint32_t>(*p_warmup));
			}
			if (const double* p_cv = findTomlFloat(ro_Table, "cv_threshold")) {
				setCvThreshold(ro_OutEntry.m_Config, *p_cv);
			}
			if (const int64_t* p_helpers = findTomlInt(ro_Table, "helper_thread_count")) {
				setHelperThreadCount(ro_OutEntry.m_Config, static_cast<uint32_t>(*p_helpers));
			}
			if (const double* p_criticality = findTomlFloat(ro_Table, "read_criticality")) {
				setReadCriticality(ro_OutEntry.m_Config, *p_criticality);
			}
			if (const int64_t* p_slices = findTomlInt(ro_Table, "fixed_slice_count")) {
				setFixedSliceCount(ro_OutEntry.m_Config, static_cast<uint32_t>(*p_slices));
			}

			reportUnknownKeys(ro_Table, v_EntryIndex, ro_OutErrors);
			return true;
		}

		TomlTable entryToTable(const SuiteTestEntry& ro_Entry) {
			TomlTable l_table;

			TomlValue l_id;
			l_id.m_Type = TomlType::String;
			l_id.m_StringValue = ro_Entry.m_Config.m_Id;
			l_table.emplace_back("id", std::move(l_id));

			TomlValue l_fixture;
			l_fixture.m_Type = TomlType::String;
			l_fixture.m_StringValue = ro_Entry.m_FixtureName;
			l_table.emplace_back("fixture", std::move(l_fixture));

			TomlValue l_kind;
			l_kind.m_Type = TomlType::String;
			l_kind.m_StringValue = (ro_Entry.m_Config.m_Kind == TestKind::Correctness) ? "correctness" : "performance";
			l_table.emplace_back("kind", std::move(l_kind));

			if (ro_Entry.m_Config.m_WarmupCount != 0) {
				TomlValue l_value;
				l_value.m_Type = TomlType::Integer;
				l_value.m_IntValue = static_cast<int64_t>(ro_Entry.m_Config.m_WarmupCount);
				l_table.emplace_back("warmup_count", std::move(l_value));
			}
			if (ro_Entry.m_Config.m_CvThreshold != 0.0) {
				TomlValue l_value;
				l_value.m_Type = TomlType::Float;
				l_value.m_FloatValue = ro_Entry.m_Config.m_CvThreshold;
				l_table.emplace_back("cv_threshold", std::move(l_value));
			}
			if (ro_Entry.m_Config.m_HelperThreadCount != 0) {
				TomlValue l_value;
				l_value.m_Type = TomlType::Integer;
				l_value.m_IntValue = static_cast<int64_t>(ro_Entry.m_Config.m_HelperThreadCount);
				l_table.emplace_back("helper_thread_count", std::move(l_value));
			}
			if (ro_Entry.m_Config.m_ReadCriticality != 0.0) {
				TomlValue l_value;
				l_value.m_Type = TomlType::Float;
				l_value.m_FloatValue = ro_Entry.m_Config.m_ReadCriticality;
				l_table.emplace_back("read_criticality", std::move(l_value));
			}
			if (ro_Entry.m_Config.m_FixedSliceCount != 0) {
				TomlValue l_value;
				l_value.m_Type = TomlType::Integer;
				l_value.m_IntValue = static_cast<int64_t>(ro_Entry.m_Config.m_FixedSliceCount);
				l_table.emplace_back("fixed_slice_count", std::move(l_value));
			}

			return l_table;
		}

	} // namespace

	bool readSuiteToml(const std::string& v_Path, std::vector<SuiteTestEntry>& ro_OutTests,
	                    std::vector<TomlParseError>& ro_OutErrors) {
		ro_OutTests.clear();

		std::vector<TomlTable> l_tables;
		const bool l_readOk = readToml(v_Path, "test", l_tables, ro_OutErrors);
		HADES_UNUSED(l_readOk); // failure is reflected via ro_OutErrors below

		for (size_t l_i = 0; l_i < l_tables.size(); ++l_i) {
			SuiteTestEntry l_entry;
			if (mapTableToEntry(l_tables[l_i], l_i, l_entry, ro_OutErrors)) {
				ro_OutTests.push_back(std::move(l_entry));
			}
		}

		return ro_OutErrors.empty();
	}

	bool writeSuiteToml(const std::string& v_Path, const std::vector<SuiteTestEntry>& ro_Tests, bool v_Append) {
		std::vector<TomlTable> l_tables;
		l_tables.reserve(ro_Tests.size());
		for (const SuiteTestEntry& r_entry : ro_Tests) {
			l_tables.push_back(entryToTable(r_entry));
		}

		return writeToml(v_Path, "test", l_tables, v_Append);
	}

} // namespace Hades::Runtime
