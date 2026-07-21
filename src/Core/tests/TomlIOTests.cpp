// Standalone unit tests for TomlIO.h / Configuration.h's suite.toml support.
// Not a Hades fixture/benchmark - this exercises Hades' own infrastructure,
// so it deliberately does not use HadesEngine/ValidationFixture (TEST_FORMAT.md's
// harness is for testing *other* libraries' fixtures via Hades, not for
// testing Hades' own reader/writer code before HadesEngine v3 even exists).
//
// Minimal local PASS/FAIL framework: EXPECT_TRUE logs and counts failures but
// keeps running so one bad assumption doesn't hide the rest of the report.

#include <Configuration.h>
#include <TomlIO.h>

#include <cstdio>
#include <string>

using namespace Hades::Runtime;

namespace {

	int g_failures = 0;

	void expectTrueImpl(bool v_Condition, const char* v_Expr, const char* v_File, int v_Line) {
		if (!v_Condition) {
			std::printf("  FAIL %s:%d: %s\n", v_File, v_Line, v_Expr);
			++g_failures;
		}
	}

#define EXPECT_TRUE(expr) expectTrueImpl((expr), #expr, __FILE__, __LINE__)
#define EXPECT_EQ(a, b)   expectTrueImpl((a) == (b), #a " == " #b, __FILE__, __LINE__)

	bool writeFile(const std::string& v_Path, const std::string& v_Content) {
		std::FILE* l_file = std::fopen(v_Path.c_str(), "wb");
		if (l_file == nullptr) {
			return false;
		}
		const size_t l_written = v_Content.empty() ? 0 : std::fwrite(v_Content.data(), 1, v_Content.size(), l_file);
		std::fclose(l_file);
		return l_written == v_Content.size();
	}

	void testRoundTrip() {
		std::printf("testRoundTrip\n");
		const std::string l_sample =
			"[[test]]\n"
			"id = \"rwlock_hotspot_25w_30read\"\n"
			"fixture = \"RwLockContentionFixture\"\n"
			"kind = \"performance\"\n"
			"warmup_count = 10\n"
			"cv_threshold = 0.02\n"
			"helper_thread_count = 25\n"
			"read_criticality = 0.3\n"
			"fixed_slice_count = 200\n"
			"\n"
			"# a comment, then a defaults-only correctness entry\n"
			"[[test]]\n"
			"id = \"simple_correctness\"\n"
			"fixture = \"SomeOtherFixture\"\n"
			"kind = \"correctness\"\n";
		EXPECT_TRUE(writeFile("t_roundtrip_in.toml", l_sample));

		std::vector<SuiteTestEntry> l_entries;
		std::vector<TomlParseError> l_errors;
		EXPECT_TRUE(readSuiteToml("t_roundtrip_in.toml", l_entries, l_errors));
		EXPECT_EQ(l_errors.size(), 0u);
		EXPECT_EQ(l_entries.size(), 2u);

		if (l_entries.size() == 2) {
			EXPECT_EQ(l_entries[0].m_Config.m_Id, "rwlock_hotspot_25w_30read");
			EXPECT_EQ(l_entries[0].m_FixtureName, "RwLockContentionFixture");
			EXPECT_TRUE(l_entries[0].m_Config.m_Kind == TestKind::Performance);
			EXPECT_EQ(l_entries[0].m_Config.m_WarmupCount, 10u);
			EXPECT_EQ(l_entries[0].m_Config.m_CvThreshold, 0.02);
			EXPECT_EQ(l_entries[0].m_Config.m_HelperThreadCount, 25u);
			EXPECT_EQ(l_entries[0].m_Config.m_ReadCriticality, 0.3);
			EXPECT_EQ(l_entries[0].m_Config.m_FixedSliceCount, 200u);

			EXPECT_EQ(l_entries[1].m_Config.m_Id, "simple_correctness");
			EXPECT_TRUE(l_entries[1].m_Config.m_Kind == TestKind::Correctness);
			EXPECT_EQ(l_entries[1].m_Config.m_WarmupCount, 0u);
		}

		EXPECT_TRUE(writeSuiteToml("t_roundtrip_out.toml", l_entries, false));

		std::vector<SuiteTestEntry> l_entries2;
		std::vector<TomlParseError> l_errors2;
		EXPECT_TRUE(readSuiteToml("t_roundtrip_out.toml", l_entries2, l_errors2));
		EXPECT_EQ(l_errors2.size(), 0u);
		EXPECT_EQ(l_entries2.size(), l_entries.size());
		for (size_t l_i = 0; l_i < l_entries.size() && l_i < l_entries2.size(); ++l_i) {
			EXPECT_EQ(l_entries[l_i].m_Config.m_Id, l_entries2[l_i].m_Config.m_Id);
			EXPECT_EQ(l_entries[l_i].m_FixtureName, l_entries2[l_i].m_FixtureName);
			EXPECT_TRUE(l_entries[l_i].m_Config.m_Kind == l_entries2[l_i].m_Config.m_Kind);
			EXPECT_EQ(l_entries[l_i].m_Config.m_WarmupCount, l_entries2[l_i].m_Config.m_WarmupCount);
			EXPECT_EQ(l_entries[l_i].m_Config.m_HelperThreadCount, l_entries2[l_i].m_Config.m_HelperThreadCount);
			EXPECT_EQ(l_entries[l_i].m_Config.m_FixedSliceCount, l_entries2[l_i].m_Config.m_FixedSliceCount);
		}
	}

