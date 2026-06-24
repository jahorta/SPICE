#include "../Compression/Aklz.h"
#include "../SpiceStd/SpiceStd.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

void writeBeU16(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint16_t value)
{
    bytes[offset + 0U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    bytes[offset + 1U] = static_cast<std::uint8_t>(value & 0xffU);
}

void writeBeS16(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::int16_t value)
{
    writeBeU16(bytes, offset, static_cast<std::uint16_t>(value));
}

void writeBeU32(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint32_t value)
{
    bytes[offset + 0U] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    bytes[offset + 3U] = static_cast<std::uint8_t>(value & 0xffU);
}

std::vector<std::uint8_t> makeActionRowsStd()
{
    std::vector<std::uint8_t> bytes(0x10U + 2U * 0x18U, 0U);
    writeBeU16(bytes, 0x00U, 0x0000U);
    writeBeU16(bytes, 0x02U, 0x0001U);
    writeBeU32(bytes, 0x04U, 0x11223344U);
    writeBeU32(bytes, 0x08U, 2U);
    writeBeU32(bytes, 0x0cU, 0U);

    std::size_t row = 0x10U;
    writeBeS16(bytes, row + 0x00U, 4);
    writeBeS16(bytes, row + 0x02U, 1);
    writeBeS16(bytes, row + 0x04U, 8);
    writeBeS16(bytes, row + 0x06U, 3);
    writeBeU32(bytes, row + 0x08U, 0x88000000U);
    writeBeS16(bytes, row + 0x0cU, -1);
    writeBeS16(bytes, row + 0x0eU, 14);
    writeBeU32(bytes, row + 0x10U, 0x40a00000U);
    writeBeU32(bytes, row + 0x14U, 0x3f800000U);

    row += 0x18U;
    writeBeS16(bytes, row + 0x00U, 0);
    writeBeS16(bytes, row + 0x02U, 0);
    writeBeS16(bytes, row + 0x04U, 7);
    writeBeS16(bytes, row + 0x06U, 0);
    writeBeU32(bytes, row + 0x08U, 0x04000000U);
    writeBeS16(bytes, row + 0x0cU, 2);
    writeBeS16(bytes, row + 0x0eU, 0);
    writeBeU32(bytes, row + 0x10U, 0U);
    writeBeU32(bytes, row + 0x14U, 0x3f800000U);

    return bytes;
}

std::vector<std::uint8_t> makeEntryTableStd()
{
    std::vector<std::uint8_t> bytes(0x50U, 0U);
    writeBeU16(bytes, 0x00U, 3U);
    writeBeU16(bytes, 0x02U, 4U);
    writeBeU32(bytes, 0x0cU, 0x40U);

    std::size_t entry = 0x10U;
    writeBeS16(bytes, entry + 0x00U, 4);
    writeBeS16(bytes, entry + 0x02U, 3);
    writeBeU32(bytes, entry + 0x04U, 0x12345678U);
    writeBeU32(bytes, entry + 0x08U, 4U);
    writeBeU32(bytes, entry + 0x0cU, 0x30U);

    entry += 0x10U;
    writeBeS16(bytes, entry + 0x00U, 5);
    writeBeS16(bytes, entry + 0x02U, 5);
    writeBeU32(bytes, entry + 0x08U, 8U);
    writeBeU32(bytes, entry + 0x0cU, 0x34U);

    entry += 0x10U;
    writeBeS16(bytes, entry + 0x00U, -1);

    bytes[0x40U] = 0xaaU;
    bytes[0x41U] = 0xbbU;
    bytes[0x42U] = 0xccU;
    bytes[0x43U] = 0xddU;
    for (std::size_t i = 0U; i < 8U; ++i) {
        bytes[0x44U + i] = static_cast<std::uint8_t>(0x10U + i);
    }
    return bytes;
}

} // namespace

