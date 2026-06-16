#include "../Compression/Aklz.h"
#include "../SpiceGvm/Encoding/GvrEncoder.h"
#include "../SpiceMll/SpiceMll.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace {

void writeBeU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    ASSERT_LE(offset + 4U, bytes.size());
    bytes[offset + 0U] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    bytes[offset + 3U] = static_cast<std::uint8_t>(value & 0xffU);
}

void writeMldHeader(std::vector<std::uint8_t>& bytes, std::size_t offset) {
    writeBeU32(bytes, offset + 0x00U, 1U);
    writeBeU32(bytes, offset + 0x04U, 0x14U);
    writeBeU32(bytes, offset + 0x08U, 0U);
    writeBeU32(bytes, offset + 0x0cU, 0x20U);
    writeBeU32(bytes, offset + 0x10U, 0U);
}

void writeName(std::vector<std::uint8_t>& bytes, std::size_t offset, const char* name) {
    for (std::size_t i = 0; name[i] != '\0'; ++i) {
        ASSERT_LT(offset + i, bytes.size());
        bytes[offset + i] = static_cast<std::uint8_t>(name[i]);
    }
}

std::vector<std::uint8_t> makeGvrTextureBytes() {
    spice::gvm::model::RgbaImage image{};
    image.width = 4U;
    image.height = 4U;
    image.rgba8.resize(4U * 4U * 4U, 0xffU);

    spice::gvm::encoding::EncodeOptions options{};
    options.textureFormat = spice::gvm::model::TextureFormat::RGBA8;
    options.hasGlobalIndex = true;
    options.globalIndex = 17U;
    return spice::gvm::encoding::encodeGvr(image, options);
}

std::vector<std::uint8_t> makeNormalMllFixture() {
    std::vector<std::uint8_t> bytes(0x140U, 0U);
    writeBeU32(bytes, 0x00U, 0x0000ffffU);
    writeBeU32(bytes, 0x04U, 0x0002ffffU);
    writeName(bytes, 0x08U, "a.mld");
    writeBeU32(bytes, 0x1cU, 0x00000048U);
    writeBeU32(bytes, 0x20U, 0x00000080U);
    writeBeU32(bytes, 0x24U, 0xffffffffU);
    writeName(bytes, 0x28U, "b.bin");
    writeBeU32(bytes, 0x3cU, 0x00000100U);
    writeBeU32(bytes, 0x40U, 0x00000004U);
    writeBeU32(bytes, 0x44U, 0xffffffffU);

    writeMldHeader(bytes, 0x48U);
    bytes[0x100U] = 'P';
    bytes[0x101U] = 'O';
    bytes[0x102U] = 'F';
    bytes[0x103U] = '0';
    return bytes;
}

std::vector<std::uint8_t> makeFirstMemberOffsetCountFixture() {
    auto bytes = makeNormalMllFixture();
    writeBeU32(bytes, 0x04U, 0x4000ffffU);
    return bytes;
}

std::vector<std::uint8_t> makeMalformedMllFixture() {
    auto bytes = makeNormalMllFixture();
    writeBeU32(bytes, 0x3cU, 0x000000a8U);
    writeBeU32(bytes, 0x40U, 0x00000100U);
    return bytes;
}

std::vector<std::uint8_t> makeMllFixtureWithIndexedBinPayload() {
    std::vector<std::uint8_t> bytes(0x90U, 0U);
    writeBeU32(bytes, 0x00U, 0x0000ffffU);
    writeBeU32(bytes, 0x04U, 0x0001ffffU);
    writeName(bytes, 0x08U, "layout.bin");
    writeBeU32(bytes, 0x1cU, 0x00000028U);
    writeBeU32(bytes, 0x20U, 0x00000050U);
    writeBeU32(bytes, 0x24U, 0xffffffffU);

    const std::size_t payloadBase = 0x28U;
    writeBeU32(bytes, payloadBase + 0x00U, 2U);
    writeBeU32(bytes, payloadBase + 0x04U, 0x10U);
    writeBeU32(bytes, payloadBase + 0x08U, 0x20U);
    writeBeU32(bytes, payloadBase + 0x1cU, 0x0cU);
    writeBeU32(bytes, payloadBase + 0x20U, 0x30U);
    bytes[payloadBase + 0x24U] = 0x41U;
    bytes[payloadBase + 0x25U] = 0x42U;
    writeBeU32(bytes, payloadBase + 0x2cU, 0x0cU);
    writeBeU32(bytes, payloadBase + 0x30U, 0x40U);
    bytes[payloadBase + 0x34U] = 0x43U;
    bytes[payloadBase + 0x35U] = 0x44U;
    return bytes;
}

