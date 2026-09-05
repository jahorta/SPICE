#include "../Compression/Aklz.h"
#include "../SpiceRoot/Binary/EndianWriter.h"
#include "../SpiceStd/SpiceStd.h"
#include "MldCorpusTestSupport.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

using spice::root::Endian;
using spice::root::EndianSpanWriter;
using namespace spice::stdfile;

std::vector<std::uint8_t> makeActionRows(const Endian endian, const std::uint32_t rowCount = 2U) {
    std::vector<std::uint8_t> bytes(0x10U + rowCount * 0x18U, 0U);
    EndianSpanWriter writer(bytes, endian);
    writer.write_u16_at(0x00U, 0x34U);
    writer.write_u16_at(0x02U, 0x12U);
    writer.write_u32_at(0x04U, 0x10203040U);
    writer.write_u32_at(0x08U, rowCount);
    writer.write_u32_at(0x0cU, 0x50607080U);
    for (std::uint32_t index = 0U; index < rowCount; ++index) {
        const auto offset = 0x10U + index * 0x18U;
        writer.write_i16_at(offset + 0x00U, static_cast<std::int16_t>(10 + index));
        writer.write_i16_at(offset + 0x02U, 2);
        writer.write_i16_at(offset + 0x04U, static_cast<std::int16_t>(20 + index));
        writer.write_i16_at(offset + 0x06U, static_cast<std::int16_t>(30 + index));
        writer.write_u32_at(offset + 0x08U, 0x80000000U | index);
        writer.write_i16_at(offset + 0x0cU, static_cast<std::int16_t>(40 + index));
        writer.write_i16_at(offset + 0x0eU, static_cast<std::int16_t>(50 + index));
        writer.write_u32_at(offset + 0x10U, 0x3f800000U + index);
        writer.write_u32_at(offset + 0x14U, 0x40000000U + index);
    }
    return bytes;
}

std::vector<std::uint8_t> makeEntryTable(const Endian endian, const bool typed = true,
    const bool includeGap = false) {
    const std::uint32_t payloadSize = typed ? kStdActionViewPayloadSize : 3U;
    const std::uint32_t gapSize = includeGap ? 4U : 0U;
    const std::uint32_t relativePayloadOffset = 0x20U + gapSize;
    std::vector<std::uint8_t> bytes(0x30U + gapSize + payloadSize, 0U);
    EndianSpanWriter writer(bytes, endian);
    writer.write_u16_at(0x00U, 2U);
    writer.write_u16_at(0x02U, 4U);
    writer.write_u32_at(0x04U, 0x11223344U);
    writer.write_u32_at(0x08U, 0x55667788U);
    writer.write_u32_at(0x0cU, static_cast<std::uint32_t>(bytes.size() - 0x10U));
    writer.write_i16_at(0x10U, typed ? 0x2a : 1);
    writer.write_i16_at(0x12U, typed ? 3 : 2);
    writer.write_u32_at(0x14U, 0xabcdef01U);
    writer.write_u32_at(0x18U, payloadSize);
    writer.write_u32_at(0x1cU, relativePayloadOffset);
    writer.write_i16_at(0x20U, -1);
    writer.write_i16_at(0x22U, 7);
    writer.write_u32_at(0x24U, 0x01020304U);
    writer.write_u32_at(0x28U, 0x05060708U);
    writer.write_u32_at(0x2cU, 0x090a0b0cU);
    if (includeGap) {
        bytes[0x30U] = 0xdeU; bytes[0x31U] = 0xadU; bytes[0x32U] = 0xbeU; bytes[0x33U] = 0xefU;
    }
    const auto payloadOffset = 0x10U + relativePayloadOffset;
    if (typed) {
        writer.write_i16_at(payloadOffset + 0x00U, 11);
        writer.write_i16_at(payloadOffset + 0x02U, 12);
        writer.write_i16_at(payloadOffset + 0x04U, 13);
        writer.write_u16_at(payloadOffset + 0x06U, 0x14U);
        writer.write_u32_at(payloadOffset + 0x08U, 0x15161718U);
        writer.write_u32_at(payloadOffset + 0x0cU, 0x191a1b1cU);
        writer.write_u32_at(payloadOffset + 0x10U, 0x1d1e1f20U);
        writer.write_u32_at(payloadOffset + 0x14U, 0x21222324U);
        writer.write_i16_at(payloadOffset + 0x18U, 25);
        writer.write_u16_at(payloadOffset + 0x1aU, 26U);
        writer.write_i16_at(payloadOffset + 0x1cU, 27);
        writer.write_i16_at(payloadOffset + 0x1eU, 28);
        writer.write_i16_at(payloadOffset + 0x20U, 29);
        writer.write_i16_at(payloadOffset + 0x22U, 30);
    } else {
        bytes[payloadOffset] = 0xaaU; bytes[payloadOffset + 1U] = 0xbbU; bytes[payloadOffset + 2U] = 0xccU;
    }
    return bytes;
}

