#include "../SpiceTrade/SpiceTrade.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

using spice::trade::alx::CsvDocument;
using spice::trade::alx::CsvFormat;
using spice::trade::alx::CsvLineEnding;
using spice::trade::alx::CsvReader;
using spice::trade::alx::CsvWriter;
using spice::trade::alx::DiagnosticSeverity;
using spice::trade::alx::TrackedDocument;
using spice::trade::alx::Workspace;
using spice::trade::alx::WorkspaceReader;
using spice::trade::alx::WorkspaceWriter;

std::vector<std::uint8_t> bytes(const std::string& value)
{
    return std::vector<std::uint8_t>(value.begin(), value.end());
}

void writeBytes(const std::filesystem::path& path, const std::string& value)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path()
            / ("spice_trade_tests_" + std::to_string(nonce));
        std::filesystem::create_directories(path);
    }

    ~TemporaryDirectory()
    {
        std::error_code error{};
        std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path{};
};

std::filesystem::path referenceCorpusRoot()
{
    auto cursor = std::filesystem::current_path();
    for (std::size_t depth = 0U; depth < 8U; ++depth) {
        const auto candidate = cursor / "SpiceTrade" / "Alx v5.0.0 corpuses";
        if (std::filesystem::is_directory(candidate)) {
            return candidate;
        }
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) {
            break;
        }
        cursor = cursor.parent_path();
    }
    return {};
}

bool hasWarning(const std::vector<spice::trade::alx::CsvDiagnostic>& diagnostics)
{
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.severity == DiagnosticSeverity::Warning) {
            return true;
        }
    }
    return false;
}

TEST(SpiceTradeCsv, ParsesAndCanonicallyWritesComplexUtf8Csv)
{
    std::string input = "Name,Note,Empty\r\n";
    input += "Alpha,\"comma, quote \"\" and\r\nline\",\r\n";
    input += "\xe8\x88\xb9,plain,last\r\n";

    const auto parsed = CsvReader{}.parse(bytes(input));
    ASSERT_TRUE(parsed.ok());
    ASSERT_TRUE(parsed.document.has_value());
    EXPECT_FALSE(parsed.format.utf8Bom);
    EXPECT_EQ(parsed.format.lineEnding, CsvLineEnding::CrLf);
    EXPECT_TRUE(parsed.format.finalLineEnding);
    EXPECT_EQ(parsed.document->columnIndex("Note"), 1U);
    EXPECT_FALSE(parsed.document->columnIndex("Missing").has_value());
    ASSERT_EQ(parsed.document->rows.size(), 2U);
    EXPECT_EQ(parsed.document->rows[0][1], "comma, quote \" and\r\nline");
    EXPECT_TRUE(parsed.document->rows[0][2].empty());
    EXPECT_EQ(parsed.document->rows[1][0], "\xe8\x88\xb9");

    const auto written = CsvWriter{}.write(*parsed.document, parsed.format);
    ASSERT_TRUE(written.ok());
    const auto reparsed = CsvReader{}.parse(written.bytes);
    ASSERT_TRUE(reparsed.ok());
    EXPECT_EQ(reparsed.document, parsed.document);
    EXPECT_EQ(reparsed.format, parsed.format);
}

TEST(SpiceTradeCsv, PreservesBomLfAndMissingFinalLineEnding)
{
    const std::vector<std::uint8_t> input{
        0xefU, 0xbbU, 0xbfU,
        'A', ',', 'B', '\n',
        '1', ',', '2',
    };
    const auto parsed = CsvReader{}.parse(input);
    ASSERT_TRUE(parsed.ok());
    EXPECT_TRUE(parsed.format.utf8Bom);
    EXPECT_EQ(parsed.format.lineEnding, CsvLineEnding::Lf);
    EXPECT_FALSE(parsed.format.finalLineEnding);

    const auto written = CsvWriter{}.write(*parsed.document, parsed.format);
    ASSERT_TRUE(written.ok());
    EXPECT_EQ(written.bytes, input);
}

