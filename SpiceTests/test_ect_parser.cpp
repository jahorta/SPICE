#include "../Compression/Aklz.h"
#include "../SpiceEct/SpiceEct.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {

void writeBeU16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    ASSERT_LE(offset + 2U, bytes.size());
    bytes[offset + 0U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    bytes[offset + 1U] = static_cast<std::uint8_t>(value & 0xffU);
}

void writeBeU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    ASSERT_LE(offset + 4U, bytes.size());
    bytes[offset + 0U] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    bytes[offset + 3U] = static_cast<std::uint8_t>(value & 0xffU);
}

void writeText(std::vector<std::uint8_t>& bytes, std::size_t offset, const std::string& text) {
    ASSERT_LE(offset + text.size(), bytes.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(text[i]);
    }
}

void writeEncounterTable(std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint16_t stage,
    std::uint16_t overallRate,
    std::uint16_t encounterBase) {
    ASSERT_LE(offset + 0x84U, bytes.size());
    writeBeU16(bytes, offset + 0x00U, stage);
    writeBeU16(bytes, offset + 0x02U, overallRate);
    for (std::uint16_t i = 0; i < 32U; ++i) {
        const auto entryOffset = offset + 0x04U + static_cast<std::size_t>(i) * 4U;
        writeBeU16(bytes, entryOffset + 0U, static_cast<std::uint16_t>(encounterBase + i));
        writeBeU16(bytes, entryOffset + 2U, static_cast<std::uint16_t>(i * 3U));
    }
}

std::vector<std::uint8_t> makeFlatFixture(std::size_t tableCount) {
    std::vector<std::uint8_t> bytes(tableCount * 0x84U, 0U);
    for (std::size_t i = 0; i < tableCount; ++i) {
        writeEncounterTable(bytes,
            i * 0x84U,
            static_cast<std::uint16_t>(0x100U + i),
            static_cast<std::uint16_t>(0x20U + i),
            static_cast<std::uint16_t>(0x40U + i * 0x40U));
    }
    return bytes;
}

std::vector<std::uint8_t> makeIndexedFixture() {
    constexpr std::uint32_t kFirstTableOffset = 0x60U;
    std::vector<std::uint8_t> bytes(kFirstTableOffset + 8U * 0x84U, 0U);
    writeBeU16(bytes, 0x04U, 2U);

    writeText(bytes, 0x08U, "area_00");
    writeBeU32(bytes, 0x08U + 0x14U, kFirstTableOffset);

    writeText(bytes, 0x28U, "dam_skip");
    writeBeU32(bytes, 0x28U + 0x14U, 0xffffffffU);

    for (std::size_t i = 0; i < 8U; ++i) {
        writeEncounterTable(bytes,
            kFirstTableOffset + i * 0x84U,
            static_cast<std::uint16_t>(0x200U + i),
            static_cast<std::uint16_t>(0x30U + i),
            static_cast<std::uint16_t>(0x80U + i * 0x40U));
    }
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

TEST(SpiceEctParser, ParsesOneFlatEncounterTable) {
    const auto fixture = makeFlatFixture(1U);
    const auto parsed = spice::ect::EctParser::parse(fixture, "a001a.ect");

    ASSERT_TRUE(parsed.ok());
    EXPECT_EQ(parsed.layout, spice::ect::EctLayout::FlatTables);
    EXPECT_FALSE(parsed.sourceWasCompressedAklz);
    EXPECT_EQ(parsed.decodedSize, 0x84U);
    ASSERT_EQ(parsed.tables.size(), 1U);
    EXPECT_EQ(parsed.tables[0].decodedOffset, 0U);
    EXPECT_EQ(parsed.tables[0].stage, 0x100U);
    EXPECT_EQ(parsed.tables[0].overallEncounterRate, 0x20U);
    ASSERT_EQ(parsed.tables[0].encounters.size(), 32U);
    EXPECT_EQ(parsed.tables[0].encounters[0].encounterId, 0x40U);
    EXPECT_EQ(parsed.tables[0].encounters[31].encounterRate, 93U);
}

TEST(SpiceEctParser, ParsesFlatEightTableFixture) {
    const auto fixture = makeFlatFixture(8U);
    const auto parsed = spice::ect::EctParser::parse(fixture, "a017a.ect");

    ASSERT_TRUE(parsed.ok());
    EXPECT_EQ(parsed.layout, spice::ect::EctLayout::FlatTables);
    EXPECT_EQ(parsed.decodedSize, 0x420U);
    ASSERT_EQ(parsed.tables.size(), 8U);
    EXPECT_EQ(parsed.tables[7].decodedOffset, 7U * 0x84U);
    EXPECT_EQ(parsed.tables[7].stage, 0x107U);
}

TEST(SpiceEctParser, ParsesIndexedFixtureAndPreservesSkippedDamEntry) {
    const auto fixture = makeIndexedFixture();
    const auto parsed = spice::ect::EctParser::parse(fixture, "a099a.ect");

    ASSERT_TRUE(parsed.ok());
    EXPECT_EQ(parsed.layout, spice::ect::EctLayout::IndexedContainer);
    ASSERT_EQ(parsed.indexEntries.size(), 2U);
    EXPECT_EQ(parsed.indexEntries[0].title, "area_00");
    EXPECT_FALSE(parsed.indexEntries[0].skippedDam);
    ASSERT_TRUE(parsed.indexEntries[0].firstParsedTableIndex.has_value());
    EXPECT_EQ(*parsed.indexEntries[0].firstParsedTableIndex, 0U);
    EXPECT_EQ(parsed.indexEntries[0].tableCount, 8U);
    EXPECT_EQ(parsed.indexEntries[1].title, "dam_skip");
    EXPECT_TRUE(parsed.indexEntries[1].skippedDam);
    EXPECT_FALSE(parsed.indexEntries[1].firstParsedTableIndex.has_value());
    EXPECT_EQ(parsed.indexEntries[1].tableCount, 0U);
    ASSERT_EQ(parsed.tables.size(), 8U);
    EXPECT_EQ(parsed.tables[0].stage, 0x200U);
    EXPECT_EQ(parsed.tables[7].stage, 0x207U);
}

TEST(SpiceEctParser, ReportsMalformedFlatSize) {
    const std::vector<std::uint8_t> bytes(0x83U, 0U);
    const auto parsed = spice::ect::EctParser::parse(
        bytes,
        "bad.ect",
        spice::ect::EctParseOptions{ .layoutHint = spice::ect::EctLayoutHint::FlatTables });

    EXPECT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.layout, spice::ect::EctLayout::FlatTables);
    EXPECT_TRUE(parsed.tables.empty());
    ASSERT_FALSE(parsed.diagnostics.empty());
    EXPECT_EQ(parsed.diagnostics[0].severity, spice::ect::DiagnosticSeverity::Error);
}