	void testRejectsSingleBracketTable() {
		std::printf("testRejectsSingleBracketTable\n");
		EXPECT_TRUE(writeFile("t_singlebracket.toml", "[oops]\nz = 1\n"));

		std::vector<TomlTable> l_tables;
		std::vector<TomlParseError> l_errors;
		EXPECT_TRUE(!readToml("t_singlebracket.toml", "test", l_tables, l_errors));
		EXPECT_EQ(l_errors.size(), 1u);
	}

	void testInvalidKindRejected() {
		std::printf("testInvalidKindRejected\n");
		EXPECT_TRUE(writeFile("t_badkind.toml",
			"[[test]]\nid = \"x\"\nfixture = \"F\"\nkind = \"weird\"\n"));

		std::vector<SuiteTestEntry> l_entries;
		std::vector<TomlParseError> l_errors;
		EXPECT_TRUE(!readSuiteToml("t_badkind.toml", l_entries, l_errors));
		EXPECT_TRUE(!l_errors.empty());
		EXPECT_EQ(l_entries.size(), 0u);
	}

	void testMissingRequiredKeysRejected() {
		std::printf("testMissingRequiredKeysRejected\n");
		EXPECT_TRUE(writeFile("t_missing.toml", "[[test]]\nwarmup_count = 5\n"));

		std::vector<SuiteTestEntry> l_entries;
		std::vector<TomlParseError> l_errors;
		EXPECT_TRUE(!readSuiteToml("t_missing.toml", l_entries, l_errors));
		EXPECT_EQ(l_errors.size(), 2u); // missing id + missing fixture
		EXPECT_EQ(l_entries.size(), 0u);
	}

	void testUnknownKeyReported() {
		std::printf("testUnknownKeyReported\n");
		EXPECT_TRUE(writeFile("t_unknown.toml",
			"[[test]]\nid = \"x\"\nfixture = \"F\"\nbogus_key = 1\n"));

		std::vector<SuiteTestEntry> l_entries;
		std::vector<TomlParseError> l_errors;
		EXPECT_TRUE(!readSuiteToml("t_unknown.toml", l_entries, l_errors));
		EXPECT_EQ(l_errors.size(), 1u);
		EXPECT_EQ(l_entries.size(), 1u); // entry still parsed - unknown key is reported, not fatal to the rest
	}