std::vector<std::uint8_t> makeMllFixtureWithMldLikeIndexedBinPayload() {
    std::vector<std::uint8_t> bytes(0x128U, 0U);
    writeBeU32(bytes, 0x00U, 0x0000ffffU);
    writeBeU32(bytes, 0x04U, 0x0001ffffU);
    writeName(bytes, 0x08U, "layout.BIN");
    writeBeU32(bytes, 0x1cU, 0x00000028U);
    writeBeU32(bytes, 0x20U, 0x00000100U);
    writeBeU32(bytes, 0x24U, 0xffffffffU);

    const std::size_t payloadBase = 0x28U;
    writeBeU32(bytes, payloadBase + 0x00U, 1U);
    writeBeU32(bytes, payloadBase + 0x04U, 0x14U);
    writeBeU32(bytes, payloadBase + 0x08U, 0U);
    writeBeU32(bytes, payloadBase + 0x0cU, 0x20U);
    writeBeU32(bytes, payloadBase + 0x10U, 0U);
    writeBeU32(bytes, payloadBase + 0x1cU, 0x08U);
    writeBeU32(bytes, payloadBase + 0x20U, 0x30U);
    writeName(bytes, payloadBase + 0x38U, "probe");
    return bytes;
}

std::vector<std::uint8_t> makeMllFixtureWithMldObjectListProbe() {
    std::vector<std::uint8_t> bytes(0x340U, 0U);
    writeBeU32(bytes, 0x00U, 0x0000ffffU);
    writeBeU32(bytes, 0x04U, 0x0002ffffU);
    writeName(bytes, 0x08U, "a.mld");
    writeBeU32(bytes, 0x1cU, 0x00000048U);
    writeBeU32(bytes, 0x20U, 0x00000180U);
    writeBeU32(bytes, 0x24U, 0xffffffffU);
    writeName(bytes, 0x28U, "b.bin");
    writeBeU32(bytes, 0x3cU, 0x00000240U);
    writeBeU32(bytes, 0x40U, 0x00000004U);
    writeBeU32(bytes, 0x44U, 0xffffffffU);

    writeMldHeader(bytes, 0x48U);
    const std::size_t payloadBase = 0x48U;
    writeBeU32(bytes, payloadBase + 0x10U, 0x000000c0U);
    const std::size_t indexEntry = payloadBase + 0x14U;
    writeBeU32(bytes, indexEntry + 0x14U, 0x00000090U);
    writeBeU32(bytes, payloadBase + 0x90U, 1U);
    writeBeU32(bytes, payloadBase + 0x94U, 0x000000a0U);
    bytes[payloadBase + 0xa0U] = 'N';
    bytes[payloadBase + 0xa1U] = 'J';
    bytes[payloadBase + 0xa2U] = 'C';
    bytes[payloadBase + 0xa3U] = 'M';
    writeBeU32(bytes, payloadBase + 0xc0U, 1U);
    writeName(bytes, payloadBase + 0xc4U, "tex_17");
    const auto textureBytes = makeGvrTextureBytes();
    EXPECT_LE(payloadBase + 0x100U + textureBytes.size(), payloadBase + 0x180U);
    std::copy(textureBytes.begin(), textureBytes.end(), bytes.begin() + payloadBase + 0x100U);
    return bytes;
}

std::filesystem::path usDiscRoot() {
    const std::filesystem::path root = "D:/SoAGC/2002-12-19-gc-us-final_Skies_of_Arcadia_Legends";
    if (std::filesystem::exists(root)) {
        return root;
    }
    return {};
}

} // namespace