TEST(SpiceStdParser, ParsesActionRowsAndExportsDecodedBytes)
{
    const auto bytes = makeActionRowsStd();
    const auto parsed = spice::stdfile::parseBytes(bytes, "ma000.std");

    ASSERT_TRUE(parsed.ok());
    EXPECT_EQ(parsed.sourceEncoding, spice::stdfile::StdSourceEncoding::Plain);
    EXPECT_EQ(parsed.layoutKind, spice::stdfile::StdLayoutKind::ActionRows);
    EXPECT_EQ(parsed.rawSize, static_cast<std::uint32_t>(bytes.size()));
    EXPECT_EQ(parsed.decodedSize, static_cast<std::uint32_t>(bytes.size()));
    EXPECT_EQ(parsed.actionRows.header.combinedCommandKind, 0x00010000U);
    EXPECT_EQ(parsed.actionRows.header.loaderContextWord, 0x11223344U);
    ASSERT_EQ(parsed.actionRows.rows.size(), 2U);
    EXPECT_EQ(parsed.actionRows.rows[0].actionId, 4);
    EXPECT_EQ(parsed.actionRows.rows[0].rowType, 1);
    EXPECT_EQ(parsed.actionRows.rows[0].callbackIndex, 8);
    EXPECT_EQ(parsed.actionRows.rows[0].motionSlotOrdinal, 3);
    EXPECT_EQ(parsed.actionRows.rows[0].flags, 0x88000000U);
    EXPECT_EQ(parsed.actionRows.rows[0].secondaryKey, -1);
    EXPECT_EQ(parsed.actionRows.rows[0].callbackAuxParam, 14);
    EXPECT_EQ(parsed.actionRows.rows[0].selectionTransitionScalarBits, 0x40a00000U);
    EXPECT_EQ(parsed.actionRows.rows[0].motionProgressScalarBits, 0x3f800000U);

    const auto exported = spice::stdfile::exportBytes(parsed, spice::stdfile::StdExportMode::DecodedBytes);
    ASSERT_TRUE(exported.ok());
    EXPECT_EQ(exported.bytes, bytes);
}

TEST(SpiceStdParser, ParsesEntryTableAndPreservesPayloadSpans)
{
    const auto bytes = makeEntryTableStd();
    const auto parsed = spice::stdfile::parseBytes(bytes, "ma0000.std");

    ASSERT_TRUE(parsed.ok());
    EXPECT_EQ(parsed.layoutKind, spice::stdfile::StdLayoutKind::EntryTable);
    EXPECT_EQ(parsed.entryTable.header.recordCountIncludingSentinel, 3U);
    EXPECT_EQ(parsed.entryTable.header.kind, 4U);
    EXPECT_EQ(parsed.entryTable.header.decodedSpanMinusHeader, 0x40U);
    EXPECT_TRUE(parsed.entryTable.hasSentinel);
    EXPECT_EQ(parsed.entryTable.sentinelIndex, 2U);
    EXPECT_EQ(parsed.entryTable.entryCountWithoutSentinel, 2U);
    EXPECT_TRUE(parsed.entryTable.hasPayloads);
    EXPECT_EQ(parsed.entryTable.firstPayloadOffsetRel, 0x30U);
    EXPECT_EQ(parsed.entryTable.maxPayloadEndRel, 0x3cU);
    EXPECT_EQ(parsed.entryTable.trailingBytesAfterMaxPayload, 4U);
    ASSERT_EQ(parsed.entryTable.records.size(), 3U);
    EXPECT_FALSE(parsed.entryTable.records[0].isSentinel);
    EXPECT_EQ(parsed.entryTable.records[0].combinedType, 0x00030004U);
    EXPECT_EQ(parsed.entryTable.records[0].field2, 0x12345678U);
    EXPECT_EQ(parsed.entryTable.records[0].payloadSize, 4U);
    EXPECT_EQ(parsed.entryTable.records[0].payloadOffsetRel, 0x30U);
    EXPECT_EQ(parsed.entryTable.records[0].payloadOffsetAbs, 0x40U);
    EXPECT_TRUE(parsed.entryTable.records[0].payloadInBounds);
    EXPECT_EQ(parsed.entryTable.records[1].combinedType, 0x00050005U);
    EXPECT_TRUE(parsed.entryTable.records[1].payloadInBounds);
    EXPECT_TRUE(parsed.entryTable.records[2].isSentinel);

    const auto exported = spice::stdfile::exportBytes(parsed, spice::stdfile::StdExportMode::DecodedBytes);
    ASSERT_TRUE(exported.ok());
    EXPECT_EQ(exported.bytes, bytes);
}

