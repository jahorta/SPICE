#include "../SpiceSCT/SpiceSCT.h"
#include "../Compression/Aklz.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <algorithm>
#include <ranges>
#include <string>
#include <utility>
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

std::size_t programCount(const spice::sct::SctParseResult& parseResult)
{
    std::size_t count = 0;
    for (const auto& section : parseResult.file.sections) {
        for (const auto& instruction : section.instructions) {
            for (const auto& parameter : instruction.parameters) {
                if (parameter.expression.has_value() && parameter.expression->program.has_value()) {
                    ++count;
                }
            }
        }
    }
    return count;
}

bool hasValueKind(const spice::sct::SctParseResult& parseResult, spice::sct::SctScptValueKind kind)
{
    for (const auto& section : parseResult.file.sections) {
        for (const auto& instruction : section.instructions) {
            for (const auto& parameter : instruction.parameters) {
                if (parameter.expression && parameter.expression->program) {
                    for (const auto& operation : parameter.expression->program->operations) {
                        const auto* value = std::get_if<spice::sct::SctScptValueOperation>(&operation);
                        if (value && value->kind == kind) return true;
                    }
                }
            }
        }
    }
    return false;
}

std::size_t documentInstructionCount(const spice::sct::SctDocument& document)
{
    std::size_t count = 0;
    for (const auto& section : document.sections) {
        if (const auto* script = std::get_if<spice::sct::SctScriptSectionContent>(&section.content)) {
            count += script->instructions.size();
        }
    }
    return count;
}

std::size_t documentStringCount(const spice::sct::SctDocument& document)
{
    return static_cast<std::size_t>(std::count_if(document.sections.begin(), document.sections.end(),
        [](const auto& section) {
            return std::holds_alternative<spice::sct::SctStringSectionContent>(section.content);
        }));
}

std::pair<std::size_t, std::size_t> opcode265ReferenceCounts(
    const spice::sct::SctDocument& document)
{
    std::size_t opcodeCount = 0;
    std::size_t typedReferenceCount = 0;
    for (const auto& section : document.sections) {
        const auto* script = std::get_if<spice::sct::SctScriptSectionContent>(&section.content);
        if (!script) continue;
        for (const auto& instruction : script->instructions) {
            if (instruction.opcode != 265u) continue;
            ++opcodeCount;
            const auto parameter = std::find_if(instruction.fixedParameters.begin(),
                instruction.fixedParameters.end(), [](const auto& value) {
                    return value.schemaIndex == 1u;
                });
            if (parameter != instruction.fixedParameters.end()
                && std::holds_alternative<spice::sct::SctStringReference>(parameter->value)) {
                ++typedReferenceCount;
            }
        }
    }
    return {opcodeCount, typedReferenceCount};
}

std::string documentDiagnosticMessages(const std::vector<spice::sct::SctDocumentDiagnostic>& diagnostics)
{
    std::string messages;
    for (const auto& diagnostic : diagnostics) messages += diagnostic.message + "\n";
    return messages;
}

} // namespace