std::vector<std::uint8_t> makeTerminatorOnly(const Endian endian) {
    std::vector<std::uint8_t> bytes(0x20U, 0U);
    EndianSpanWriter writer(bytes, endian);
    writer.write_u16_at(0x00U, 1U);
    writer.write_u16_at(0x02U, 4U);
    writer.write_u32_at(0x0cU, 0x10U);
    writer.write_i16_at(0x10U, -1);
    return bytes;
}

std::vector<std::uint8_t> compressed(const std::vector<std::uint8_t>& bytes) {
    auto result = spice::compression::aklz::compress(bytes);
    EXPECT_TRUE(result.ok());
    return result.bytes;
}

bool hasCode(const std::vector<StdDocumentDiagnostic>& diagnostics, const StdDiagnosticCode code) {
    return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const auto& item) { return item.code == code; });
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
}

StdPlatform platformFor(const Endian endian) {
    return endian == Endian::Little ? StdPlatform::Dreamcast : StdPlatform::GameCube;
}

} // namespace

TEST(SpiceStdDocumentImporter, EquivalentActionRowsAcrossEndianAndCompression) {
    std::optional<StdDocument> expected{};
    for (const auto endian : { Endian::Little, Endian::Big }) {
        const auto raw = makeActionRows(endian);
        for (const auto& bytes : { raw, compressed(raw) }) {
            const auto imported = StdDocumentImporter::importBytes(bytes);
            ASSERT_TRUE(imported.ok());
            EXPECT_EQ(imported.receipt.byteOrder, endian);
            ASSERT_TRUE(std::holds_alternative<StdActionRowsContent>(imported.document->content));
            if (!expected.has_value()) expected = imported.document;
            else EXPECT_EQ(*imported.document, *expected);
        }
    }
}

TEST(SpiceStdDocumentImporter, EquivalentTypedEntryTablesAcrossEndianAndCompression) {
    std::optional<StdDocument> expected{};
    for (const auto endian : { Endian::Little, Endian::Big }) {
        const auto raw = makeEntryTable(endian);
        for (const auto& bytes : { raw, compressed(raw) }) {
            const auto imported = StdDocumentImporter::importBytes(bytes);
            ASSERT_TRUE(imported.ok());
            const auto& table = std::get<StdEntryTableContent>(imported.document->content);
            ASSERT_EQ(table.records.size(), 1U);
            ASSERT_EQ(table.payloads.size(), 1U);
            EXPECT_TRUE(std::holds_alternative<StdActionViewPayload>(table.payloads[0].content));
            if (!expected.has_value()) expected = imported.document;
            else EXPECT_EQ(*imported.document, *expected);
        }
    }
}

TEST(SpiceStdDocumentImporter, AuthoritativeByteOrderDoesNotRetry) {
    const auto bytes = makeActionRows(Endian::Little);
    const auto correct = StdDocumentImporter::importBytes(bytes, { .byteOrder = Endian::Little });
    ASSERT_TRUE(correct.ok());
    EXPECT_EQ(correct.receipt.byteOrderSelection, StdByteOrderSelection::CallerSpecified);
    const auto wrong = StdDocumentImporter::importBytes(bytes, { .byteOrder = Endian::Big });
    ASSERT_TRUE(wrong.ok());
    EXPECT_TRUE(std::holds_alternative<StdOpaqueContent>(wrong.document->content));
    EXPECT_EQ(wrong.receipt.byteOrder, Endian::Big);

    std::vector<std::uint8_t> malformedBytes(0x10U, 0U);
    const auto malformed = StdDocumentImporter::importBytes(malformedBytes, { .byteOrder = Endian::Little });
    EXPECT_FALSE(malformed.ok());
    EXPECT_TRUE(hasCode(malformed.diagnostics, StdDiagnosticCode::MalformedActionRows));
}