TEST(SpiceMllParser, ParsesNamedOffsetSizeMemberTable) {
    const auto parsed = spice::mll::MllParser::parse(makeNormalMllFixture(), "fixture.mll");

    ASSERT_TRUE(parsed.ok());
    EXPECT_TRUE(parsed.supported);
    EXPECT_EQ(parsed.memberCountSource, spice::mll::MllMemberCountSource::HeaderU16At04);
    EXPECT_EQ(parsed.tableShape, spice::mll::MllTableShape::Normal);
    EXPECT_EQ(parsed.selectedMemberCount, 2U);
    EXPECT_EQ(parsed.memberTableEndOffset, 0x48U);
    EXPECT_TRUE(parsed.memberCountMatchesFirstMemberOffset);
    ASSERT_EQ(parsed.members.size(), 2U);
    EXPECT_EQ(parsed.members[0].name, "a.mld");
    EXPECT_EQ(parsed.members[0].payloadOffset, 0x48U);
    EXPECT_EQ(parsed.members[0].payloadSize, 0x80U);
    EXPECT_EQ(parsed.members[0].rawWord1c, 0xffffffffU);
    EXPECT_TRUE(parsed.members[0].payloadInBounds);
    EXPECT_EQ(parsed.members[0].payloadKind, spice::mll::MllPayloadKind::MldFile);
    EXPECT_TRUE(parsed.members[0].embeddedMldHeader.plausible);
    EXPECT_EQ(parsed.members[1].payloadKind, spice::mll::MllPayloadKind::Pof0);
}