TEST(SctRealFixtures, Me017bParsesAndBuildsScptPrograms)
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
    EXPECT_GT(programCount(parsed), 100u);
    EXPECT_TRUE(hasValueKind(parsed, spice::sct::SctScptValueKind::FloatLiteral));
    EXPECT_TRUE(hasValueKind(parsed, spice::sct::SctScptValueKind::DirectIntVariable));
    EXPECT_TRUE(hasValueKind(parsed, spice::sct::SctScptValueKind::BitVariable));
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
    const auto first = spice::sct::SctDocumentImporter::import(parsed,
        {{spice::sct::SctPlatform::GameCube}, spice::sct::kSctWindows1252Byte7FEncoding});
    const auto second = spice::sct::SctDocumentImporter::import(parsed,
        {{spice::sct::SctPlatform::GameCube}, spice::sct::kSctWindows1252Byte7FEncoding});
    std::string importDiagnostics;
    for (const auto& diagnostic : first.diagnostics) importDiagnostics += diagnostic.message + "\n";
    ASSERT_TRUE(first.document.has_value()) << importDiagnostics;
    ASSERT_TRUE(second.document.has_value());
    EXPECT_EQ(first.document->sections.size(), second.document->sections.size());
    EXPECT_EQ(documentStringCount(*first.document), documentStringCount(*second.document));
    EXPECT_EQ(first.document->footerEntries.size(), second.document->footerEntries.size());
    EXPECT_EQ(first.document->opaqueAttachments.size(), second.document->opaqueAttachments.size());
    ASSERT_FALSE(first.document->sections.empty());
    EXPECT_EQ(first.document->sections.front().id, second.document->sections.front().id);
    EXPECT_EQ(first.document->sections.front().nameBytes, parsed.file.sections.front().id.name);

    std::size_t typedExpressions = 0;
    for (const auto& section : first.document->sections) {
        const auto* script = std::get_if<spice::sct::SctScriptSectionContent>(&section.content);
        if (!script) continue;
        for (const auto& instruction : script->instructions) {
            const auto count = [&](const auto& parameters) {
                for (const auto& parameter : parameters) {
                    const auto* expression = std::get_if<spice::sct::SctCanonicalExpression>(&parameter.value);
                    if (expression && std::holds_alternative<spice::sct::SctTypedScptProgram>(expression->body)) {
                        ++typedExpressions;
                    }
                }
            };
            count(instruction.fixedParameters);
            for (const auto& group : instruction.repeatedParameterGroups) count(group.parameters);
        }
    }
    EXPECT_GT(typedExpressions, 100u);
    const auto evidence = first.context.bind(first.context.revisionProvenance());
    ASSERT_TRUE(evidence);

    EXPECT_TRUE(first.context.receipt().sourceMap.hasCompleteLeafCoverage());
    EXPECT_TRUE(std::none_of(first.document->opaqueAttachments.begin(), first.document->opaqueAttachments.end(),
        [&](const auto& attachment) {
            return attachment.fixedOffset == 0 && attachment.bytes.size() == parsed.file.originalPayloadBytes.size();
        }));
    const auto validation = spice::sct::SctDocumentValidator::validateForTarget(
        *first.document, spice::sct::SctPlatform::GameCube,
        spice::sct::kSctWindows1252Byte7FEncoding, &*evidence);
    EXPECT_TRUE(validation.validForTarget);
}

TEST(SctRealFixtures, Me004aCarriesGameCubeOnlyOpcode265)
{
    const auto fixture = findSctFixture("me004a.sct");
    if (fixture.empty()) {
        GTEST_SKIP() << "me004a.sct real fixture is not present in the ignored soa_parser_reference_bundle.";
    }

    const auto parsed = spice::sct::SctParser{}.parseFile(fixture.string());
    ASSERT_TRUE(parsed.parseOk);
    const auto imported = spice::sct::SctDocumentImporter::import(parsed,
        {{spice::sct::SctPlatform::GameCube}, spice::sct::kSctWindows1252Byte7FEncoding});
    ASSERT_TRUE(imported.document.has_value());
    const auto evidence = imported.context.bind(imported.context.revisionProvenance());
    ASSERT_TRUE(evidence);

    const auto [opcode265Count, typedReferenceCount] = opcode265ReferenceCounts(*imported.document);
    ASSERT_GT(opcode265Count, 0u);
    EXPECT_EQ(opcode265Count, typedReferenceCount);

    const auto structural = spice::sct::SctDocumentValidator::validateDocument(*imported.document);
    const auto gameCube = spice::sct::SctDocumentValidator::validateForTarget(
        *imported.document, spice::sct::SctPlatform::GameCube,
        spice::sct::kSctWindows1252Byte7FEncoding, &*evidence);
    const auto dreamcast = spice::sct::SctDocumentValidator::validateForTarget(
        *imported.document, spice::sct::SctPlatform::Dreamcast,
        spice::sct::kSctWindows1252Byte7FEncoding, &*evidence);
    EXPECT_TRUE(structural.validDocument);
    EXPECT_TRUE(gameCube.validForTarget);
    EXPECT_FALSE(dreamcast.validForTarget);
    EXPECT_TRUE(std::any_of(dreamcast.diagnostics.begin(), dreamcast.diagnostics.end(),
        [](const auto& diagnostic) {
            return diagnostic.code == spice::sct::SctDiagnosticCode::OpcodeUnavailable;
        }));
}