TEST(SpiceStdDocumentImporter, DistinguishesMalformedUnknownAndAmbiguousInputs) {
    std::vector<std::uint8_t> zeroRows(0x10U, 0U);
    const auto malformed = StdDocumentImporter::importBytes(zeroRows, { .byteOrder = Endian::Little });
    EXPECT_FALSE(malformed.ok());
    EXPECT_TRUE(hasCode(malformed.diagnostics, StdDiagnosticCode::MalformedActionRows));

    const std::vector<std::uint8_t> unknown{ 1U, 2U, 3U, 4U, 5U };
    const auto undetectable = StdDocumentImporter::importBytes(unknown);
    EXPECT_FALSE(undetectable.ok());
    EXPECT_TRUE(hasCode(undetectable.diagnostics, StdDiagnosticCode::ByteOrderUndetectable));
    const auto preserved = StdDocumentImporter::importBytes(unknown, { .byteOrder = Endian::Big });
    ASSERT_TRUE(preserved.ok());
    EXPECT_TRUE(std::holds_alternative<StdOpaqueContent>(preserved.document->content));

    auto ambiguous = makeActionRows(Endian::Little, 1U);
    EndianSpanWriter writer(ambiguous, Endian::Little);
    writer.write_u16_at(0x00U, 1U);
    writer.write_u16_at(0x02U, 4U);
    writer.write_u32_at(0x08U, 1U);
    writer.write_u32_at(0x0cU, 0x18U);
    writer.write_i16_at(0x10U, -1);
    const auto ambiguousResult = StdDocumentImporter::importBytes(ambiguous, { .byteOrder = Endian::Little });
    EXPECT_FALSE(ambiguousResult.ok());
    EXPECT_TRUE(hasCode(ambiguousResult.diagnostics, StdDiagnosticCode::LayoutAmbiguous));
}

TEST(SpiceStdDocumentImporter, AcceptsTerminatorOnlyEntryTable) {
    const auto imported = StdDocumentImporter::importBytes(makeTerminatorOnly(Endian::Big));
    ASSERT_TRUE(imported.ok());
    const auto& table = std::get<StdEntryTableContent>(imported.document->content);
    EXPECT_TRUE(table.records.empty());
    EXPECT_EQ(table.terminator.negativeLocation, -1);
    const auto written = StdDocumentWriter::write(*imported.document,
        { StdPlatform::GameCube, StdCompression::None });
    ASSERT_TRUE(written.ok());
    EXPECT_EQ(written.bytes, makeTerminatorOnly(Endian::Big));
}

TEST(SpiceStdDocumentWriter, ActionRowEditsSurviveAllTargets) {
    auto imported = StdDocumentImporter::importBytes(makeActionRows(Endian::Little));
    ASSERT_TRUE(imported.ok());
    auto& rows = std::get<StdActionRowsContent>(imported.document->content);
    rows.rows[0].flags = 0x89abcdefU;
    rows.rows[1].raw14Bits = 0x13572468U;
    const auto expected = *imported.document;
    for (const auto platform : { StdPlatform::Dreamcast, StdPlatform::GameCube }) {
        for (const auto compression : { StdCompression::None, StdCompression::Aklz }) {
            const auto written = StdDocumentWriter::write(expected, { platform, compression });
            ASSERT_TRUE(written.ok());
            const auto reparsed = StdDocumentImporter::importBytes(written.bytes);
            ASSERT_TRUE(reparsed.ok());
            EXPECT_EQ(*reparsed.document, expected);
        }
    }
}

TEST(SpiceStdDocumentWriter, TypedActionViewEditsSurviveAllTargetsWithoutReceipt) {
    auto imported = StdDocumentImporter::importBytes(makeEntryTable(Endian::Big));
    ASSERT_TRUE(imported.ok());
    auto& table = std::get<StdEntryTableContent>(imported.document->content);
    auto& payload = std::get<StdActionViewPayload>(table.payloads[0].content);
    payload.requestedMode = 99;
    payload.actionViewFlags ^= 0x80000000U;
    const auto expected = *imported.document;
    for (const auto platform : { StdPlatform::Dreamcast, StdPlatform::GameCube }) {
        for (const auto compression : { StdCompression::None, StdCompression::Aklz }) {
            const auto written = StdDocumentWriter::write(expected, { platform, compression });
            ASSERT_TRUE(written.ok());
            const auto reparsed = StdDocumentImporter::importBytes(written.bytes);
            ASSERT_TRUE(reparsed.ok());
            EXPECT_EQ(*reparsed.document, expected);
        }
    }
}

