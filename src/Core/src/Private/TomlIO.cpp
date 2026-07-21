#include <TomlIO.h>
#include <HadesDiagnostics.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>

namespace Hades::Runtime {

	namespace {

		bool readWholeFile(const std::string& v_Path, std::string& ro_OutContent) {
			std::FILE* l_file = std::fopen(v_Path.c_str(), "rb");
			if (l_file == nullptr) {
				return false;
			}

			std::fseek(l_file, 0, SEEK_END);
			const long l_size = std::ftell(l_file);
			std::fseek(l_file, 0, SEEK_SET);

			if (l_size < 0) {
				std::fclose(l_file);
				return false;
			}

			ro_OutContent.resize(static_cast<size_t>(l_size));
			const size_t l_read = l_size > 0
				? std::fread(ro_OutContent.data(), 1, static_cast<size_t>(l_size), l_file)
				: 0;
			std::fclose(l_file);

			return l_read == static_cast<size_t>(l_size);
		}

		bool writeWholeFile(const std::string& v_Path, const std::string& ro_Content, bool v_Append) {
			std::FILE* l_file = std::fopen(v_Path.c_str(), v_Append ? "ab" : "wb");
			if (l_file == nullptr) {
				return false;
			}

			const size_t l_written = ro_Content.empty()
				? 0
				: std::fwrite(ro_Content.data(), 1, ro_Content.size(), l_file);
			std::fclose(l_file);

			return l_written == ro_Content.size();
		}

		std::vector<std::string> splitLines(const std::string& ro_Content) {
			std::vector<std::string> l_lines;
			size_t l_start = 0;

			while (l_start <= ro_Content.size()) {
				const size_t l_end = ro_Content.find('\n', l_start);
				const size_t l_stop = (l_end == std::string::npos) ? ro_Content.size() : l_end;

				std::string l_line = ro_Content.substr(l_start, l_stop - l_start);
				if (!l_line.empty() && l_line.back() == '\r') {
					l_line.pop_back();
				}
				l_lines.push_back(std::move(l_line));

				if (l_end == std::string::npos) {
					break;
				}
				l_start = l_end + 1;
			}

			return l_lines;
		}

		std::string trim(const std::string& ro_Text) {
			size_t l_begin = 0;
			size_t l_finish = ro_Text.size();
			while (l_begin < l_finish && std::isspace(static_cast<unsigned char>(ro_Text[l_begin]))) {
				++l_begin;
			}
			while (l_finish > l_begin && std::isspace(static_cast<unsigned char>(ro_Text[l_finish - 1]))) {
				--l_finish;
			}
			return ro_Text.substr(l_begin, l_finish - l_begin);
		}

		bool isIdentChar(char v_Char) {
			return std::isalnum(static_cast<unsigned char>(v_Char)) || v_Char == '_';
		}

		// Strips a trailing "# ..." comment, ignoring '#' that appears inside a
		// quoted string. Does not attempt to unescape - purely for isolating the
		// value token from trailing commentary.
		std::string stripComment(const std::string& ro_Line) {
			bool l_inString = false;
			for (size_t l_i = 0; l_i < ro_Line.size(); ++l_i) {
				const char l_char = ro_Line[l_i];
				if (l_char == '"' && (l_i == 0 || ro_Line[l_i - 1] != '\\')) {
					l_inString = !l_inString;
				} else if (l_char == '#' && !l_inString) {
					return ro_Line.substr(0, l_i);
				}
			}
			return ro_Line;
		}

		// Returns true and sets ro_OutName if ro_Line is a "[[name]]" header
		// (double-bracket). Sets ro_OutIsSingleBracket if it's a "[name]" header
		// instead (unsupported grammar - caller reports it as an error).
		bool matchHeader(const std::string& ro_Line, std::string& ro_OutName, bool& ro_OutIsSingleBracket) {
			ro_OutIsSingleBracket = false;
			if (ro_Line.size() >= 4 && ro_Line[0] == '[' && ro_Line[1] == '[' &&
			    ro_Line[ro_Line.size() - 1] == ']' && ro_Line[ro_Line.size() - 2] == ']') {
				ro_OutName = trim(ro_Line.substr(2, ro_Line.size() - 4));
				return true;
			}
			if (ro_Line.size() >= 2 && ro_Line[0] == '[' && ro_Line[ro_Line.size() - 1] == ']') {
				ro_OutIsSingleBracket = true;
				return true;
			}
			return false;
		}

		bool splitKeyValue(const std::string& ro_Line, std::string& ro_OutKey, std::string& ro_OutValueToken) {
			bool l_inString = false;
			for (size_t l_i = 0; l_i < ro_Line.size(); ++l_i) {
				const char l_char = ro_Line[l_i];
				if (l_char == '"' && (l_i == 0 || ro_Line[l_i - 1] != '\\')) {
					l_inString = !l_inString;
				} else if (l_char == '=' && !l_inString) {
					ro_OutKey = trim(ro_Line.substr(0, l_i));
					ro_OutValueToken = trim(ro_Line.substr(l_i + 1));
					return true;
				}
			}
			return false;
		}