TEST(SpiceStdJsonExporter, EmitsActionRowsWithNamedFields)
{
    const auto bytes = makeActionRowsStd();
    const auto parsed = spice::stdfile::parseBytes(bytes, "ma000.std");

    ASSERT_TRUE(parsed.ok());
    const std::string json = spice::stdfile::StdJsonExporter{}.toJson(parsed);

    EXPECT_NE(json.find("\"schema\": \"spice_std_ir_v1\""), std::string::npos);
    EXPECT_NE(json.find("\"source\": \"ma000.std\""), std::string::npos);
    EXPECT_NE(json.find("\"layoutKind\": \"action_rows\""), std::string::npos);
    EXPECT_NE(json.find("\"actionRows\": {"), std::string::npos);
    EXPECT_NE(json.find("\"commandLow\": 0"), std::string::npos);
    EXPECT_NE(json.find("\"commandHigh\": 1"), std::string::npos);
    EXPECT_NE(json.find("\"combinedCommandKind\": 65536"), std::string::npos);
    EXPECT_NE(json.find("\"loaderContextWordHex\": \"0x11223344\""), std::string::npos);
    EXPECT_NE(json.find("\"rowTablePtrWordHex\": \"0x00000000\""), std::string::npos);
    EXPECT_NE(json.find("\"actionId\": 4"), std::string::npos);
    EXPECT_NE(json.find("\"rowType\": 1"), std::string::npos);
    EXPECT_NE(json.find("\"callbackIndex\": 8"), std::string::npos);
    EXPECT_NE(json.find("\"callbackOrdinal\": 3"), std::string::npos);
    EXPECT_NE(json.find("\"flagsHex\": \"0x88000000\""), std::string::npos);
    EXPECT_NE(json.find("\"secondaryKey\": -1"), std::string::npos);
    EXPECT_NE(json.find("\"callbackAuxParam\": 14"), std::string::npos);
    EXPECT_NE(json.find("\"transitionGateDivisorBits\": 1084227584"), std::string::npos);
    EXPECT_NE(json.find("\"motionProgressStepHex\": \"0x3f800000\""), std::string::npos);
    EXPECT_NE(json.find("\"entryTable\": null"), std::string::npos);
}

TEST(SpiceStdJsonExporter, EmitsEntryTableWithNamedFieldsAndPayloadBytes)
{
    const auto bytes = makeEntryTableStd();
    const auto parsed = spice::stdfile::parseBytes(bytes, "ma0000.std");

    ASSERT_TRUE(parsed.ok());
    const std::string json = spice::stdfile::StdJsonExporter{}.toJson(parsed);

    EXPECT_NE(json.find("\"schema\": \"spice_std_ir_v1\""), std::string::npos);
    EXPECT_NE(json.find("\"layoutKind\": \"entry_table\""), std::string::npos);
    EXPECT_NE(json.find("\"actionRows\": null"), std::string::npos);
    EXPECT_NE(json.find("\"entryTable\": {"), std::string::npos);
    EXPECT_NE(json.find("\"recordCountIncludingSentinel\": 3"), std::string::npos);
    EXPECT_NE(json.find("\"kind\": 4"), std::string::npos);
    EXPECT_NE(json.find("\"reserved0Hex\": \"0x00000000\""), std::string::npos);
    EXPECT_NE(json.find("\"reserved1Hex\": \"0x00000000\""), std::string::npos);
    EXPECT_NE(json.find("\"decodedSpanMinusHeader\": 64"), std::string::npos);
    EXPECT_NE(json.find("\"entryCountWithoutSentinel\": 2"), std::string::npos);
    EXPECT_NE(json.find("\"locationCode\": 4"), std::string::npos);
    EXPECT_NE(json.find("\"opcode\": 3"), std::string::npos);
    EXPECT_NE(json.find("\"combinedTypeHex\": \"0x00030004\""), std::string::npos);
    EXPECT_NE(json.find("\"field2Hex\": \"0x12345678\""), std::string::npos);
    EXPECT_NE(json.find("\"payloadSize\": 4"), std::string::npos);
    EXPECT_NE(json.find("\"payloadOffsetOrPtr\": 48"), std::string::npos);
    EXPECT_NE(json.find("\"payloadOffsetAbs\": 64"), std::string::npos);
    EXPECT_NE(json.find("\"payloadBytesHex\": \"aabbccdd\""), std::string::npos);
    EXPECT_NE(json.find("\"payloadBytesHex\": \"1011121314151617\""), std::string::npos);
    EXPECT_NE(json.find("\"isSentinel\": true"), std::string::npos);
}

