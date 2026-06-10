#include "../SoaSimMLD/SoaSimMLD.h"

#include <gtest/gtest.h>

#include <bit>
#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include <array>

namespace {

using spice::core::Endian;
using soasim::mld::exporting::MldExportOptions;
using soasim::mld::exporting::MldFileExporter;
using soasim::mld::model::TargetPlatform;
using soasim::mld::parsing::MldParser;

constexpr std::size_t kHeaderSize = 0x14U;
constexpr std::size_t kEntryOffset = 0x20U;
constexpr std::size_t kEntrySize = 0x68U;
constexpr std::size_t kListGroundLinks = 0x100U;
constexpr std::size_t kListParam2 = 0x108U;
constexpr std::size_t kListFunctionParams = 0x110U;
constexpr std::size_t kListObjects = 0x11CU;
constexpr std::size_t kListGrounds = 0x124U;
constexpr std::size_t kListMotions = 0x12CU;
constexpr std::size_t kGrndOffset = 0x140U;
constexpr std::size_t kTextureTable = 0x170U;

void writeU16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value, Endian endian) {
    if (endian == Endian::Big) {
        bytes[offset + 0U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
        bytes[offset + 1U] = static_cast<std::uint8_t>(value & 0xFFU);
    } else {
        bytes[offset + 0U] = static_cast<std::uint8_t>(value & 0xFFU);
        bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    }
}

void writeU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value, Endian endian) {
    if (endian == Endian::Big) {
        bytes[offset + 0U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
        bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
        bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
        bytes[offset + 3U] = static_cast<std::uint8_t>(value & 0xFFU);
    } else {
        bytes[offset + 0U] = static_cast<std::uint8_t>(value & 0xFFU);
        bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
        bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
        bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
    }
}

void writeF32(std::vector<std::uint8_t>& bytes, std::size_t offset, float value, Endian endian) {
    writeU32(bytes, offset, std::bit_cast<std::uint32_t>(value), endian);
}

void writeTag(std::vector<std::uint8_t>& bytes, std::size_t offset, const char* tag) {
    bytes[offset + 0U] = static_cast<std::uint8_t>(tag[0]);
    bytes[offset + 1U] = static_cast<std::uint8_t>(tag[1]);
    bytes[offset + 2U] = static_cast<std::uint8_t>(tag[2]);
    bytes[offset + 3U] = static_cast<std::uint8_t>(tag[3]);
}

void writeList(std::vector<std::uint8_t>& bytes, std::size_t offset, std::span<const std::uint32_t> values, Endian endian) {
    writeU32(bytes, offset, static_cast<std::uint32_t>(values.size()), endian);
    for (std::size_t i = 0; i < values.size(); ++i) {
        writeU32(bytes, offset + 4U + (i * 4U), values[i], endian);
    }
}

std::vector<std::uint8_t> makeMinimalMld(Endian endian) {
    std::vector<std::uint8_t> bytes(0x180U, 0U);
    writeU32(bytes, 0x00U, 1U, endian);
    writeU32(bytes, 0x04U, static_cast<std::uint32_t>(kEntryOffset), endian);
    writeU32(bytes, 0x08U, static_cast<std::uint32_t>(kListFunctionParams), endian);
    writeU32(bytes, 0x0CU, static_cast<std::uint32_t>(kGrndOffset), endian);
    writeU32(bytes, 0x10U, static_cast<std::uint32_t>(kTextureTable), endian);

    writeU32(bytes, kEntryOffset + 0x00U, 0x101U, endian);
    writeU32(bytes, kEntryOffset + 0x04U, 0x202U, endian);
    writeU32(bytes, kEntryOffset + 0x08U, static_cast<std::uint32_t>(kListGroundLinks), endian);
    writeU32(bytes, kEntryOffset + 0x0CU, static_cast<std::uint32_t>(kListParam2), endian);
    writeU32(bytes, kEntryOffset + 0x10U, static_cast<std::uint32_t>(kListFunctionParams), endian);
    writeU32(bytes, kEntryOffset + 0x14U, static_cast<std::uint32_t>(kListObjects), endian);
    writeU32(bytes, kEntryOffset + 0x18U, static_cast<std::uint32_t>(kListGrounds), endian);
    writeU32(bytes, kEntryOffset + 0x1CU, static_cast<std::uint32_t>(kListMotions), endian);
    writeU32(bytes, kEntryOffset + 0x20U, 0U, endian);
    const std::string fxn = "wall";
    std::copy(fxn.begin(), fxn.end(), bytes.begin() + static_cast<std::ptrdiff_t>(kEntryOffset + 0x24U));
    writeF32(bytes, kEntryOffset + 0x44U, 1.0F, endian);
    writeF32(bytes, kEntryOffset + 0x48U, 2.0F, endian);
    writeF32(bytes, kEntryOffset + 0x4CU, 3.0F, endian);
    writeF32(bytes, kEntryOffset + 0x50U, 4.0F, endian);
    writeF32(bytes, kEntryOffset + 0x54U, 5.0F, endian);
    writeF32(bytes, kEntryOffset + 0x58U, 6.0F, endian);
    writeF32(bytes, kEntryOffset + 0x5CU, 1.0F, endian);
    writeF32(bytes, kEntryOffset + 0x60U, 1.0F, endian);
    writeF32(bytes, kEntryOffset + 0x64U, 1.0F, endian);

    const std::uint32_t groundLinks[] = { 7U };
    const std::uint32_t functionParams[] = { 0x333U, 0x444U };
    const std::uint32_t grounds[] = { static_cast<std::uint32_t>(kGrndOffset) };
    const std::array<std::uint32_t, 0> empty{};
    writeList(bytes, kListGroundLinks, groundLinks, endian);
    writeList(bytes, kListParam2, empty, endian);
    writeList(bytes, kListFunctionParams, functionParams, endian);
    writeList(bytes, kListObjects, empty, endian);
    writeList(bytes, kListGrounds, grounds, endian);
    writeList(bytes, kListMotions, empty, endian);

    writeTag(bytes, kGrndOffset, "GRND");
    writeU32(bytes, kGrndOffset + 4U, 0x2CU, endian);
    writeU32(bytes, kGrndOffset + 0x10U, 0, endian);
    writeU32(bytes, kGrndOffset + 0x14U, 0, endian);
    writeU16(bytes, kGrndOffset + 0x20U, 2U, endian);
    writeU16(bytes, kGrndOffset + 0x22U, 3U, endian);
    writeU16(bytes, kGrndOffset + 0x24U, 4U, endian);
    writeU16(bytes, kGrndOffset + 0x26U, 5U, endian);
    writeU16(bytes, kGrndOffset + 0x28U, 0U, endian);
    writeU16(bytes, kGrndOffset + 0x2AU, 0U, endian);

    writeU32(bytes, kTextureTable, 0U, endian);
    return bytes;
}

} // namespace