	void testDuplicateKeyRejected() {
		std::printf("testDuplicateKeyRejected\n");
		EXPECT_TRUE(writeFile("t_dupe.toml",
			"[[test]]\nid = \"x\"\nid = \"y\"\nfixture = \"F\"\n"));

		std::vector<TomlTable> l_tables;
		std::vector<TomlParseError> l_errors;
		EXPECT_TRUE(!readToml("t_dupe.toml", "test", l_tables, l_errors));
		EXPECT_TRUE(!l_errors.empty());
	}

	void testStringEscaping() {
		std::printf("testStringEscaping\n");
		EXPECT_TRUE(writeFile("t_escape.toml",
			"[[test]]\nid = \"has \\\"quotes\\\" and \\\\backslash\\\\\"\nfixture = \"F\"\n"));

		std::vector<SuiteTestEntry> l_entries;
		std::vector<TomlParseError> l_errors;
		EXPECT_TRUE(readSuiteToml("t_escape.toml", l_entries, l_errors));
		if (l_entries.size() == 1) {
			EXPECT_EQ(l_entries[0].m_Config.m_Id, "has \"quotes\" and \\backslash\\");
		} else {
			EXPECT_TRUE(false);
		}
	}

	void testNegativeNumbersAndComments() {
		std::printf("testNegativeNumbersAndComments\n");
		EXPECT_TRUE(writeFile("t_negcomment.toml",
			"# a leading full-line comment\n"
			"[[test]]\n"
			"id = \"neg\" # trailing comment after string\n"
			"fixture = \"F\"\n"
			"cv_threshold = -0.5\n"));

		std::vector<SuiteTestEntry> l_entries;
		std::vector<TomlParseError> l_errors;
		EXPECT_TRUE(readSuiteToml("t_negcomment.toml", l_entries, l_errors));
		if (l_entries.size() == 1) {
			EXPECT_EQ(l_entries[0].m_Config.m_Id, "neg");
			EXPECT_EQ(l_entries[0].m_Config.m_CvThreshold, -0.5);
		} else {
			EXPECT_TRUE(false);
		}
	}

	void testMissingFileReported() {
		std::printf("testMissingFileReported\n");
		std::vector<SuiteTestEntry> l_entries;
		std::vector<TomlParseError> l_errors;
		EXPECT_TRUE(!readSuiteToml("t_does_not_exist.toml", l_entries, l_errors));
		EXPECT_EQ(l_errors.size(), 1u);
	}

	void testAppendWriteDoesNotTouchExisting() {
		std::printf("testAppendWriteDoesNotTouchExisting\n");
		EXPECT_TRUE(writeFile("t_append.toml", "[[test]]\nid = \"first\"\nfixture = \"F1\"\n\n"));

		SuiteTestEntry l_second;
		setId(l_second.m_Config, "second");
		l_second.m_FixtureName = "F2";
		EXPECT_TRUE(writeSuiteToml("t_append.toml", { l_second }, true));

		std::vector<SuiteTestEntry> l_entries;
		std::vector<TomlParseError> l_errors;
		EXPECT_TRUE(readSuiteToml("t_append.toml", l_entries, l_errors));
		EXPECT_EQ(l_entries.size(), 2u);
		if (l_entries.size() == 2) {
			EXPECT_EQ(l_entries[0].m_Config.m_Id, "first");
			EXPECT_EQ(l_entries[1].m_Config.m_Id, "second");
		}
	}

} // namespace

int main() {
	testRoundTrip();
	testRejectsSingleBracketTable();
	testInvalidKindRejected();
	testMissingRequiredKeysRejected();
	testUnknownKeyReported();
	testDuplicateKeyRejected();
	testStringEscaping();
	testNegativeNumbersAndComments();
	testMissingFileReported();
	testAppendWriteDoesNotTouchExisting();

	if (g_failures == 0) {
		std::printf("ALL PASSED\n");
		return 0;
	}
	std::printf("%d FAILURE(S)\n", g_failures);
	return 1;
}