TEST(SctRealFixtures, Me017bStrictDocumentExportPreservesOpaqueBytesAndReimports)
{
    const auto fixture = findSctFixture("me017b.sct");
    if (fixture.empty()) {
        GTEST_SKIP() << "me017b.sct real fixture is not present in the ignored reference bundle.";
    }
    const auto parsed = spice::sct::SctParser{}.parseFile(fixture.string());
    ASSERT_TRUE(parsed.parseOk);
    const auto imported = spice::sct::SctDocumentImporter::import(parsed,
        {{spice::sct::SctPlatform::GameCube}, spice::sct::kSctWindows1252Byte7FEncoding});
    ASSERT_TRUE(imported.document.has_value());
    const auto evidence = imported.context.bind(imported.context.revisionProvenance());
    ASSERT_TRUE(evidence);

    const spice::sct::SctDocumentExportOptions options{
        spice::sct::SctPlatform::GameCube,
        spice::sct::kSctWindows1252Byte7FEncoding,
        spice::sct::SctDocumentOutputByteOrder::BigEndian,
        spice::sct::SctDocumentOutputWrapper::Aklz,
        spice::sct::SctOpaquePreservationPolicy::RequirePreservation};
    const auto exported = spice::sct::SctDocumentExporter::exportDocument(
        *imported.document, options, &*evidence);
    ASSERT_TRUE(exported.success) << documentDiagnosticMessages(exported.diagnostics);
    ASSERT_TRUE(exported.layout.has_value());
    ASSERT_EQ(exported.preservation.attachments.size(), imported.document->opaqueAttachments.size());

    const auto decoded = spice::compression::aklz::decompress(exported.bytes);
    ASSERT_TRUE(decoded.ok());
    for (const auto& preservation : exported.preservation.attachments) {
        ASSERT_EQ(preservation.status, spice::sct::SctOpaquePreservationStatus::PreservedByteIdentically);
        const auto found = std::find_if(imported.document->opaqueAttachments.begin(),
            imported.document->opaqueAttachments.end(), [&](const auto& attachment) {
                return attachment.id == preservation.id;
            });
        ASSERT_NE(found, imported.document->opaqueAttachments.end());
        ASSERT_LE(static_cast<std::size_t>(preservation.span.offset) + preservation.span.size, decoded.bytes.size());
        EXPECT_TRUE(std::equal(found->bytes.begin(), found->bytes.end(),
            decoded.bytes.begin() + preservation.span.offset));
    }

    const auto reparsed = spice::sct::SctParser{}.parse(exported.bytes, "me017b.document-export.sct");
    ASSERT_TRUE(reparsed.parseOk);
    const auto reimported = spice::sct::SctDocumentImporter::import(reparsed,
        {{spice::sct::SctPlatform::GameCube}, spice::sct::kSctWindows1252Byte7FEncoding});
    ASSERT_TRUE(reimported.document.has_value());
    EXPECT_EQ(reimported.document->sections.size(), imported.document->sections.size());
    EXPECT_EQ(documentInstructionCount(*reimported.document), documentInstructionCount(*imported.document));
    EXPECT_EQ(documentStringCount(*reimported.document), documentStringCount(*imported.document));
    EXPECT_EQ(reimported.document->footerEntries.size(), imported.document->footerEntries.size());
}

TEST(SctRealFixtures, Me004aStrictExportAcceptsGameCubeAndRejectsDreamcast)
{
    const auto fixture = findSctFixture("me004a.sct");
    if (fixture.empty()) {
        GTEST_SKIP() << "me004a.sct real fixture is not present in the ignored reference bundle.";
    }
    const auto parsed = spice::sct::SctParser{}.parseFile(fixture.string());
    ASSERT_TRUE(parsed.parseOk);
    const auto imported = spice::sct::SctDocumentImporter::import(parsed,
        {{spice::sct::SctPlatform::GameCube}, spice::sct::kSctWindows1252Byte7FEncoding});
    ASSERT_TRUE(imported.document.has_value());
    const auto evidence = imported.context.bind(imported.context.revisionProvenance());
    ASSERT_TRUE(evidence);
    spice::sct::SctDocumentExportOptions options{
        spice::sct::SctPlatform::GameCube,
        spice::sct::kSctWindows1252Byte7FEncoding,
        spice::sct::SctDocumentOutputByteOrder::BigEndian,
        spice::sct::SctDocumentOutputWrapper::Raw,
        spice::sct::SctOpaquePreservationPolicy::RequirePreservation};
    const auto gameCube = spice::sct::SctDocumentExporter::exportDocument(
        *imported.document, options, &*evidence);
    ASSERT_TRUE(gameCube.success) << documentDiagnosticMessages(gameCube.diagnostics);
    const auto reparsed = spice::sct::SctParser{}.parse(gameCube.bytes, "me004a.document-export.sct");
    ASSERT_TRUE(reparsed.parseOk);
    const auto reimported = spice::sct::SctDocumentImporter::import(reparsed,
        {{spice::sct::SctPlatform::GameCube}, spice::sct::kSctWindows1252Byte7FEncoding});
    ASSERT_TRUE(reimported.document.has_value());
    const auto [original265Count, originalTypedCount] = opcode265ReferenceCounts(*imported.document);
    const auto [reimported265Count, reimportedTypedCount] = opcode265ReferenceCounts(*reimported.document);
    ASSERT_GT(original265Count, 0u);
    EXPECT_EQ(original265Count, originalTypedCount);
    EXPECT_EQ(original265Count, reimported265Count);
    EXPECT_EQ(reimported265Count, reimportedTypedCount);

    options.targetPlatform = spice::sct::SctPlatform::Dreamcast;
    options.byteOrder = spice::sct::SctDocumentOutputByteOrder::LittleEndian;
    const auto dreamcast = spice::sct::SctDocumentExporter::exportDocument(
        *imported.document, options, &*evidence);
    EXPECT_FALSE(dreamcast.success);
    EXPECT_TRUE(std::any_of(dreamcast.diagnostics.begin(), dreamcast.diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.code == spice::sct::SctDiagnosticCode::OpcodeUnavailable;
    }));
    EXPECT_TRUE(std::any_of(dreamcast.diagnostics.begin(), dreamcast.diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.code == spice::sct::SctDiagnosticCode::OpaquePlatformUnverified;
    }));
}

