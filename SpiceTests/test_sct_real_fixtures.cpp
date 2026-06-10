#include "../SpiceSCT/SpiceSCT.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace {

std::filesystem::path findMe017bFixture()
{
    auto cursor = std::filesystem::current_path();
    for (int i = 0; i < 6; ++i) {
        const auto candidate = cursor / "soa_parser_reference_bundle" / "sct_context" / "sct_input" / "me017b.sct";
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (!cursor.has_parent_path() || cursor == cursor.parent_path()) {
            break;
        }
        cursor = cursor.parent_path();
    }
    return {};
}

std::size_t instructionCount(const spice::sct::SctParseResult& parseResult)
{
    std::size_t count = 0;
    for (const auto& section : parseResult.file.sections) {
        count += section.instructions.size();
    }
    return count;
}

std::size_t expressionCount(const spice::sct::SctParseResult& parseResult)
{
    std::size_t count = 0;
    for (const auto& section : parseResult.file.sections) {
        for (const auto& instruction : section.instructions) {
            for (const auto& parameter : instruction.parameters) {
                if (parameter.expression.has_value()) {
                    ++count;
                }
            }
        }
    }
    return count;
}

std::size_t astCount(const spice::sct::SctParseResult& parseResult)
{
    std::size_t count = 0;
    for (const auto& section : parseResult.file.sections) {
        for (const auto& instruction : section.instructions) {
            for (const auto& parameter : instruction.parameters) {
                if (parameter.expression.has_value() && parameter.expression->ast.has_value()) {
                    ++count;
                }
            }
        }
    }
    return count;
}

bool hasAstKind(const spice::sct::SctScptAstNode& node, spice::sct::SctScptAstNodeKind kind)
{
    if (node.kind == kind) {
        return true;
    }
    for (const auto& child : node.children) {
        if (hasAstKind(child, kind)) {
            return true;
        }
    }
    return false;
}

bool hasAstKind(const spice::sct::SctParseResult& parseResult, spice::sct::SctScptAstNodeKind kind)
{
    for (const auto& section : parseResult.file.sections) {
        for (const auto& instruction : section.instructions) {
            for (const auto& parameter : instruction.parameters) {
                if (parameter.expression.has_value()
                    && parameter.expression->ast.has_value()
                    && hasAstKind(*parameter.expression->ast, kind)) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

TEST(SctRealFixtures, Me017bParsesAndBuildsScptAst)
{
    const auto fixture = findMe017bFixture();
    if (fixture.empty()) {
        GTEST_SKIP() << "me017b.sct real fixture is not present in soa_parser_reference_bundle.";
    }

    const auto parsed = spice::sct::SctParser{}.parseFile(fixture.string());

    ASSERT_TRUE(parsed.parseOk);
    EXPECT_GT(parsed.file.sections.size(), 100u);
    EXPECT_GT(instructionCount(parsed), 100u);
    EXPECT_GT(expressionCount(parsed), 100u);
    EXPECT_GT(astCount(parsed), 100u);
    EXPECT_TRUE(hasAstKind(parsed, spice::sct::SctScptAstNodeKind::FloatLiteral));
    EXPECT_TRUE(hasAstKind(parsed, spice::sct::SctScptAstNodeKind::IntVariable));
    EXPECT_TRUE(hasAstKind(parsed, spice::sct::SctScptAstNodeKind::BitVariable));
    EXPECT_TRUE(hasAstKind(parsed, spice::sct::SctScptAstNodeKind::CompareOp));
}

TEST(SctRealFixtures, Me017bPreserveModeIsByteIdentical)
{
    const auto fixture = findMe017bFixture();
    if (fixture.empty()) {
        GTEST_SKIP() << "me017b.sct real fixture is not present in soa_parser_reference_bundle.";
    }

    const auto parsed = spice::sct::SctParser{}.parseFile(fixture.string());
    ASSERT_TRUE(parsed.parseOk);

    spice::sct::SctExportOptions options{};
    options.mode = spice::sct::SctExportMode::PreserveBytesForTest;
    const auto exported = spice::sct::SctBinaryExporter{}.exportFile(parsed, options);

    EXPECT_EQ(parsed.file.originalBytes, exported);
}