TEST(SpiceStdDocument, AllocatesStableIdsIndependentOfOrder) {
    auto imported = StdDocumentImporter::importBytes(makeActionRows(Endian::Little));
    ASSERT_TRUE(imported.ok());
    auto& rows = std::get<StdActionRowsContent>(imported.document->content).rows;
    const auto firstId = rows[0].id;
    const auto secondId = rows[1].id;
    std::swap(rows[0], rows[1]);
    EXPECT_EQ(rows[0].id, secondId);
    EXPECT_EQ(rows[1].id, firstId);
    const auto newId = imported.document->allocateActionRowId();
    EXPECT_GT(newId.value, std::max(firstId.value, secondId.value));
    rows.push_back(StdActionRow{ .id = newId });
    rows.erase(rows.begin() + 1);
    EXPECT_EQ(rows[0].id, secondId);
    EXPECT_EQ(rows[1].id, newId);
}

TEST(SpiceStdDocumentValidator, RejectsDuplicateDanglingAndDuplicateLayoutOwnership) {
    auto imported = StdDocumentImporter::importBytes(makeEntryTable(Endian::Big));
    ASSERT_TRUE(imported.ok());
    auto duplicate = *imported.document;
    auto& duplicateTable = std::get<StdEntryTableContent>(duplicate.content);
    duplicateTable.payloads.push_back(duplicateTable.payloads.front());
    EXPECT_TRUE(hasCode(StdDocumentValidator::validate(duplicate,
        { StdPlatform::GameCube, StdCompression::None }).diagnostics, StdDiagnosticCode::DuplicateId));

    auto dangling = *imported.document;
    std::get<StdEntryTableContent>(dangling.content).records[0].payload = StdEntryPayloadId{ 999U };
    EXPECT_TRUE(hasCode(StdDocumentValidator::validate(dangling,
        { StdPlatform::GameCube, StdCompression::None }).diagnostics, StdDiagnosticCode::DanglingReference));

    auto repeated = *imported.document;
    auto& repeatedTable = std::get<StdEntryTableContent>(repeated.content);
    repeatedTable.payloadLayout.push_back(repeatedTable.payloadLayout.front());
    EXPECT_TRUE(hasCode(StdDocumentValidator::validate(repeated,
        { StdPlatform::GameCube, StdCompression::None }).diagnostics, StdDiagnosticCode::DuplicateLayoutOwnership));
}

TEST(SpiceStdDocumentWriter, RebuildsOffsetsAfterOpaquePayloadResizeAndPreservesGaps) {
    auto imported = StdDocumentImporter::importBytes(makeEntryTable(Endian::Little, false, true));
    ASSERT_TRUE(imported.ok());
    auto& table = std::get<StdEntryTableContent>(imported.document->content);
    ASSERT_EQ(table.opaqueFragments.size(), 1U);
    ASSERT_TRUE(std::holds_alternative<StdOpaquePayload>(table.payloads[0].content));
    std::get<StdOpaquePayload>(table.payloads[0].content).bytes.push_back(0xddU);
    const auto expected = *imported.document;
    EXPECT_FALSE(StdDocumentWriter::write(expected,
        { StdPlatform::Dreamcast, StdCompression::None }).ok());
    const auto written = StdDocumentWriter::write(expected,
        { StdPlatform::Dreamcast, StdCompression::Aklz }, &imported.receipt);
    ASSERT_TRUE(written.ok());
    const auto reparsed = StdDocumentImporter::importBytes(written.bytes);
    ASSERT_TRUE(reparsed.ok());
    EXPECT_EQ(*reparsed.document, expected);
    const auto crossEndian = StdDocumentWriter::write(expected,
        { StdPlatform::GameCube, StdCompression::None }, &imported.receipt);
    EXPECT_FALSE(crossEndian.ok());
    EXPECT_TRUE(hasCode(crossEndian.diagnostics, StdDiagnosticCode::OpaqueByteOrderMismatch));
}

TEST(SpiceStdDocumentWriter, TopLevelOpaqueRequiresExactReceiptButAllowsWrapperChange) {
    const std::vector<std::uint8_t> bytes{ 9U, 8U, 7U, 6U, 5U };
    auto imported = StdDocumentImporter::importBytes(bytes, { .byteOrder = Endian::Big });
    ASSERT_TRUE(imported.ok());
    EXPECT_FALSE(StdDocumentWriter::write(*imported.document,
        { StdPlatform::GameCube, StdCompression::None }).ok());
    const auto compressedOutput = StdDocumentWriter::write(*imported.document,
        { StdPlatform::GameCube, StdCompression::Aklz }, &imported.receipt);
    ASSERT_TRUE(compressedOutput.ok());
    const auto decoded = spice::compression::aklz::decompress(compressedOutput.bytes);
    ASSERT_TRUE(decoded.ok());
    EXPECT_EQ(decoded.bytes, bytes);
    std::get<StdOpaqueContent>(imported.document->content).decodedBytes[0] ^= 1U;
    const auto modified = StdDocumentWriter::write(*imported.document,
        { StdPlatform::GameCube, StdCompression::None }, &imported.receipt);
    EXPECT_FALSE(modified.ok());
    EXPECT_TRUE(hasCode(modified.diagnostics, StdDiagnosticCode::ReceiptMismatch));
}