TEST(SpiceMllParser, ProbesEmbeddedMldObjectListFields) {
    const auto parsed = spice::mll::MllParser::parse(makeMllFixtureWithMldObjectListProbe(), "fixture.mll");

    ASSERT_TRUE(parsed.ok());
    ASSERT_EQ(parsed.members.size(), 2U);
    const auto& member = parsed.members[0];
    ASSERT_TRUE(member.embeddedMldHeader.plausible);
    EXPECT_TRUE(member.embeddedMldHeader.indexEntryShapePlausible);
    EXPECT_EQ(member.embeddedMldHeader.plausibleCountedListFieldCount, 1U);
    ASSERT_EQ(member.embeddedMldObjectListProbes.size(), 3U);

    const auto& probe = member.embeddedMldObjectListProbes[0];
    EXPECT_EQ(probe.entryIndex, 0U);
    EXPECT_EQ(probe.fieldOffset, 0x14U);
    EXPECT_EQ(probe.listOffset, 0x90U);
    EXPECT_TRUE(probe.listHeaderInBounds);
    EXPECT_EQ(probe.declaredCount, 1U);
    EXPECT_TRUE(probe.listEntriesInBounds);
    EXPECT_TRUE(probe.listLooksPlausible);
    EXPECT_EQ(probe.listBytes32Hex.substr(0U, 16U), "00000001000000a0");
    ASSERT_EQ(probe.targetSamples.size(), 1U);
    EXPECT_EQ(probe.targetSamples[0].targetOffset, 0xa0U);
    EXPECT_TRUE(probe.targetSamples[0].targetOffsetAligned);
    EXPECT_TRUE(probe.targetSamples[0].targetInBounds);
    EXPECT_TRUE(probe.targetSamples[0].targetLooksPlausible);
    EXPECT_TRUE(probe.targetSamples[0].listBaseTargetLooksPlausible);
    EXPECT_EQ(probe.targetSamples[0].entryBaseTargetOffset, 0xb4U);
    EXPECT_TRUE(probe.targetSamples[0].entryBaseTargetLooksPlausible);
    EXPECT_TRUE(probe.targetSamples[0].pointerBaseTargetLooksPlausible);
    EXPECT_EQ(probe.targetSamples[0].targetSignature, "NJCM");
    EXPECT_EQ(probe.targetSamples[0].targetBytes16Hex.substr(0U, 8U), "4e4a434d");

    ASSERT_EQ(member.embeddedBlockProbes.size(), 3U);
    const auto njcmBlock = std::find_if(member.embeddedBlockProbes.begin(), member.embeddedBlockProbes.end(), [](const auto& block) {
        return block.tag == "NJCM";
    });
    ASSERT_NE(njcmBlock, member.embeddedBlockProbes.end());
    EXPECT_EQ(njcmBlock->blockOffset, 0xa0U);
    EXPECT_TRUE(njcmBlock->offsetAligned);
    EXPECT_EQ(njcmBlock->exactCountedListReferenceCount, 1U);
    EXPECT_NE(njcmBlock->firstExactCountedListReference.find("field=0x14"), std::string::npos);
    EXPECT_EQ(njcmBlock->bytes32Hex.substr(0U, 8U), "4e4a434d");

    ASSERT_EQ(member.embeddedGvrTextureProbes.size(), 1U);
    EXPECT_EQ(member.embeddedGvrTextureProbes[0].gcixOffset, 0x100U);
    EXPECT_EQ(member.embeddedGvrTextureProbes[0].gvrtOffset, 0x110U);
    EXPECT_TRUE(member.embeddedGvrTextureProbes[0].recordInBounds);
    EXPECT_TRUE(member.embeddedGvrTextureProbes[0].parseAttempted);
    EXPECT_FALSE(member.embeddedGvrTextureProbes[0].parseHasFailureDiagnostics);
    EXPECT_TRUE(member.embeddedGvrTextureProbes[0].hasGlobalIndex);
    EXPECT_EQ(member.embeddedGvrTextureProbes[0].globalIndex, 17U);
    EXPECT_EQ(member.embeddedGvrTextureProbes[0].textureFormat, "RGBA8");
    EXPECT_EQ(member.embeddedGvrTextureProbes[0].width, 4U);
    EXPECT_EQ(member.embeddedGvrTextureProbes[0].height, 4U);
    EXPECT_TRUE(member.embeddedGvrTextureProbes[0].decodedBaseLevelPresent);

    EXPECT_TRUE(member.textureTableProbe.hasTextures);
    EXPECT_EQ(member.textureTableProbe.textureCount, 1U);
    EXPECT_EQ(member.textureTableProbe.firstTextureOffset, 0x100U);
    EXPECT_EQ(member.textureTableProbe.clusterCount, 1U);
    EXPECT_TRUE(member.textureTableProbe.allRecordsInBounds);
    EXPECT_TRUE(member.textureTableProbe.allTexturesParsed);
    EXPECT_TRUE(member.textureTableProbe.allTexturesDecoded);
    EXPECT_TRUE(member.textureTableProbe.allTexturesHaveGlobalIndex);
    EXPECT_EQ(member.textureTableProbe.globalIndexMin, 17U);
    EXPECT_EQ(member.textureTableProbe.globalIndexMax, 17U);
    EXPECT_EQ(member.textureTableProbe.uniqueGlobalIndexCount, 1U);
    EXPECT_EQ(member.textureTableProbe.duplicateGlobalIndexCount, 0U);
    EXPECT_EQ(member.textureTableProbe.missingGlobalIndexCount, 0U);
    EXPECT_TRUE(member.textureTableProbe.globalIndexSequenceDense);
    EXPECT_FALSE(member.textureTableProbe.globalIndexSequenceStartsAtZero);
    EXPECT_EQ(member.textureTableProbe.globalIndexSequencePreview, "17");
    EXPECT_EQ(member.textureTableProbe.indexTexturePointerCount, 1U);
    EXPECT_EQ(member.textureTableProbe.nonZeroIndexTexturePointerCount, 0U);

    EXPECT_TRUE(member.preTextureTableProbe.present);
    EXPECT_TRUE(member.preTextureTableProbe.spanInBounds);
    EXPECT_TRUE(member.preTextureTableProbe.spanAlignedTo20);
    EXPECT_TRUE(member.preTextureTableProbe.recordsFit);
    EXPECT_TRUE(member.preTextureTableProbe.declaredCountMatchesTextureCount);
    EXPECT_EQ(member.preTextureTableProbe.tableOffset, 0xc0U);
    EXPECT_EQ(member.preTextureTableProbe.tableEndOffset, 0x100U);
    EXPECT_EQ(member.preTextureTableProbe.declaredEntryCount, 1U);
    EXPECT_EQ(member.preTextureTableProbe.entryStride, 0x2cU);
    EXPECT_EQ(member.preTextureTableProbe.entryCount, 1U);
    EXPECT_EQ(member.preTextureTableProbe.trailingPaddingSize, 16U);
    EXPECT_EQ(member.preTextureTableProbe.printableNameCount, 1U);
    EXPECT_EQ(member.preTextureTableProbe.textureNameMatchCount, 1U);
    ASSERT_EQ(member.preTextureTableProbe.entries.size(), 1U);
    EXPECT_EQ(member.preTextureTableProbe.entries[0].entryOffset, 0xc4U);
    EXPECT_EQ(member.preTextureTableProbe.entries[0].name, "tex_17");
    EXPECT_TRUE(member.preTextureTableProbe.entries[0].nameMatchesKnownTextureGlobalIndex);
    EXPECT_EQ(member.preTextureTableProbe.entries[0].matchedGlobalIndex, 17U);
    EXPECT_TRUE(member.preTextureTableProbe.entries[0].orderTexturePresent);
    EXPECT_EQ(member.preTextureTableProbe.entries[0].orderTextureGcixOffset, 0x100U);
    EXPECT_EQ(member.preTextureTableProbe.entries[0].orderTextureGvrtOffset, 0x110U);
    EXPECT_TRUE(member.preTextureTableProbe.entries[0].orderTextureHasGlobalIndex);
    EXPECT_EQ(member.preTextureTableProbe.entries[0].orderTextureGlobalIndex, 17U);
    EXPECT_EQ(member.preTextureTableProbe.entries[0].orderTextureFormat, "RGBA8");
    EXPECT_EQ(member.preTextureTableProbe.entries[0].orderTextureWidth, 4U);
    EXPECT_EQ(member.preTextureTableProbe.entries[0].orderTextureHeight, 4U);
    EXPECT_TRUE(member.preTextureTableProbe.entries[0].orderTextureDecoded);
    EXPECT_TRUE(member.preTextureTableProbe.entries[0].nameSuffixMatchesOrderTextureGlobalIndex);
}