TEST(SctRealFixtures, Me002eAmbiguousIndexedRecordRemainsWhollyOpaque)
{
    const auto fixture = findSctFixture("me002e.sct");
    if (fixture.empty()) {
        GTEST_SKIP() << "me002e.sct real fixture is not present in the ignored reference bundle.";
    }
    const auto parsed = spice::sct::SctParser{}.parseFile(fixture.string());
    ASSERT_TRUE(parsed.parseOk);
    const auto imported = spice::sct::SctDocumentImporter::import(parsed,
        {{spice::sct::SctPlatform::GameCube}, spice::sct::kSctWindows1252Byte7FEncoding});
    ASSERT_TRUE(imported.document.has_value());
    const auto section = std::find_if(imported.document->sections.begin(), imported.document->sections.end(),
        [](const auto& candidate) { return candidate.nameBytes == "M99990010"; });
    ASSERT_NE(section, imported.document->sections.end());
    const auto* content = std::get_if<spice::sct::SctStringSectionContent>(&section->content);
    ASSERT_NE(content, nullptr);
    const auto index = spice::sct::SctDocumentIndex::build(*imported.document);
    const auto* string = index.find(*imported.document, content->string.id);
    ASSERT_NE(string, nullptr);
    EXPECT_TRUE(std::holds_alternative<spice::sct::SctOpaqueText>(string->value));
}

TEST(SctRealFixtures, Me017bStructuredAnalysisIsDeterministicAndCoversEveryInstruction)
{
    const auto fixture = findSctFixture("me017b.sct");
    if (fixture.empty()) {
        GTEST_SKIP() << "me017b.sct real fixture is not present in the ignored reference bundle.";
    }
    const auto parsed = spice::sct::SctParser{}.parseFile(fixture.string());
    ASSERT_TRUE(parsed.parseOk);
    const auto imported = spice::sct::SctDocumentImporter::import(parsed,
        {{spice::sct::SctPlatform::GameCube}, spice::sct::kSctWindows1252Byte7FEncoding});
    ASSERT_TRUE(imported.document.has_value());
    const auto evidence = imported.context.bind(imported.context.revisionProvenance());
    ASSERT_TRUE(evidence.has_value());

    const auto aggregate = spice::sct::SctDocumentAnalysis::build(
        *imported.document, &*evidence);
    const auto standalone = spice::sct::SctStructuredControlFlowAnalysis::build(
        *imported.document, &*evidence);
    EXPECT_TRUE(std::ranges::equal(
        aggregate.structuredControlFlow.sections(), standalone.sections()));

    std::size_t coveredInstructions = 0;
    for (const auto& section : aggregate.structuredControlFlow.sections()) {
        for (const auto& block : section.blocks) {
            ASSERT_FALSE(block.instructions.empty());
            EXPECT_EQ(block.id.entryInstruction, block.instructions.front());
            coveredInstructions += block.instructions.size();
        }
        for (const auto& region : section.regions) {
            EXPECT_TRUE(std::ranges::none_of(region.evidence, [](const auto& item) {
                return item.kind == spice::sct::SctStructureEvidenceKind::ImportedControlFlow
                    || item.kind
                        == spice::sct::SctStructureEvidenceKind::ImportedOpaqueControlFlowGap;
            }));
        }
    }
    EXPECT_EQ(coveredInstructions, documentInstructionCount(*imported.document));
}