TEST(SpiceStdJsonExporter, SeparatesReceiptFromSemanticDocument) {
    const auto imported = StdDocumentImporter::importBytes(makeActionRows(Endian::Little));
    ASSERT_TRUE(imported.ok());
    const auto json = StdJsonExporter{}.toJson(imported);
    EXPECT_NE(json.find("spice_std_document_v2"), std::string::npos);
    EXPECT_NE(json.find("\"receipt\""), std::string::npos);
    EXPECT_NE(json.find("\"document\""), std::string::npos);
    EXPECT_NE(json.find("\"raw06\""), std::string::npos);
    EXPECT_EQ(json.find("rawBytesHex"), std::string::npos);
}

TEST(SpiceStdRealCorpus, RequestedRegionalArtifactsNoEditRoundTrip) {
    if (!spice::tests::corpusTestsEnabled()) GTEST_SKIP() << spice::tests::kCorpusTestsOptInMessage;
    struct Corpus { std::filesystem::path root; Endian endian; StdCompression compression; };
    const std::array corpora{
        Corpus{ R"(D:\SoADC\SoA(Usa)Disc1Assets\BCHARA)", Endian::Little, StdCompression::None },
        Corpus{ R"(D:\SoADC\SoA(Eu)bchara_combined)", Endian::Little, StdCompression::None },
        Corpus{ R"(D:\SoADC\SoA(JP)Disc1\Track 03\ETERNAL_ARCADIA_DISC1\BCHARA)", Endian::Little, StdCompression::None },
        Corpus{ R"(D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends\bchara)", Endian::Big, StdCompression::Aklz },
        Corpus{ R"(D:\SoAGC\2003-03-05-gc-eu-final_Skies_of_Arcadia_Legends\bchara)", Endian::Big, StdCompression::Aklz },
        Corpus{ R"(D:\SoAGC\2002-11-12-gc-jp-final_Eternal_Arcadia_Legends\bchara)", Endian::Big, StdCompression::Aklz },
    };
    const std::array names{ "ma000.std", "ma0000.std", "ma001.std", "ma0010.std",
        "mb000.std", "mb0000.std", "damage.std" };
    std::size_t accepted = 0U;
    for (const auto& corpus : corpora) {
        ASSERT_TRUE(std::filesystem::is_directory(corpus.root)) << corpus.root.string();
        for (const auto* name : names) {
            const auto path = corpus.root / name;
            ASSERT_TRUE(std::filesystem::exists(path)) << path.string();
            const auto original = readBytes(path);
            const auto imported = StdDocumentImporter::importFile(path);
            ASSERT_TRUE(imported.ok()) << path.string();
            EXPECT_EQ(imported.receipt.byteOrder, corpus.endian) << path.string();
            EXPECT_EQ(imported.receipt.compression, corpus.compression) << path.string();
            const auto written = StdDocumentWriter::write(*imported.document,
                { platformFor(corpus.endian), corpus.compression }, &imported.receipt);
            ASSERT_TRUE(written.ok()) << path.string();
            const auto reparsed = StdDocumentImporter::importBytes(written.bytes);
            ASSERT_TRUE(reparsed.ok()) << path.string();
            EXPECT_EQ(*reparsed.document, *imported.document) << path.string();
            if (corpus.compression == StdCompression::None) {
                EXPECT_EQ(written.bytes, original) << path.string();
            } else {
                const auto expectedDecoded = spice::compression::aklz::decompress(original);
                const auto actualDecoded = spice::compression::aklz::decompress(written.bytes);
                ASSERT_TRUE(expectedDecoded.ok()); ASSERT_TRUE(actualDecoded.ok());
                EXPECT_EQ(actualDecoded.bytes, expectedDecoded.bytes) << path.string();
            }
            ++accepted;
        }
    }
    EXPECT_EQ(accepted, 42U);
}