TEST(MldEndian, ParsesBigAndLittleEndianFixturesToEquivalentIr) {
    MldParser parser;
    const auto be = parser.parseFile(makeMinimalMld(Endian::Big));
    const auto le = parser.parseFile(makeMinimalMld(Endian::Little));

    ASSERT_EQ(be.entries.size(), 1U);
    ASSERT_EQ(le.entries.size(), 1U);
    EXPECT_EQ(be.endian, Endian::Big);
    EXPECT_EQ(le.endian, Endian::Little);
    EXPECT_EQ(be.header.entryCount, le.header.entryCount);
    EXPECT_EQ(be.entries[0].entry.entryId, le.entries[0].entry.entryId);
    EXPECT_EQ(be.entries[0].entry.fxnName, le.entries[0].entry.fxnName);
    EXPECT_FLOAT_EQ(be.entries[0].entry.transform.position.y, le.entries[0].entry.transform.position.y);
    ASSERT_TRUE(be.entries[0].entry.groundAddresses);
    ASSERT_TRUE(le.entries[0].entry.groundAddresses);
    EXPECT_EQ(be.entries[0].entry.groundAddresses->values, le.entries[0].entry.groundAddresses->values);
}

TEST(MldEndian, ExportGameCubeToDreamcastPreservesSemanticIrAndFourCcBytes) {
    MldParser parser;
    const auto be = parser.parseFile(makeMinimalMld(Endian::Big));
    const auto out = MldFileExporter{}.exportFile(be, MldExportOptions{ .platform = TargetPlatform::Dreamcast });
    ASSERT_EQ(out[kGrndOffset + 0U], 'G');
    ASSERT_EQ(out[kGrndOffset + 1U], 'R');
    ASSERT_EQ(out[kGrndOffset + 2U], 'N');
    ASSERT_EQ(out[kGrndOffset + 3U], 'D');
    EXPECT_EQ(out[0], 0x01U);
    EXPECT_EQ(out[1], 0x00U);
    EXPECT_EQ(out[kGrndOffset + 4U], 0x2CU);
    EXPECT_EQ(out[kGrndOffset + 5U], 0x00U);

    const auto le = parser.parseFile(out);
    ASSERT_EQ(le.entries.size(), 1U);
    EXPECT_EQ(le.endian, Endian::Little);
    EXPECT_EQ(le.entries[0].entry.entryId, be.entries[0].entry.entryId);
    EXPECT_EQ(le.entries[0].entry.tblId, be.entries[0].entry.tblId);
    EXPECT_EQ(le.entries[0].entry.functionParameters->values, be.entries[0].entry.functionParameters->values);
    EXPECT_FLOAT_EQ(le.entries[0].entry.transform.position.x, be.entries[0].entry.transform.position.x);
}

TEST(MldEndian, AutoDetectChoosesSmallerPlausibleEntryCount) {
    std::vector<std::uint8_t> bytes(7U * 1024U * 1024U, 0U);
    bytes[0] = 0x00U;
    bytes[1] = 0x01U;
    bytes[2] = 0x00U;
    bytes[3] = 0x00U;
    bytes[4] = 0x00U;
    bytes[5] = 0x00U;
    bytes[6] = 0x20U;
    bytes[7] = 0x00U;
    bytes[0x10] = 0xFFU;
    bytes[0x11] = 0xFFU;
    bytes[0x12] = 0xFFU;
    bytes[0x13] = 0xFFU;

    MldParser parser;
    const auto parsed = parser.parseFile(bytes);
    EXPECT_EQ(parsed.endian, Endian::Little);
    EXPECT_EQ(parsed.header.entryCount, 256U);
    EXPECT_EQ(parsed.header.indexTableOffset, 0x200000U);
}