TEST(SpiceTradeCsv, WarnsOnMixedRecordLineEndingsAndUsesFirstStyle)
{
    const auto parsed = CsvReader{}.parse(bytes("A\r\n1\n2\r\n"));
    ASSERT_TRUE(parsed.ok());
    EXPECT_TRUE(hasWarning(parsed.diagnostics));
    EXPECT_EQ(parsed.format.lineEnding, CsvLineEnding::CrLf);

    const auto written = CsvWriter{}.write(*parsed.document, parsed.format);
    ASSERT_TRUE(written.ok());
    EXPECT_EQ(std::string(written.bytes.begin(), written.bytes.end()), "A\r\n1\r\n2\r\n");
}

TEST(SpiceTradeCsv, RejectsMalformedAndStructurallyInvalidDocuments)
{
    EXPECT_FALSE(CsvReader{}.parse({}).ok());
    EXPECT_FALSE(CsvReader{}.parse(bytes("A,A\r\n1,2\r\n")).ok());
    EXPECT_FALSE(CsvReader{}.parse(bytes("A,B\r\n1\r\n")).ok());
    EXPECT_FALSE(CsvReader{}.parse(bytes("A,B\r\n1,ab\"cd\r\n")).ok());
    EXPECT_FALSE(CsvReader{}.parse(bytes("A,B\r\n1,\"unterminated")).ok());

    const std::vector<std::uint8_t> invalidUtf8{ 'A', '\r', '\n', 0xc0U, 0xafU, '\r', '\n' };
    EXPECT_FALSE(CsvReader{}.parse(invalidUtf8).ok());

    CsvDocument invalidWidth{
        .headers = { "A", "B" },
        .rows = { { "1" } },
    };
    EXPECT_FALSE(CsvWriter{}.write(invalidWidth).ok());
}

TEST(SpiceTradeWorkspace, TracksStructuralChangesByBaselineComparison)
{
    const CsvDocument baseline{
        .headers = { "A", "B" },
        .rows = { { "1", "2" }, { "3", "4" } },
    };
    TrackedDocument document{
        .relativePath = "sample.csv",
        .baseline = baseline,
        .current = baseline,
    };
    EXPECT_FALSE(document.changed());

    document.current.headers[0] = "Changed";
    EXPECT_TRUE(document.changed());
    document.current = baseline;
    document.current.rows.push_back({ "5", "6" });
    EXPECT_TRUE(document.changed());
    document.current = baseline;
    std::swap(document.current.rows[0], document.current.rows[1]);
    EXPECT_TRUE(document.changed());
}

TEST(SpiceTradeWorkspace, WritesOnlyChangedFilesAndMarksThemClean)
{
    TemporaryDirectory temp{};
    const auto source = temp.path / "source";
    const auto output = temp.path / "output";
    writeBytes(source / "alpha.csv", "ID,Name\r\n1,Alpha\r\n");
    writeBytes(source / "nested" / "beta.csv", "ID,Name\r\n2,Beta\r\n");

    const std::array<std::filesystem::path, 2U> requested{
        "alpha.csv",
        std::filesystem::path("nested") / "beta.csv",
    };
    auto read = WorkspaceReader{}.read(source, requested);
    ASSERT_TRUE(read.ok());
    ASSERT_TRUE(read.workspace.has_value());
    EXPECT_TRUE(read.workspace->changedPaths().empty());

    auto* alpha = read.workspace->find("ALPHA.CSV");
    ASSERT_NE(alpha, nullptr);
    alpha->current.rows[0][1] = "Changed, Alpha";
    EXPECT_EQ(read.workspace->changedPaths(), std::vector<std::filesystem::path>{ "alpha.csv" });

    const auto written = WorkspaceWriter{}.writeChanged(*read.workspace, output);
    ASSERT_TRUE(written.ok());
    EXPECT_EQ(written.writtenPaths, std::vector<std::filesystem::path>{ "alpha.csv" });
    EXPECT_TRUE(std::filesystem::exists(output / "alpha.csv"));
    EXPECT_FALSE(std::filesystem::exists(output / "nested" / "beta.csv"));
    EXPECT_NE(readText(output / "alpha.csv").find("\"Changed, Alpha\""), std::string::npos);
    EXPECT_TRUE(read.workspace->changedPaths().empty());

    const auto secondWrite = WorkspaceWriter{}.writeChanged(*read.workspace, output);
    EXPECT_TRUE(secondWrite.ok());
    EXPECT_TRUE(secondWrite.writtenPaths.empty());
}

