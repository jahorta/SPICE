#include "../SpiceSCT/SpiceSCT.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <algorithm>
#include <string>
#include <vector>

namespace {

std::filesystem::path findSctFixture(const std::string& fileName)
{
    auto cursor = std::filesystem::current_path();
    for (int i = 0; i < 6; ++i) {
        // Game files remain local-only under this repository's ignored reference bundle.
        const auto candidate = cursor / "soa_parser_reference_bundle" / "sct_context" / "sct_input" / fileName;
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
    const auto fixture = findSctFixture("me017b.sct");
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
    const auto fixture = findSctFixture("me017b.sct");
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

TEST(SctRealFixtures, Me017bImportsDeterministicCanonicalDocumentWithCompleteCoverage)
{
    const auto fixture = findSctFixture("me017b.sct");
    if (fixture.empty()) {
        GTEST_SKIP() << "me017b.sct real fixture is not present in soa_parser_reference_bundle.";
    }

    const auto parsed = spice::sct::SctParser{}.parseFile(fixture.string());
    ASSERT_TRUE(parsed.parseOk);
    const auto first = spice::sct::SctDocumentImporter::import(parsed);
    const auto second = spice::sct::SctDocumentImporter::import(parsed);
    ASSERT_TRUE(first.document.has_value());
    ASSERT_TRUE(second.document.has_value());
    EXPECT_EQ(first.document->sections.size(), second.document->sections.size());
    EXPECT_EQ(first.document->strings.size(), second.document->strings.size());
    EXPECT_EQ(first.document->footerEntries.size(), second.document->footerEntries.size());
    EXPECT_EQ(first.document->opaqueAttachments.size(), second.document->opaqueAttachments.size());
    ASSERT_FALSE(first.document->sections.empty());
    EXPECT_EQ(first.document->sections.front().id, second.document->sections.front().id);

    std::size_t typedExpressions = 0;
    for (const auto& section : first.document->sections) {
        const auto* script = std::get_if<spice::sct::SctScriptSectionContent>(&section.content);
        if (!script) continue;
        for (const auto& instruction : script->instructions) {
            const auto count = [&](const auto& parameters) {
                for (const auto& parameter : parameters) {
                    const auto* expression = std::get_if<spice::sct::SctCanonicalExpression>(&parameter.value);
                    if (expression && std::holds_alternative<spice::sct::SctCanonicalExpressionNode>(expression->root)) {
                        ++typedExpressions;
                    }
                }
            };
            count(instruction.fixedParameters);
            for (const auto& group : instruction.repeatedParameterGroups) count(group.parameters);
        }
    }
    EXPECT_GT(typedExpressions, 100u);

    std::vector<unsigned> coverage(parsed.file.originalPayloadBytes.size(), 0);
    for (const auto& provenance : first.provenance) {
        ASSERT_LE(static_cast<std::size_t>(provenance.decodedPayloadOffset) + provenance.byteSize, coverage.size());
        for (std::size_t i = provenance.decodedPayloadOffset;
             i < provenance.decodedPayloadOffset + provenance.byteSize; ++i) ++coverage[i];
    }
    EXPECT_TRUE(std::all_of(coverage.begin(), coverage.end(), [](unsigned claims) { return claims == 1; }));
    EXPECT_TRUE(std::none_of(first.document->opaqueAttachments.begin(), first.document->opaqueAttachments.end(),
        [&](const auto& attachment) {
            return attachment.fixedOffset == 0 && attachment.bytes.size() == parsed.file.originalPayloadBytes.size();
        }));
    const auto validation = spice::sct::SctDocumentValidator::validate(
        *first.document, spice::sct::SctPlatform::GameCube);
    EXPECT_TRUE(validation.validForLayout);
}

TEST(SctRealFixtures, Me004aCarriesGameCubeOnlyOpcode265)
{
    const auto fixture = findSctFixture("me004a.sct");
    if (fixture.empty()) {
        GTEST_SKIP() << "me004a.sct real fixture is not present in the ignored soa_parser_reference_bundle.";
    }

    const auto parsed = spice::sct::SctParser{}.parseFile(fixture.string());
    ASSERT_TRUE(parsed.parseOk);
    const auto imported = spice::sct::SctDocumentImporter::import(parsed);
    ASSERT_TRUE(imported.document.has_value());

    std::size_t opcode265Count = 0;
    for (const auto& section : imported.document->sections) {
        const auto* script = std::get_if<spice::sct::SctScriptSectionContent>(&section.content);
        if (!script) continue;
        opcode265Count += std::count_if(script->instructions.begin(), script->instructions.end(),
            [](const auto& instruction) { return instruction.opcode == 265; });
    }
    ASSERT_GT(opcode265Count, 0u);

    const auto gameCube = spice::sct::SctDocumentValidator::validate(
        *imported.document, spice::sct::SctPlatform::GameCube);
    const auto dreamcast = spice::sct::SctDocumentValidator::validate(
        *imported.document, spice::sct::SctPlatform::Dreamcast);
    EXPECT_TRUE(gameCube.validForLayout);
    EXPECT_FALSE(dreamcast.validForLayout);
    EXPECT_TRUE(std::any_of(dreamcast.diagnostics.begin(), dreamcast.diagnostics.end(),
        [](const auto& diagnostic) {
            return diagnostic.code == spice::sct::SctDiagnosticCode::OpcodeUnavailable;
        }));
}