		bool parseQuotedString(const std::string& ro_Token, std::string& ro_OutValue) {
			if (ro_Token.size() < 2 || ro_Token.front() != '"' || ro_Token.back() != '"') {
				return false;
			}

			ro_OutValue.clear();
			for (size_t l_i = 1; l_i + 1 < ro_Token.size(); ++l_i) {
				const char l_char = ro_Token[l_i];
				if (l_char != '\\') {
					ro_OutValue.push_back(l_char);
					continue;
				}

				if (l_i + 2 >= ro_Token.size()) {
					return false;
				}
				++l_i;
				switch (ro_Token[l_i]) {
					case '"':  ro_OutValue.push_back('"');  break;
					case '\\': ro_OutValue.push_back('\\'); break;
					case 'n':  ro_OutValue.push_back('\n'); break;
					case 't':  ro_OutValue.push_back('\t'); break;
					case 'r':  ro_OutValue.push_back('\r'); break;
					default:   return false;
				}
			}
			return true;
		}

		bool parseValueToken(const std::string& ro_Token, uint32_t v_LineNo,
		                      TomlValue& ro_OutValue, std::vector<TomlParseError>& ro_OutErrors) {
			if (!ro_Token.empty() && ro_Token.front() == '"') {
				std::string l_string;
				if (!parseQuotedString(ro_Token, l_string)) {
					ro_OutErrors.push_back({ v_LineNo, "malformed quoted string" });
					return false;
				}
				ro_OutValue.m_Type = TomlType::String;
				ro_OutValue.m_StringValue = std::move(l_string);
				return true;
			}

			if (ro_Token == "true" || ro_Token == "false") {
				ro_OutValue.m_Type = TomlType::Boolean;
				ro_OutValue.m_BoolValue = (ro_Token == "true");
				return true;
			}

			int64_t l_intValue = 0;
			auto l_intResult = std::from_chars(ro_Token.data(), ro_Token.data() + ro_Token.size(), l_intValue);
			if (l_intResult.ec == std::errc() && l_intResult.ptr == ro_Token.data() + ro_Token.size()) {
				ro_OutValue.m_Type = TomlType::Integer;
				ro_OutValue.m_IntValue = l_intValue;
				return true;
			}

			double l_floatValue = 0.0;
			auto l_floatResult = std::from_chars(ro_Token.data(), ro_Token.data() + ro_Token.size(), l_floatValue);
			if (l_floatResult.ec == std::errc() && l_floatResult.ptr == ro_Token.data() + ro_Token.size()) {
				ro_OutValue.m_Type = TomlType::Float;
				ro_OutValue.m_FloatValue = l_floatValue;
				return true;
			}

			ro_OutErrors.push_back({ v_LineNo, "unrecognized value: " + ro_Token });
			return false;
		}

		void formatValue(const TomlValue& ro_Value, std::string& ro_Out) {
			char l_buffer[64];

			switch (ro_Value.m_Type) {
				case TomlType::String: {
					ro_Out.push_back('"');
					for (const char l_char : ro_Value.m_StringValue) {
						switch (l_char) {
							case '"':  ro_Out += "\\\""; break;
							case '\\': ro_Out += "\\\\"; break;
							case '\n': ro_Out += "\\n";  break;
							case '\t': ro_Out += "\\t";  break;
							case '\r': ro_Out += "\\r";  break;
							default:   ro_Out.push_back(l_char);
						}
					}
					ro_Out.push_back('"');
					break;
				}
				case TomlType::Boolean:
					ro_Out += ro_Value.m_BoolValue ? "true" : "false";
					break;
				case TomlType::Integer:
					ro_Out += std::to_string(ro_Value.m_IntValue);
					break;
				case TomlType::Float: {
					auto l_result = std::to_chars(l_buffer, l_buffer + sizeof(l_buffer), ro_Value.m_FloatValue);
					ro_Out.append(l_buffer, l_result.ptr);
					break;
				}
			}
		}

		// Owns the in-progress parse state across the line loop so readToml()
		// itself stays short - one call to handleLine() per input line.
		class Parser final {
		public:
			Parser(const std::string& v_ArrayName,
			       std::vector<TomlTable>& ro_OutTables,
			       std::vector<TomlParseError>& ro_OutErrors) noexcept
				: m_arrayName(v_ArrayName)
				, m_outTables(ro_OutTables)
				, m_outErrors(ro_OutErrors) {
			}

			void handleLine(const std::string& ro_RawLine, uint32_t v_LineNo) {
				const std::string l_line = trim(stripComment(ro_RawLine));
				if (l_line.empty() || l_line.front() == '#') {
					return;
				}

				std::string l_headerName;
				bool l_singleBracket = false;
				if (matchHeader(l_line, l_headerName, l_singleBracket)) {
					handleHeader(l_headerName, l_singleBracket, v_LineNo);
					return;
				}

				handleKeyValue(l_line, v_LineNo);
			}