TEST(SpiceMllParser, ProbesUnknownIndexedBinTables) {
    const auto parsed = spice::mll::MllParser::parse(makeMllFixtureWithIndexedBinPayload(), "fixture.mll");

    ASSERT_TRUE(parsed.ok());
    ASSERT_EQ(parsed.members.size(), 1U);
    const auto& member = parsed.members[0];
    EXPECT_EQ(member.payloadKind, spice::mll::MllPayloadKind::Unknown);
    const auto& probe = member.indexedBinTableProbe;
    EXPECT_TRUE(probe.present);
    EXPECT_TRUE(probe.headerInBounds);
    EXPECT_EQ(probe.count, 2U);
    EXPECT_EQ(probe.offsetTableOffset, 0x04U);
    EXPECT_EQ(probe.offsetTableEndOffset, 0x0cU);
    EXPECT_EQ(probe.dataBaseOffset, 0x0cU);
    EXPECT_TRUE(probe.offsetTableInBounds);
    EXPECT_TRUE(probe.offsetsInBounds);
    EXPECT_TRUE(probe.offsetsMonotonic);
    EXPECT_EQ(probe.firstRecordOffset, 0x1cU);
    EXPECT_EQ(probe.lastRecordOffset, 0x2cU);
    EXPECT_EQ(probe.offsetsPreview, "16 32");
    ASSERT_EQ(probe.samples.size(), 2U);
    EXPECT_EQ(probe.samples[0].recordOffset, 0x1cU);
    EXPECT_EQ(probe.samples[0].word0, 0x0cU);
    EXPECT_TRUE(probe.samples[0].word0EqualsDataBaseOffset);
    EXPECT_EQ(probe.samples[0].word4, 0x30U);
    EXPECT_TRUE(probe.samples[0].word4TargetInBounds);
    EXPECT_EQ(probe.samples[0].word8, 0x41420000U);
    EXPECT_EQ(probe.samples[0].word12, 0U);
    EXPECT_EQ(probe.samples[0].word16, 0x0cU);
    EXPECT_EQ(probe.samples[0].word20, 0x40U);
    EXPECT_EQ(probe.samples[0].word24, 0x43440000U);
    EXPECT_EQ(probe.samples[0].bytes16Hex.substr(0U, 16U), "0000000c00000030");
    EXPECT_EQ(probe.samples[0].bytes32Hex.substr(0U, 24U), "0000000c0000003041420000");
    EXPECT_EQ(probe.samples[1].recordOffset, 0x2cU);
    EXPECT_EQ(probe.samples[1].word0, 0x0cU);
    EXPECT_EQ(probe.samples[1].word4, 0x40U);
}