TEST(SpiceEctParser, ReportsIndexedOffsetPastDecodedBytes) {
    std::vector<std::uint8_t> bytes(0x40U, 0U);
    writeBeU16(bytes, 0x04U, 1U);
    writeText(bytes, 0x08U, "area_00");
    writeBeU32(bytes, 0x08U + 0x14U, 0x30U);

    const auto parsed = spice::ect::EctParser::parse(
        bytes,
        "a099a.ect",
        spice::ect::EctParseOptions{ .layoutHint = spice::ect::EctLayoutHint::IndexedContainer });

    EXPECT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.layout, spice::ect::EctLayout::IndexedContainer);
    ASSERT_EQ(parsed.indexEntries.size(), 1U);
    EXPECT_EQ(parsed.indexEntries[0].title, "area_00");
    EXPECT_TRUE(parsed.tables.empty());
}

TEST(SpiceEctParser, ParsesAklzCompressedInput) {
    const auto fixture = makeFlatFixture(1U);
    const auto compressed = spice::compression::aklz::compress(fixture);
    ASSERT_TRUE(compressed.ok());

    const auto parsed = spice::ect::EctParser::parse(compressed.bytes, "compressed.ect");

    ASSERT_TRUE(parsed.ok());
    EXPECT_TRUE(parsed.sourceWasCompressedAklz);
    EXPECT_EQ(parsed.rawSize, compressed.bytes.size());
    EXPECT_EQ(parsed.decodedSize, fixture.size());
    ASSERT_EQ(parsed.tables.size(), 1U);
}

TEST(SpiceEctParserRealFiles, UsA017aIsFlatEightTableAklzFile) {
    const auto root = usDiscRoot();
    if (root.empty()) {
        GTEST_SKIP() << "US Skies of Arcadia Legends dump is not present.";
    }

    const auto parsed = spice::ect::EctParser::parseFile(root / "field/a017a.ect");

    ASSERT_TRUE(parsed.ok());
    EXPECT_TRUE(parsed.sourceWasCompressedAklz);
    EXPECT_EQ(parsed.layout, spice::ect::EctLayout::FlatTables);
    EXPECT_EQ(parsed.decodedSize, 0x420U);
    EXPECT_EQ(parsed.tables.size(), 8U);
}

TEST(SpiceEctParserRealFiles, UsA099aIsIndexedAklzFile) {
    const auto root = usDiscRoot();
    if (root.empty()) {
        GTEST_SKIP() << "US Skies of Arcadia Legends dump is not present.";
    }

    const auto parsed = spice::ect::EctParser::parseFile(root / "field/a099a.ect");

    ASSERT_TRUE(parsed.ok());
    EXPECT_TRUE(parsed.sourceWasCompressedAklz);
    EXPECT_EQ(parsed.layout, spice::ect::EctLayout::IndexedContainer);
    EXPECT_GT(parsed.indexEntries.size(), 0U);
    EXPECT_GT(parsed.tables.size(), 0U);
}