TEST(SpiceStdParser, PreservesOriginalAklzBytesAndCanReencode)
{
    const auto decoded = makeActionRowsStd();
    const auto compressed = spice::compression::aklz::compress(decoded);
    ASSERT_TRUE(compressed.ok());

    const auto parsed = spice::stdfile::parseBytes(compressed.bytes, "ma000.std");
    ASSERT_TRUE(parsed.ok());
    EXPECT_EQ(parsed.sourceEncoding, spice::stdfile::StdSourceEncoding::Aklz);
    EXPECT_EQ(parsed.layoutKind, spice::stdfile::StdLayoutKind::ActionRows);
    EXPECT_EQ(parsed.rawBytes, compressed.bytes);
    EXPECT_EQ(parsed.decodedBytes, decoded);

    const auto original = spice::stdfile::exportBytes(parsed, spice::stdfile::StdExportMode::OriginalSourceBytes);
    ASSERT_TRUE(original.ok());
    EXPECT_EQ(original.bytes, compressed.bytes);

    const auto reencoded = spice::stdfile::exportBytes(parsed, spice::stdfile::StdExportMode::ReencodeSourceKind);
    ASSERT_TRUE(reencoded.ok());
    EXPECT_TRUE(spice::compression::aklz::isAklz(reencoded.bytes));
    const auto decodedAgain = spice::compression::aklz::decompress(reencoded.bytes);
    ASSERT_TRUE(decodedAgain.ok());
    EXPECT_EQ(decodedAgain.bytes, decoded);
}

TEST(SpiceStdParser, KeepsRawBytesAvailableWhenLayoutIsUnknown)
{
    std::vector<std::uint8_t> bytes(0x20U, 0U);
    bytes[0x1fU] = 0x7fU;

    const auto parsed = spice::stdfile::parseBytes(bytes, "unknown.std");
    EXPECT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.layoutKind, spice::stdfile::StdLayoutKind::Unknown);
    ASSERT_FALSE(parsed.diagnostics.empty());

    const auto exported = spice::stdfile::exportBytes(parsed, spice::stdfile::StdExportMode::OriginalSourceBytes);
    ASSERT_TRUE(exported.ok());
    EXPECT_EQ(exported.bytes, bytes);
}

TEST(SpiceStdParser, DoesNotReencodeWhenAklzDecodeFails)
{
    const std::vector<std::uint8_t> bytes{
        'A', 'K', 'L', 'Z', '~', '?', 'Q', 'd', '=', 0xccU, 0xccU, 0xcdU
    };

    const auto parsed = spice::stdfile::parseBytes(bytes, "broken.std");
    EXPECT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.sourceEncoding, spice::stdfile::StdSourceEncoding::Aklz);
    EXPECT_FALSE(parsed.decodedAvailable);

    const auto original = spice::stdfile::exportBytes(parsed, spice::stdfile::StdExportMode::OriginalSourceBytes);
    ASSERT_TRUE(original.ok());
    EXPECT_EQ(original.bytes, bytes);

    const auto decoded = spice::stdfile::exportBytes(parsed, spice::stdfile::StdExportMode::DecodedBytes);
    EXPECT_FALSE(decoded.ok());
    EXPECT_TRUE(decoded.bytes.empty());

    const auto reencoded = spice::stdfile::exportBytes(parsed, spice::stdfile::StdExportMode::ReencodeAklz);
    EXPECT_FALSE(reencoded.ok());
    EXPECT_TRUE(reencoded.bytes.empty());
}