TEST(SpiceMllParser, ProbesNamedBinTablesEvenWhenMldLike) {
    const auto parsed =
        spice::mll::MllParser::parse(makeMllFixtureWithMldLikeIndexedBinPayload(), "fixture.mll");

    ASSERT_TRUE(parsed.ok());
    ASSERT_EQ(parsed.members.size(), 1U);
    const auto& member = parsed.members[0];
    EXPECT_EQ(member.payloadKind, spice::mll::MllPayloadKind::MldFile);
    const auto& probe = member.indexedBinTableProbe;
    EXPECT_TRUE(probe.present);
    EXPECT_EQ(probe.count, 1U);
    EXPECT_TRUE(probe.offsetTableInBounds);
    EXPECT_EQ(probe.dataBaseOffset, 0x08U);
    ASSERT_EQ(probe.samples.size(), 1U);
    EXPECT_TRUE(probe.samples[0].word0EqualsDataBaseOffset);
    EXPECT_TRUE(probe.samples[0].word4TargetInBounds);
}

TEST(SpiceMllParser, CanProbeCountFromFirstMemberOffsetWhenHeaderCandidateDoesNotFit) {
    const auto parsed = spice::mll::MllParser::parse(makeFirstMemberOffsetCountFixture(), "inferred.mll");

    EXPECT_TRUE(parsed.ok());
    EXPECT_FALSE(parsed.supported);
    EXPECT_EQ(parsed.memberCountSource, spice::mll::MllMemberCountSource::FirstMemberOffset);
    EXPECT_EQ(parsed.selectedMemberCount, 2U);
    EXPECT_EQ(parsed.memberCountInferredFromFirstMemberOffset, 2U);
    ASSERT_EQ(parsed.members.size(), 2U);
    ASSERT_FALSE(parsed.diagnostics.empty());
    EXPECT_EQ(parsed.diagnostics[0].severity, spice::mll::DiagnosticSeverity::Warning);
}

TEST(SpiceMllParser, ReportsOutOfBoundsMemberSpan) {
    const auto parsed = spice::mll::MllParser::parse(makeMalformedMllFixture(), "bad.mll");

    EXPECT_FALSE(parsed.ok());
    EXPECT_FALSE(parsed.supported);
    EXPECT_EQ(parsed.tableShape, spice::mll::MllTableShape::MalformedMemberSpans);
    ASSERT_EQ(parsed.members.size(), 2U);
    EXPECT_FALSE(parsed.members[1].payloadInBounds);
    ASSERT_FALSE(parsed.diagnostics.empty());
}

TEST(SpiceMllParser, ParsesAklzCompressedInput) {
    const auto fixture = makeNormalMllFixture();
    const auto compressed = spice::compression::aklz::compress(fixture);
    ASSERT_TRUE(compressed.ok());

    const auto parsed = spice::mll::MllParser::parse(compressed.bytes, "compressed.mll");

    ASSERT_TRUE(parsed.ok());
    EXPECT_TRUE(parsed.sourceWasCompressedAklz);
    EXPECT_EQ(parsed.rawSize, compressed.bytes.size());
    EXPECT_EQ(parsed.decodedSize, fixture.size());
    EXPECT_EQ(parsed.members.size(), 2U);
}

TEST(SpiceMllParserRealFiles, UsKnownMllCanBeOpenedAsResearchProbe) {
    const auto root = usDiscRoot();
    if (root.empty()) {
        GTEST_SKIP() << "US Skies of Arcadia Legends dump is not present.";
    }
    const auto sample = root / "field/HrsBin_Hakken.mll";
    if (!std::filesystem::exists(sample)) {
        GTEST_SKIP() << "field/HrsBin_Hakken.mll is not present in the US dump.";
    }

    const auto parsed = spice::mll::MllParser::parseFile(sample);

    EXPECT_GT(parsed.rawSize, 0U);
    EXPECT_GT(parsed.decodedSize, 0U);
    EXPECT_EQ(parsed.sourcePath, sample.string());
}