TEST(SpiceTradeWorkspace, FailsWholeImportForInvalidMissingOrUnsafeRequests)
{
    TemporaryDirectory temp{};
    writeBytes(temp.path / "valid.csv", "A,B\r\n1,2\r\n");
    writeBytes(temp.path / "invalid.csv", "A,B\r\n1\r\n");

    const std::array<std::filesystem::path, 3U> requested{
        "valid.csv",
        "invalid.csv",
        "missing.csv",
    };
    const auto invalidBatch = WorkspaceReader{}.read(temp.path, requested);
    EXPECT_FALSE(invalidBatch.ok());
    EXPECT_FALSE(invalidBatch.workspace.has_value());

    const std::array<std::filesystem::path, 1U> unsafe{ std::filesystem::path("..") / "escape.csv" };
    const auto unsafeBatch = WorkspaceReader{}.read(temp.path, unsafe);
    EXPECT_FALSE(unsafeBatch.ok());
    EXPECT_FALSE(unsafeBatch.workspace.has_value());
}

TEST(SpiceTradeWorkspace, LeavesDocumentDirtyWhenOutputFails)
{
    TemporaryDirectory temp{};
    const auto blockedRoot = temp.path / "not_a_directory";
    writeBytes(blockedRoot, "blocking file");

    Workspace workspace{};
    workspace.documents.push_back(TrackedDocument{
        .relativePath = "dirty.csv",
        .baseline = CsvDocument{ .headers = { "A" }, .rows = { { "1" } } },
        .current = CsvDocument{ .headers = { "A" }, .rows = { { "2" } } },
        .format = CsvFormat{},
    });

    const auto write = WorkspaceWriter{}.writeChanged(workspace, blockedRoot);
    EXPECT_FALSE(write.ok());
    EXPECT_TRUE(write.writtenPaths.empty());
    EXPECT_TRUE(workspace.documents.front().changed());
}

TEST(SpiceTradeAlx500CodecCorpus, RepoReferenceCsvFilesParseAndSemanticallyRoundTrip)
{
    const auto dataRoot = referenceCorpusRoot();
    ASSERT_FALSE(dataRoot.empty()) << "Repo-local ALX 5.0.0 reference corpus is unavailable";

    std::vector<std::filesystem::path> csvFiles{};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dataRoot)) {
        if (entry.is_regular_file() && entry.path().extension() == ".csv") {
            csvFiles.push_back(entry.path());
        }
    }
    std::sort(csvFiles.begin(), csvFiles.end());

    ASSERT_EQ(csvFiles.size(), 282U);
    for (const auto& csvFile : csvFiles) {
        const auto parsed = CsvReader{}.readFile(csvFile);
        ASSERT_TRUE(parsed.ok()) << csvFile.string();

        const auto written = CsvWriter{}.write(*parsed.document, parsed.format);
        ASSERT_TRUE(written.ok()) << csvFile.string();

        const auto reparsed = CsvReader{}.parse(written.bytes, csvFile.filename());
        ASSERT_TRUE(reparsed.ok()) << csvFile.string();
        EXPECT_EQ(reparsed.document, parsed.document) << csvFile.string();
        EXPECT_EQ(reparsed.format, parsed.format) << csvFile.string();
    }
}

} // namespace