			void finish() {
				if (m_inTargetTable) {
					m_outTables.push_back(std::move(m_currentTable));
				}
			}

		private:
			void handleHeader(const std::string& v_Name, bool v_SingleBracket, uint32_t v_LineNo) {
				if (m_inTargetTable) {
					m_outTables.push_back(std::move(m_currentTable));
					m_currentTable = TomlTable();
					m_inTargetTable = false;
				}

				if (v_SingleBracket) {
					m_outErrors.push_back({ v_LineNo, "single-bracket tables are not supported (only [[" + m_arrayName + "]])" });
					return;
				}

				m_inTargetTable = (v_Name == m_arrayName);
			}

			void handleKeyValue(const std::string& ro_Line, uint32_t v_LineNo) {
				std::string l_key, l_valueToken;
				if (!splitKeyValue(ro_Line, l_key, l_valueToken)) {
					m_outErrors.push_back({ v_LineNo, "expected 'key = value'" });
					return;
				}

				if (l_key.empty() || !std::all_of(l_key.begin(), l_key.end(), isIdentChar)) {
					m_outErrors.push_back({ v_LineNo, "invalid key: " + l_key });
					return;
				}

				if (!m_inTargetTable) {
					return; // foreign or invalid block - already reported if applicable
				}

				for (const auto& r_pair : m_currentTable) {
					if (r_pair.first == l_key) {
						m_outErrors.push_back({ v_LineNo, "duplicate key: " + l_key });
						return;
					}
				}

				TomlValue l_value;
				if (parseValueToken(l_valueToken, v_LineNo, l_value, m_outErrors)) {
					m_currentTable.emplace_back(l_key, std::move(l_value));
				}
			}

			const std::string& m_arrayName;
			std::vector<TomlTable>& m_outTables;
			std::vector<TomlParseError>& m_outErrors;
			TomlTable m_currentTable;
			bool m_inTargetTable = false;
		};

	} // namespace

	bool readToml(const std::string& v_Path, const std::string& v_ArrayName,
	              std::vector<TomlTable>& ro_OutTables, std::vector<TomlParseError>& ro_OutErrors) {
		ro_OutTables.clear();
		ro_OutErrors.clear();

		std::string l_content;
		if (!readWholeFile(v_Path, l_content)) {
			ro_OutErrors.push_back({ 0, "could not open file: " + v_Path });
			return false;
		}

		Parser l_parser(v_ArrayName, ro_OutTables, ro_OutErrors);
		const std::vector<std::string> l_lines = splitLines(l_content);
		for (size_t l_i = 0; l_i < l_lines.size(); ++l_i) {
			l_parser.handleLine(l_lines[l_i], static_cast<uint32_t>(l_i + 1));
		}
		l_parser.finish();

		return ro_OutErrors.empty();
	}

	bool writeToml(const std::string& v_Path, const std::string& v_ArrayName,
	               const std::vector<TomlTable>& ro_Tables, bool v_Append) {
		std::string l_content;

		for (const TomlTable& r_table : ro_Tables) {
			l_content += "[[" + v_ArrayName + "]]\n";
			for (const auto& r_pair : r_table) {
				l_content += r_pair.first;
				l_content += " = ";
				formatValue(r_pair.second, l_content);
				l_content += '\n';
			}
			l_content += '\n';
		}

		return writeWholeFile(v_Path, l_content, v_Append);
	}

	const std::string* findTomlString(const TomlTable& ro_Table, const std::string& v_Key) {
		for (const auto& r_pair : ro_Table) {
			if (r_pair.first == v_Key && r_pair.second.m_Type == TomlType::String) {
				return &r_pair.second.m_StringValue;
			}
		}
		return nullptr;
	}

	const int64_t* findTomlInt(const TomlTable& ro_Table, const std::string& v_Key) {
		for (const auto& r_pair : ro_Table) {
			if (r_pair.first == v_Key && r_pair.second.m_Type == TomlType::Integer) {
				return &r_pair.second.m_IntValue;
			}
		}
		return nullptr;
	}

	const double* findTomlFloat(const TomlTable& ro_Table, const std::string& v_Key) {
		for (const auto& r_pair : ro_Table) {
			if (r_pair.first == v_Key && r_pair.second.m_Type == TomlType::Float) {
				return &r_pair.second.m_FloatValue;
			}
		}
		return nullptr;
	}

	const bool* findTomlBool(const TomlTable& ro_Table, const std::string& v_Key) {
		for (const auto& r_pair : ro_Table) {
			if (r_pair.first == v_Key && r_pair.second.m_Type == TomlType::Boolean) {
				return &r_pair.second.m_BoolValue;
			}
		}
		return nullptr;
	}

} // namespace Hades::Runtime
