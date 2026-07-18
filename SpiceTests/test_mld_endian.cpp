#include "../SpiceMLD/SpiceMLD.h"
#include "../SpiceMLD/Parsing/GobjParser.h"
#include "../SpiceMLD/Parsing/GrndParser.h"
#include "../SpiceGvm/SpiceGvm.h"
#include "../Compression/Aklz.h"

#include <gtest/gtest.h>

#include <bit>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <array>

namespace {

using spice::core::Endian;
using spice::mld::exporting::MldExportOptions;
using spice::mld::exporting::MldFileExporter;
using spice::mld::exporting::MldFileWriter;
using spice::mld::model::TargetPlatform;
using spice::mld::parsing::MldParser;

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

std::vector<std::uint8_t> makeSyntheticGobj(
    const std::uint8_t chunkType,
    const std::uint8_t recordWords,
    const bool hasNormal,
    const bool hasUserAttributes) {
    constexpr std::size_t nodeOffset = 0x10U;
    constexpr std::size_t attachOffset = 0x50U;
    constexpr std::size_t payloadOffset = attachOffset + 0x10U;
    constexpr std::size_t polyOffset = payloadOffset + 76U;
    constexpr std::size_t vertexOffset = 0xBCU;
    constexpr std::size_t vertexCount = 3U;
    const std::size_t declaredSize = vertexOffset + 8U + (vertexCount * recordWords * 4U);

    std::vector<std::uint8_t> bytes(declaredSize, 0U);
    writeTag(bytes, 0U, "GOBJ");
    writeU32(bytes, 4U, static_cast<std::uint32_t>(declaredSize), Endian::Big);
    writeU32(bytes, nodeOffset, static_cast<std::uint32_t>(attachOffset - nodeOffset), Endian::Big);
    writeF32(bytes, nodeOffset + 0x20U, 1.0F, Endian::Big);
    writeF32(bytes, nodeOffset + 0x24U, 1.0F, Endian::Big);
    writeF32(bytes, nodeOffset + 0x28U, 1.0F, Endian::Big);

    writeU32(bytes, payloadOffset, static_cast<std::uint32_t>(vertexOffset - payloadOffset), Endian::Big);
    constexpr std::array<std::uint16_t, 3> flags{ 0x0001U, 0x0002U, 0x8003U };
    for (std::size_t i = 0; i < vertexCount; ++i) {
        writeU16(bytes, polyOffset + (i * 4U), static_cast<std::uint16_t>(2U + (i * recordWords)), Endian::Big);
        writeU16(bytes, polyOffset + (i * 4U) + 2U, flags[i], Endian::Big);
    }
    writeU16(bytes, polyOffset + 12U, 0xFFFFU, Endian::Big);
    writeU16(bytes, polyOffset + 14U, 0xFFFFU, Endian::Big);

    writeU32(bytes, vertexOffset, chunkType, Endian::Big);
    writeU32(bytes, vertexOffset + 4U, static_cast<std::uint32_t>(vertexCount << 16U), Endian::Big);
    constexpr std::array<std::uint32_t, 3> userAttributes{ 0xFFFFFFFFU, 0x12345678U, 0x00000000U };
    for (std::size_t i = 0; i < vertexCount; ++i) {
        const std::size_t recordOffset = vertexOffset + 8U + (i * recordWords * 4U);
        writeF32(bytes, recordOffset + 0U, static_cast<float>(i), Endian::Big);
        writeF32(bytes, recordOffset + 4U, static_cast<float>(i + 1U), Endian::Big);
        writeF32(bytes, recordOffset + 8U, static_cast<float>(i + 2U), Endian::Big);
        if (hasNormal) {
            writeF32(bytes, recordOffset + 12U, 0.0F, Endian::Big);
            writeF32(bytes, recordOffset + 16U, 1.0F, Endian::Big);
            writeF32(bytes, recordOffset + 20U, 0.0F, Endian::Big);
        }
        if (hasUserAttributes) {
            writeU32(bytes, recordOffset + 24U, userAttributes[i], Endian::Big);
        }
    }
    return bytes;
}

std::vector<std::uint8_t> makeSyntheticGrnd() {
    constexpr std::size_t innerHeader = 0x10U;
    constexpr std::size_t triangleSetsOffset = 0x40U;
    constexpr std::size_t streamOffset = 0x60U;
    constexpr std::size_t vertexOffset = 0x80U;
    constexpr std::size_t quadRegistryOffset = 0xC8U;
    constexpr std::size_t quadTableOffset = quadRegistryOffset + 4U;
    constexpr std::size_t refListOffset = 0xDCU;
    constexpr std::size_t declaredSize = 0xE0U;

    std::vector<std::uint8_t> bytes(declaredSize, 0U);
    writeTag(bytes, 0U, "GRND");
    writeU32(bytes, 4U, static_cast<std::uint32_t>(declaredSize), Endian::Big);
    writeU32(bytes, innerHeader, static_cast<std::uint32_t>(triangleSetsOffset - innerHeader), Endian::Big);
    writeU32(bytes, innerHeader + 4U, static_cast<std::uint32_t>(quadRegistryOffset - innerHeader), Endian::Big);
    writeU16(bytes, innerHeader + 0x10U, 1U, Endian::Big);
    writeU16(bytes, innerHeader + 0x12U, 1U, Endian::Big);
    writeU16(bytes, innerHeader + 0x14U, 1U, Endian::Big);
    writeU16(bytes, innerHeader + 0x16U, 1U, Endian::Big);
    writeU16(bytes, innerHeader + 0x18U, 1U, Endian::Big);
    writeU16(bytes, innerHeader + 0x1AU, 1U, Endian::Big);

    writeU32(bytes, triangleSetsOffset + 0x0CU,
        static_cast<std::uint32_t>(vertexOffset - (triangleSetsOffset + 0x0CU)), Endian::Big);
    writeU32(bytes, triangleSetsOffset + 0x10U,
        static_cast<std::uint32_t>(streamOffset - (triangleSetsOffset + 0x10U)), Endian::Big);
    writeU32(bytes, triangleSetsOffset + 0x14U, 1U, Endian::Big);

    constexpr std::array<std::uint16_t, 3> floatIndices{ 0U, 6U, 12U };
    constexpr std::array<std::uint16_t, 3> flags{ 0x0001U, 0x7FFFU, 0x800AU };
    for (std::size_t i = 0; i < 3U; ++i) {
        writeU16(bytes, streamOffset + (i * 4U), floatIndices[i], Endian::Big);
        writeU16(bytes, streamOffset + (i * 4U) + 2U, flags[i], Endian::Big);
        const std::size_t recordOffset = vertexOffset + (i * 24U);
        writeF32(bytes, recordOffset + 0U, static_cast<float>(i), Endian::Big);
        writeF32(bytes, recordOffset + 4U, static_cast<float>(i == 1U), Endian::Big);
        writeF32(bytes, recordOffset + 8U, static_cast<float>(i == 2U), Endian::Big);
        writeF32(bytes, recordOffset + 12U, 0.0F, Endian::Big);
        writeF32(bytes, recordOffset + 16U, 1.0F, Endian::Big);
        writeF32(bytes, recordOffset + 20U, 0.0F, Endian::Big);
    }

    writeU32(bytes, quadTableOffset, 1U, Endian::Big);
    writeU32(bytes, quadTableOffset + 4U,
        static_cast<std::uint32_t>(refListOffset - (quadTableOffset + 4U)), Endian::Big);
    writeU16(bytes, refListOffset, 0U, Endian::Big);
    writeU16(bytes, refListOffset + 2U, 0U, Endian::Big);
    return bytes;
}

void appendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value, Endian endian) {
    const auto offset = bytes.size();
    bytes.resize(offset + 4U);
    writeU32(bytes, offset, value, endian);
}

void appendNameRecord(std::vector<std::uint8_t>& bytes, const std::string& name) {
    const auto offset = bytes.size();
    bytes.resize(offset + 44U, 0U);
    const auto count = std::min<std::size_t>(name.size(), 31U);
    std::copy_n(name.begin(), count, bytes.begin() + static_cast<std::ptrdiff_t>(offset));
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

void writeSa3dPointer(std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint32_t modelBlockRelativeOffset) {
    writeU32(bytes, offset, modelBlockRelativeOffset - 0x08U, Endian::Little);
}

std::vector<std::uint8_t> makeWrappedObjectMldWithTriangleGeometry() {
    constexpr std::uint32_t kObjectWrapperOffset = 0x180U;
    constexpr std::uint32_t kModelRelativeOffset = 0x40U;
    constexpr std::uint32_t kModelBlockOffset = kObjectWrapperOffset + kModelRelativeOffset;
    constexpr std::uint32_t kNodeRelativeOffset = 0x08U;
    constexpr std::uint32_t kAttachRelativeOffset = 0x48U;
    constexpr std::uint32_t kVertexChunkRelativeOffset = 0x78U;
    constexpr std::uint32_t kPolyChunkRelativeOffset = 0xC8U;
    constexpr std::uint32_t kNodeOffset = kModelBlockOffset + kNodeRelativeOffset;
    constexpr std::uint32_t kAttachOffset = kModelBlockOffset + kAttachRelativeOffset;
    constexpr std::uint32_t kVertexChunkOffset = kModelBlockOffset + kVertexChunkRelativeOffset;
    constexpr std::uint32_t kPolyChunkOffset = kModelBlockOffset + kPolyChunkRelativeOffset;

    auto bytes = makeMinimalMld(Endian::Big);
    bytes.resize(0x300U, 0U);

    const std::uint32_t objects[] = { kObjectWrapperOffset };
    writeList(bytes, kListObjects, objects, Endian::Big);

    writeU32(bytes, kObjectWrapperOffset, kModelRelativeOffset, Endian::Big);
    writeTag(bytes, kModelBlockOffset, "NJCM");
    writeU32(bytes, kModelBlockOffset + 0x04U, 0xF0U, Endian::Little);

    writeU32(bytes, kNodeOffset + 0x00U, 0U, Endian::Little);
    writeSa3dPointer(bytes, kNodeOffset + 0x04U, kAttachRelativeOffset);
    writeF32(bytes, kNodeOffset + 0x08U, 1.0F, Endian::Little);
    writeF32(bytes, kNodeOffset + 0x0CU, 2.0F, Endian::Little);
    writeF32(bytes, kNodeOffset + 0x10U, 3.0F, Endian::Little);
    writeU32(bytes, kNodeOffset + 0x14U, 0U, Endian::Little);
    writeU32(bytes, kNodeOffset + 0x18U, 0U, Endian::Little);
    writeU32(bytes, kNodeOffset + 0x1CU, 0U, Endian::Little);
    writeF32(bytes, kNodeOffset + 0x20U, 1.0F, Endian::Little);
    writeF32(bytes, kNodeOffset + 0x24U, 1.0F, Endian::Little);
    writeF32(bytes, kNodeOffset + 0x28U, 1.0F, Endian::Little);
    writeU32(bytes, kNodeOffset + 0x2CU, 0U, Endian::Little);
    writeU32(bytes, kNodeOffset + 0x30U, 0U, Endian::Little);

    writeSa3dPointer(bytes, kAttachOffset + 0x00U, kVertexChunkRelativeOffset);
    writeSa3dPointer(bytes, kAttachOffset + 0x04U, kPolyChunkRelativeOffset);
    writeF32(bytes, kAttachOffset + 0x08U, 0.0F, Endian::Little);
    writeF32(bytes, kAttachOffset + 0x0CU, 0.0F, Endian::Little);
    writeF32(bytes, kAttachOffset + 0x10U, 0.0F, Endian::Little);
    writeF32(bytes, kAttachOffset + 0x14U, 8.0F, Endian::Little);

    writeU32(bytes, kVertexChunkOffset + 0x00U, 0x00000022U, Endian::Little);
    writeU32(bytes, kVertexChunkOffset + 0x04U, 0x00030000U, Endian::Little);
    writeF32(bytes, kVertexChunkOffset + 0x08U, 0.0F, Endian::Little);
    writeF32(bytes, kVertexChunkOffset + 0x0CU, 0.0F, Endian::Little);
    writeF32(bytes, kVertexChunkOffset + 0x10U, 0.0F, Endian::Little);
    writeF32(bytes, kVertexChunkOffset + 0x14U, 1.0F, Endian::Little);
    writeF32(bytes, kVertexChunkOffset + 0x18U, 0.0F, Endian::Little);
    writeF32(bytes, kVertexChunkOffset + 0x1CU, 0.0F, Endian::Little);
    writeF32(bytes, kVertexChunkOffset + 0x20U, 0.0F, Endian::Little);
    writeF32(bytes, kVertexChunkOffset + 0x24U, 1.0F, Endian::Little);
    writeF32(bytes, kVertexChunkOffset + 0x28U, 0.0F, Endian::Little);
    writeU32(bytes, kVertexChunkOffset + 0x2CU, 0x000000FFU, Endian::Little);
    writeU32(bytes, kVertexChunkOffset + 0x30U, 0U, Endian::Little);

    writeU16(bytes, kPolyChunkOffset + 0x00U, 0x0040U, Endian::Little);
    writeU16(bytes, kPolyChunkOffset + 0x02U, 5U, Endian::Little);
    writeU16(bytes, kPolyChunkOffset + 0x04U, 1U, Endian::Little);
    writeU16(bytes, kPolyChunkOffset + 0x06U, 3U, Endian::Little);
    writeU16(bytes, kPolyChunkOffset + 0x08U, 0U, Endian::Little);
    writeU16(bytes, kPolyChunkOffset + 0x0AU, 1U, Endian::Little);
    writeU16(bytes, kPolyChunkOffset + 0x0CU, 2U, Endian::Little);
    writeU16(bytes, kPolyChunkOffset + 0x0EU, 0x00FFU, Endian::Little);

    return bytes;
}

spice::gvm::model::RgbaImage makeImage(std::uint32_t width, std::uint32_t height, std::uint8_t bias = 0U) {
    spice::gvm::model::RgbaImage image{};
    image.width = width;
    image.height = height;
    image.rgba8.resize(static_cast<std::size_t>(width) * height * 4U);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * width + x) * 4U;
            image.rgba8[offset + 0U] = static_cast<std::uint8_t>((x * 17U + bias) & 0xFFU);
            image.rgba8[offset + 1U] = static_cast<std::uint8_t>((y * 19U + bias) & 0xFFU);
            image.rgba8[offset + 2U] = static_cast<std::uint8_t>(((x + y) * 13U + bias) & 0xFFU);
            image.rgba8[offset + 3U] = 0xFFU;
        }
    }
    return image;
}

std::filesystem::path testOutDir(const char* name) {
    auto dir = std::filesystem::temp_directory_path() / "spice_mld_endian_tests" / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

std::vector<std::uint8_t> encodeTexture(
    const spice::gvm::model::RgbaImage& image,
    spice::gvm::model::TextureFormat format,
    spice::gvm::model::PaletteFormat palette = spice::gvm::model::PaletteFormat::None,
    bool hasGlobalIndex = false,
    std::uint32_t globalIndex = 0U) {
    spice::gvm::encoding::EncodeOptions options{};
    options.textureFormat = format;
    options.paletteFormat = palette;
    options.hasGlobalIndex = hasGlobalIndex;
    options.globalIndex = globalIndex;
    return spice::gvm::encoding::encodeGvr(image, options);
}

std::vector<std::uint8_t> makeTexturedMld(
    const std::vector<std::uint8_t>& firstTexture,
    const std::vector<std::uint8_t>& secondTexture,
    bool addSuffix = false) {
    auto bytes = makeMinimalMld(Endian::Big);
    bytes.resize(kTextureTable);
    appendU32(bytes, 2U, Endian::Big);
    appendNameRecord(bytes, "tex_a");
    appendNameRecord(bytes, "tex_b");
    bytes.insert(bytes.end(), firstTexture.begin(), firstTexture.end());
    bytes.insert(bytes.end(), secondTexture.begin(), secondTexture.end());
    if (addSuffix) {
        bytes.push_back('T');
        bytes.push_back('A');
        bytes.push_back('I');
        bytes.push_back('L');
    }
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

TEST(MldEndian, PreservesFullMotionAddressSlotListWithZeroEntries) {
    auto bytes = makeMinimalMld(Endian::Big);
    bytes.resize(0x220U, 0U);
    const std::vector<std::uint32_t> motions{ 0U, 0x180U, 0U, 0x1C0U };
    writeList(bytes, kListMotions, motions, Endian::Big);
    writeTag(bytes, 0x180U, "NJCM");
    writeTag(bytes, 0x1C0U, "NJCM");

    MldParser parser;
    const auto parsedFile = parser.parseFile(bytes);
    ASSERT_EQ(parsedFile.entries.size(), 1U);
    ASSERT_TRUE(parsedFile.entries[0].entry.motionAddresses);
    EXPECT_EQ(parsedFile.entries[0].entry.motionAddresses->values, motions);
    EXPECT_EQ(parsedFile.entries[0].entry.motionCount, 2U);

    spice::mld::parsing::ParseOptions options{};
    options.buildBlenderIntermediateIr = false;
    const auto parsed = parser.parse(bytes, options);
    ASSERT_EQ(parsed.entryList.size(), 1U);
    EXPECT_EQ(parsed.entryList[0].motionAddresses, motions);
    EXPECT_EQ(parsed.entryList[0].motionCount, 2U);
    ASSERT_EQ(parsed.rawEntries.size(), 1U);
    EXPECT_EQ(parsed.rawEntries[0].motionAddresses, motions);
}

TEST(MldParser, ResolvesWrappedObjectModelOffsetBeforeFallbackScan) {
    auto bytes = makeMinimalMld(Endian::Big);
    bytes.resize(0x300U, 0U);
    const std::uint32_t objects[] = { 0x180U };
    writeList(bytes, kListObjects, objects, Endian::Big);

    writeU32(bytes, 0x180U, 0x40U, Endian::Big);
    writeU32(bytes, 0x184U, 0x80U, Endian::Big);
    writeU32(bytes, 0x188U, 0x10U, Endian::Big);
    writeTag(bytes, 0x190U, "NJTL");
    writeU32(bytes, 0x194U, 0x20U, Endian::Big);
    writeU32(bytes, 0x19CU, 0U, Endian::Big);
    writeTag(bytes, 0x1A0U, "NJCM");
    writeTag(bytes, 0x1C0U, "NJCM");

    MldParser parser;
    spice::mld::parsing::ParseOptions options{};
    options.buildBlenderIntermediateIr = false;
    const auto blocks = parser.extractNjBlocks(bytes, options);

    const auto found = std::find_if(blocks.begin(), blocks.end(), [](const auto& block) {
        return block.sourceObjectAddress == 0x180U;
    });
    ASSERT_NE(found, blocks.end());
    ASSERT_TRUE(found->modelBlockOffset.has_value());
    ASSERT_TRUE(found->modelReadOffset.has_value());
    ASSERT_TRUE(found->textureListOffset.has_value());
    EXPECT_EQ(*found->modelBlockOffset, 0x1C0U);
    EXPECT_EQ(*found->modelReadOffset, 0x40U);
    EXPECT_EQ(*found->textureListOffset, 0x190U);
    EXPECT_EQ(found->wrapperLayout, "mld-object-wrapper");
}

TEST(MldParser, ParsesSyntheticWrappedMldObjectIntoBlenderIrGeometry) {
    const auto bytes = makeWrappedObjectMldWithTriangleGeometry();

    MldParser parser;
    const auto parsed = parser.parse(bytes);

    ASSERT_EQ(parsed.entryList.size(), 1U);
    ASSERT_EQ(parsed.entryList[0].objectAddresses.size(), 1U);
    EXPECT_EQ(parsed.entryList[0].objectAddresses[0], 0x180U);

    const auto found = std::find_if(parsed.extractedNjBlocks.begin(), parsed.extractedNjBlocks.end(), [](const auto& block) {
        return block.sourceObjectAddress == 0x180U;
    });
    ASSERT_NE(found, parsed.extractedNjBlocks.end());
    ASSERT_TRUE(found->modelBlockOffset.has_value());
    ASSERT_TRUE(found->modelReadOffset.has_value());
    EXPECT_EQ(*found->modelBlockOffset, 0x1C0U);
    EXPECT_EQ(*found->modelReadOffset, 0x40U);
    EXPECT_EQ(found->wrapperLayout, "mld-object-wrapper");

    ASSERT_TRUE(parsed.blenderIrScene.has_value());
    EXPECT_GT(parsed.blenderIrScene->objectTrees.size(), 0U);
    EXPECT_GT(parsed.blenderIrScene->meshes.size(), 0U);
    ASSERT_EQ(parsed.blenderIrScene->indexEntries.size(), 1U);
    EXPECT_FALSE(parsed.blenderIrScene->indexEntries[0].objectTreeIndices.empty());
    EXPECT_FALSE(parsed.blenderIrScene->indexEntries[0].meshIndices.empty());
}

TEST(BlenderIrJsonExporter, EmitsWeightedMeshBinding) {
    spice::mld::model::BlenderIrScene scene{};
    spice::mld::model::BlenderIrMesh mesh{};
    mesh.label = "weighted";
    mesh.sourceObjectAddress = 0x1000;
    mesh.sourceChunkOffset = 0x2000;
    mesh.sourceAttachOffset = 0x3000;
    mesh.weightedBinding = spice::mld::model::BlenderIrWeightedBinding{
        .rootNodeIndex = 4,
        .sourceNodeIndex = 7,
        .nodeIndices = {4, 7},
    };

    spice::mld::model::BlenderIrVertex vertex{};
    vertex.hasPosition = true;
    vertex.hasNormal = true;
    vertex.weights.push_back({.boneOrNodeIndex = 4, .weight = 0.25f});
    vertex.weights.push_back({.boneOrNodeIndex = 7, .weight = 0.75f});
    mesh.vertices.push_back(std::move(vertex));

    spice::mld::model::BlenderIrMaterial material{};
    material.materialHash = 1;
    mesh.materials.push_back(material);

    spice::mld::model::BlenderIrTriangleSet triangles{};
    triangles.corners.resize(3);
    mesh.triangleSets.push_back(std::move(triangles));
    scene.meshes.push_back(std::move(mesh));

    const auto json = spice::mld::exporting::BlenderIrJsonExporter{}.toJson(scene);
    EXPECT_NE(json.find("\"weightedBinding\":{\"rootNodeIndex\":4,\"sourceNodeIndex\":7,\"nodeIndices\":[4,7]}"), std::string::npos);
    EXPECT_NE(json.find("\"weights\":[{\"boneOrNodeIndex\":4,\"weight\":0.25},{\"boneOrNodeIndex\":7,\"weight\":0.75}]"), std::string::npos);
}

TEST(BlenderIrJsonExporter, EmitsTblIdAsSignedDecimalNumber) {
    spice::mld::model::BlenderIrScene scene{};
    spice::mld::model::BlenderIrInstance instance{};
    instance.sourceEntryId = 1;
    instance.tableIndex = 0;
    instance.tblId = -42;
    instance.fxnName = "wall";
    scene.indexEntries.push_back(std::move(instance));

    const auto json = spice::mld::exporting::BlenderIrJsonExporter{}.toJson(scene);
    EXPECT_NE(json.find("\"tblId\":-42"), std::string::npos);
    EXPECT_EQ(json.find("\"tblId\":\"-42\""), std::string::npos);
    EXPECT_EQ(json.find("\"tblId\":0x"), std::string::npos);
}

TEST(BlenderIrJsonExporter, EmitsFunctionParametersAsUnsignedDecimalNumbers) {
    spice::mld::model::BlenderIrScene scene{};
    spice::mld::model::BlenderIrInstance instance{};
    instance.functionParameters = {0U, 0xFFFFFFFFU, 0x12345678U};
    scene.indexEntries.push_back(std::move(instance));

    const auto json = spice::mld::exporting::BlenderIrJsonExporter{}.toJson(scene);
    EXPECT_NE(
        json.find("\"functionParameters\":[0,4294967295,305419896]"),
        std::string::npos);
    EXPECT_EQ(json.find("\"functionParameters\":[0x"), std::string::npos);
    EXPECT_EQ(json.find("\"functionParameters\":[-1"), std::string::npos);
}

TEST(Sa3dBlenderIrBuilder, PreservesParsedFunctionParameters) {
    spice::mld::parsing::ParseResult parsed{};
    spice::mld::parsing::ParsedRawEntry entry{};
    entry.sourceEntryId = 9U;
    entry.functionParameters = {0U, 0xFFFFFFFFU, 0x12345678U};
    parsed.rawEntries.push_back(std::move(entry));

    const auto scene = spice::mld::parsing::Sa3dBlenderIrBuilder{}.build(parsed);
    ASSERT_EQ(scene.indexEntries.size(), 1U);
    EXPECT_EQ(
        scene.indexEntries[0].functionParameters,
        (std::vector<std::uint32_t>{0U, 0xFFFFFFFFU, 0x12345678U}));
}

TEST(BlenderIrJsonExporter, EmitsValueBearingAnimationChannels) {
    spice::mld::model::BlenderIrScene scene{};
    spice::mld::model::BlenderIrAnimation animation{};
    animation.sourceEntryId = 4;
    animation.tableIndex = 0;
    animation.sourceObjectAddress = 0x1000;
    animation.sourceMotionAddress = 0x2000;
    animation.motionSlot = 3;
    animation.objectTreeIndex = 1;
    animation.nodeCount = 2;
    animation.frameCount = 23;
    animation.interpolationMode = "linear";

    spice::mld::model::BlenderIrAnimationChannel roll{};
    roll.nodeIndex = 1;
    roll.channel = "roll";
    roll.valueType = "float";
    roll.floatValues.push_back({.frame = 7, .value = 1.25f});
    animation.channels.push_back(std::move(roll));

    spice::mld::model::BlenderIrAnimationChannel vertex{};
    vertex.nodeIndex = 1;
    vertex.channel = "vertex";
    vertex.valueType = "vec3Array";
    vertex.vectorArrayValues.push_back({
        .frame = 9,
        .label = "shape-a",
        .values = {{.x = 1.0f, .y = 2.0f, .z = 3.0f}},
    });
    animation.channels.push_back(std::move(vertex));
    scene.animations.push_back(std::move(animation));

    const auto json = spice::mld::exporting::BlenderIrJsonExporter{}.toJson(scene);
    EXPECT_NE(json.find("\"channels\":[{\"nodeIndex\":1,\"channel\":\"roll\",\"valueType\":\"float\",\"keyframes\":[{\"frame\":7,\"value\":1.25}]"), std::string::npos);
    EXPECT_NE(json.find("\"channel\":\"vertex\",\"valueType\":\"vec3Array\""), std::string::npos);
    EXPECT_NE(json.find("\"frame\":9,\"label\":\"shape-a\",\"values\":[[1,2,3]]"), std::string::npos);
}

TEST(MldEndian, ParsesTblIdAsSignedAndExportsOriginalBits) {
    auto bytes = makeMinimalMld(Endian::Big);
    writeU32(bytes, kEntryOffset + 0x04U, 0xFFFFFFFEU, Endian::Big);

    MldParser parser;
    const auto parsed = parser.parseFile(bytes);
    ASSERT_EQ(parsed.entries.size(), 1U);
    EXPECT_EQ(parsed.entries[0].entry.tblId, -2);

    const auto exported = MldFileExporter{}.exportFile(parsed, MldExportOptions{ .platform = TargetPlatform::GameCube });
    ASSERT_GT(exported.size(), kEntryOffset + 0x07U);
    EXPECT_EQ(exported[kEntryOffset + 0x04U], 0xFFU);
    EXPECT_EQ(exported[kEntryOffset + 0x05U], 0xFFU);
    EXPECT_EQ(exported[kEntryOffset + 0x06U], 0xFFU);
    EXPECT_EQ(exported[kEntryOffset + 0x07U], 0xFEU);
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

TEST(MldEndian, GameCubeExportCanBeAklzCompressed) {
    MldParser parser;
    const auto file = parser.parseFile(makeMinimalMld(Endian::Big));
    const MldFileExporter exporter;
    const auto uncompressed = exporter.exportFile(file, MldExportOptions{ .platform = TargetPlatform::GameCube });
    const auto compressed = exporter.exportFile(file, MldExportOptions{ .platform = TargetPlatform::GameCube, .compressAklz = true });

    ASSERT_TRUE(spice::compression::aklz::isAklz(compressed));
    const auto decoded = spice::compression::aklz::decompress(compressed);
    ASSERT_TRUE(decoded.ok()) << spice::compression::aklz::errorToString(decoded.error);
    EXPECT_EQ(decoded.bytes, uncompressed);
}

TEST(MldEndian, DreamcastExportRejectsAklzCompression) {
    MldParser parser;
    const auto file = parser.parseFile(makeMinimalMld(Endian::Big));
    const MldFileExporter exporter;

    try {
        (void)exporter.exportFile(file, MldExportOptions{ .platform = TargetPlatform::Dreamcast, .compressAklz = true });
        FAIL() << "Expected AKLZ compression to reject Dreamcast export";
    } catch (const std::runtime_error& ex) {
        EXPECT_STREQ(ex.what(), "AKLZ compression is GameCube-only");
    }
}

TEST(MldTextureArchiveRebuild, ReplacesWithLargerTextureAndPreservesNames) {
    const auto small = encodeTexture(makeImage(8U, 8U, 3U), spice::gvm::model::TextureFormat::I4);
    const auto second = encodeTexture(makeImage(8U, 8U, 7U), spice::gvm::model::TextureFormat::RGB565);
    const auto replacement = encodeTexture(makeImage(8U, 8U, 11U), spice::gvm::model::TextureFormat::RGBA8);
    ASSERT_GT(replacement.size(), small.size());

    MldParser parser;
    const auto originalBytes = makeTexturedMld(small, second);
    const auto parsed = parser.parseFile(originalBytes);
    ASSERT_TRUE(parsed.textureArchive.has_value());
    ASSERT_EQ(parsed.textureArchive->entries.size(), 2U);
    EXPECT_EQ(parsed.textureArchive->entries[0].textureName, "tex_a");
    EXPECT_EQ(parsed.textureArchive->entries[1].textureName, "tex_b");
    ASSERT_FALSE(parsed.textureArchive->archivePrefixBytes.empty());

    MldExportOptions options{};
    options.platform = TargetPlatform::GameCube;
    options.textureReplacement = spice::mld::exporting::MldTextureReplacement{
        .textureIndex = 0U,
        .gvrData = replacement,
    };

    const auto rebuilt = MldFileExporter{}.exportFile(parsed, options);
    EXPECT_EQ(rebuilt.size(), originalBytes.size() + replacement.size() - small.size());

    const auto reparsed = parser.parseFile(rebuilt);
    ASSERT_TRUE(reparsed.textureArchive.has_value());
    ASSERT_EQ(reparsed.textureArchive->entries.size(), 2U);
    EXPECT_EQ(reparsed.textureArchive->entries[0].textureName, "tex_a");
    EXPECT_EQ(reparsed.textureArchive->entries[1].textureName, "tex_b");
    EXPECT_EQ(reparsed.textureArchive->entries[0].gvrDataSize, replacement.size());
    EXPECT_EQ(reparsed.textureArchive->entries[1].gvrDataSize, second.size());
    EXPECT_EQ(reparsed.textureArchive->entries[0].sourceFormat, "RGBA8");
    EXPECT_EQ(reparsed.textureArchive->archivePrefixBytes, parsed.textureArchive->archivePrefixBytes);
}

TEST(MldCanonicalTextureWriter, RebuildsEditedTextureArchiveFromCanonicalFile) {
    const auto small = encodeTexture(makeImage(8U, 8U, 3U), spice::gvm::model::TextureFormat::I4);
    const auto second = encodeTexture(makeImage(8U, 8U, 7U), spice::gvm::model::TextureFormat::RGB565);
    const auto replacement = encodeTexture(makeImage(8U, 8U, 11U), spice::gvm::model::TextureFormat::RGBA8);
    ASSERT_GT(replacement.size(), small.size());

    auto file = MldParser{}.parseBytes(makeTexturedMld(small, second));
    ASSERT_TRUE(file.textureArchive.has_value());
    ASSERT_EQ(file.textureArchive->entries.size(), 2U);
    file.textureArchive->entries[0].gvrData = replacement;

    const auto written = MldFileWriter{}.write(file);
    ASSERT_TRUE(written.ok());
    const auto reparsed = MldParser{}.parseBytes(written.bytes);
    ASSERT_TRUE(reparsed.textureArchive.has_value());
    ASSERT_EQ(reparsed.textureArchive->entries.size(), 2U);
    EXPECT_EQ(reparsed.textureArchive->entries[0].textureName, "tex_a");
    EXPECT_EQ(reparsed.textureArchive->entries[0].gvrData, replacement);
    EXPECT_EQ(reparsed.textureArchive->entries[1].textureName, "tex_b");
    EXPECT_EQ(reparsed.textureArchive->entries[1].gvrData, second);
}

TEST(MldCanonicalTextureWriter, RebuildsNameTableWhenAddingTextureEntry) {
    const auto first = encodeTexture(makeImage(8U, 8U, 3U), spice::gvm::model::TextureFormat::I4);
    const auto second = encodeTexture(makeImage(8U, 8U, 7U), spice::gvm::model::TextureFormat::RGB565);
    const auto added = encodeTexture(makeImage(8U, 8U, 13U), spice::gvm::model::TextureFormat::RGB5A3);

    auto file = MldParser{}.parseBytes(makeTexturedMld(first, second));
    ASSERT_TRUE(file.textureArchive.has_value());
    auto addedEntry = file.textureArchive->entries.front();
    addedEntry.archiveTextureIndex = 2U;
    addedEntry.textureName = "tex_c";
    addedEntry.gvrData = added;
    file.textureArchive->entries.push_back(std::move(addedEntry));

    const auto written = MldFileWriter{}.write(file);
    ASSERT_TRUE(written.ok());
    const auto reparsed = MldParser{}.parseBytes(written.bytes);
    ASSERT_TRUE(reparsed.textureArchive.has_value());
    ASSERT_EQ(reparsed.textureArchive->entries.size(), 3U);
    EXPECT_EQ(reparsed.textureArchive->entries[2].textureName, "tex_c");
    EXPECT_EQ(reparsed.textureArchive->entries[2].gvrData, added);
}

TEST(MldTextureArchiveRebuild, PreservesExactTextureGvrPayloadsForExtraction) {
    const auto first = encodeTexture(makeImage(8U, 8U, 3U), spice::gvm::model::TextureFormat::I4);
    const auto second = encodeTexture(makeImage(8U, 8U, 7U), spice::gvm::model::TextureFormat::RGB565);

    MldParser parser;
    const auto parsed = parser.parseFile(makeTexturedMld(first, second));
    ASSERT_TRUE(parsed.textureArchive.has_value());
    ASSERT_EQ(parsed.textureArchive->entries.size(), 2U);

    const auto& firstEntry = parsed.textureArchive->entries[0];
    const auto& secondEntry = parsed.textureArchive->entries[1];
    EXPECT_EQ(firstEntry.archiveTextureIndex, 0U);
    EXPECT_EQ(firstEntry.textureName, "tex_a");
    EXPECT_EQ(firstEntry.gvrDataSize, first.size());
    EXPECT_EQ(firstEntry.gvrData, first);
    EXPECT_EQ(secondEntry.archiveTextureIndex, 1U);
    EXPECT_EQ(secondEntry.textureName, "tex_b");
    EXPECT_EQ(secondEntry.gvrDataSize, second.size());
    EXPECT_EQ(secondEntry.gvrData, second);
}

TEST(MldTextureArchiveRebuild, ExtractedTextureGvrPayloadDecodesToPng) {
    const auto dir = testOutDir("texture_gvr_to_png");
    const auto image = makeImage(8U, 8U, 17U);
    const auto first = encodeTexture(image, spice::gvm::model::TextureFormat::RGBA8);
    const auto second = encodeTexture(makeImage(8U, 8U, 7U), spice::gvm::model::TextureFormat::RGB565);

    MldParser parser;
    const auto parsed = parser.parseFile(makeTexturedMld(first, second));
    ASSERT_TRUE(parsed.textureArchive.has_value());
    ASSERT_EQ(parsed.textureArchive->entries.size(), 2U);

    const auto& firstEntry = parsed.textureArchive->entries[0];
    const auto pngPath = dir / "tex_a.png";
    const auto exported = spice::gvm::ir::exportGvrPng(
        std::span<const std::uint8_t>(firstEntry.gvrData.data(), firstEntry.gvrData.size()),
        pngPath);
    const auto decoded = spice::gvm::image::readPngRgba8(pngPath);

    EXPECT_EQ(exported.texture.textureFormat, spice::gvm::model::TextureFormat::RGBA8);
    EXPECT_EQ(decoded.width, image.width);
    EXPECT_EQ(decoded.height, image.height);
    EXPECT_EQ(decoded.rgba8, image.rgba8);
}

TEST(MldTextureArchiveRebuild, ReplacesWithSmallerTextureWithoutPadding) {
    const auto large = encodeTexture(makeImage(8U, 8U, 5U), spice::gvm::model::TextureFormat::RGBA8);
    const auto second = encodeTexture(makeImage(8U, 8U, 9U), spice::gvm::model::TextureFormat::RGB5A3);
    const auto replacement = encodeTexture(makeImage(8U, 8U, 13U), spice::gvm::model::TextureFormat::I4);
    ASSERT_LT(replacement.size(), large.size());

    MldParser parser;
    const auto originalBytes = makeTexturedMld(large, second);
    const auto parsed = parser.parseFile(originalBytes);
    ASSERT_TRUE(parsed.textureArchive.has_value());

    MldExportOptions options{};
    options.platform = TargetPlatform::GameCube;
    options.textureReplacement = spice::mld::exporting::MldTextureReplacement{
        .textureIndex = 0U,
        .gvrData = replacement,
    };

    const auto rebuilt = MldFileExporter{}.exportFile(parsed, options);
    EXPECT_EQ(rebuilt.size(), originalBytes.size() - (large.size() - replacement.size()));

    const auto reparsed = parser.parseFile(rebuilt);
    ASSERT_TRUE(reparsed.textureArchive.has_value());
    ASSERT_EQ(reparsed.textureArchive->entries.size(), 2U);
    EXPECT_EQ(reparsed.textureArchive->entries[0].gvrDataSize, replacement.size());
    EXPECT_EQ(reparsed.textureArchive->entries[1].gvrDataSize, second.size());
    EXPECT_EQ(reparsed.textureArchive->entries[0].sourceFormat, "I4");
}

TEST(MldTextureArchiveRebuild, RejectsNonTerminalArchiveSizeShiftByDefault) {
    const auto small = encodeTexture(makeImage(8U, 8U, 3U), spice::gvm::model::TextureFormat::I4);
    const auto second = encodeTexture(makeImage(8U, 8U, 7U), spice::gvm::model::TextureFormat::RGB565);
    const auto replacement = encodeTexture(makeImage(8U, 8U, 11U), spice::gvm::model::TextureFormat::RGBA8);

    MldParser parser;
    const auto parsed = parser.parseFile(makeTexturedMld(small, second, true));
    ASSERT_TRUE(parsed.textureArchive.has_value());

    MldExportOptions options{};
    options.platform = TargetPlatform::GameCube;
    options.textureReplacement = spice::mld::exporting::MldTextureReplacement{
        .textureIndex = 0U,
        .gvrData = replacement,
    };

    EXPECT_THROW((void)MldFileExporter{}.exportFile(parsed, options), std::runtime_error);
    options.textureReplacement->allowPostArchiveShift = true;
    const auto rebuilt = MldFileExporter{}.exportFile(parsed, options);
    ASSERT_GE(rebuilt.size(), 4U);
    EXPECT_EQ(rebuilt[rebuilt.size() - 4U], 'T');
    EXPECT_EQ(rebuilt[rebuilt.size() - 3U], 'A');
    EXPECT_EQ(rebuilt[rebuilt.size() - 2U], 'I');
    EXPECT_EQ(rebuilt[rebuilt.size() - 1U], 'L');
}

TEST(MldTextureArchiveRebuild, PreservesAklzWrappingByDefaultWhenCompressed) {
    const auto small = encodeTexture(makeImage(8U, 8U, 3U), spice::gvm::model::TextureFormat::I4);
    const auto second = encodeTexture(makeImage(8U, 8U, 7U), spice::gvm::model::TextureFormat::RGB565);
    const auto replacement = encodeTexture(makeImage(8U, 8U, 11U), spice::gvm::model::TextureFormat::RGBA8);
    const auto compressed = spice::compression::aklz::compress(makeTexturedMld(small, second));
    ASSERT_TRUE(compressed.ok()) << spice::compression::aklz::errorToString(compressed.error);

    MldParser parser;
    const auto parsed = parser.parseFile(compressed.bytes);
    ASSERT_TRUE(parsed.sourceWasCompressedAklz);
    ASSERT_TRUE(parsed.textureArchive.has_value());

    MldExportOptions options{};
    options.platform = TargetPlatform::GameCube;
    options.compressAklz = parsed.sourceWasCompressedAklz;
    options.textureReplacement = spice::mld::exporting::MldTextureReplacement{
        .textureIndex = 0U,
        .gvrData = replacement,
    };

    const auto rebuilt = MldFileExporter{}.exportFile(parsed, options);
    ASSERT_TRUE(spice::compression::aklz::isAklz(rebuilt));
    const auto decoded = spice::compression::aklz::decompress(rebuilt);
    ASSERT_TRUE(decoded.ok()) << spice::compression::aklz::errorToString(decoded.error);
    const auto reparsed = parser.parseFile(decoded.bytes);
    ASSERT_TRUE(reparsed.textureArchive.has_value());
    EXPECT_EQ(reparsed.textureArchive->entries[0].gvrDataSize, replacement.size());
}

TEST(GobjParser, DecodesPositionOnlyVertexChunk22) {
    const auto bytes = makeSyntheticGobj(0x22U, 3U, false, false);
    const auto decoded = spice::mld::parsing::GobjParser{}.decode(bytes, 0x1000U, Endian::Big);

    ASSERT_TRUE(decoded.decoded);
    ASSERT_EQ(decoded.nodes.size(), 1U);
    const auto& mesh = decoded.nodes[0].streamMesh;
    ASSERT_EQ(mesh.vertices.size(), 3U);
    ASSERT_EQ(mesh.indices.size(), 3U);
    ASSERT_EQ(mesh.triangleMetadata.size(), 1U);
    EXPECT_FALSE(mesh.vertices[0].hasNormal);
    EXPECT_FALSE(mesh.vertices[0].rawUserAttributesU32.has_value());
    EXPECT_FLOAT_EQ(mesh.vertices[0].position.x, 2.0F);
    EXPECT_EQ(mesh.triangleMetadata[0].rawU16, (std::array<std::uint16_t, 3>{ 1U, 2U, 0x8003U }));
}

TEST(GobjParser, PreservesExistingNormalVertexChunk29) {
    const auto bytes = makeSyntheticGobj(0x29U, 6U, true, false);
    const auto decoded = spice::mld::parsing::GobjParser{}.decode(bytes, 0x2000U, Endian::Big);

    ASSERT_TRUE(decoded.decoded);
    ASSERT_EQ(decoded.nodes.size(), 1U);
    const auto& mesh = decoded.nodes[0].streamMesh;
    ASSERT_EQ(mesh.vertices.size(), 3U);
    EXPECT_TRUE(mesh.vertices[0].hasNormal);
    EXPECT_FLOAT_EQ(mesh.vertices[0].normal.y, 1.0F);
    EXPECT_FALSE(mesh.vertices[0].rawUserAttributesU32.has_value());
}

TEST(GobjParser, DecodesChunk2bAndPreservesRawUserAttributes) {
    const auto bytes = makeSyntheticGobj(0x2BU, 7U, true, true);
    const auto decoded = spice::mld::parsing::GobjParser{}.decode(bytes, 0x3000U, Endian::Big);

    ASSERT_TRUE(decoded.decoded);
    ASSERT_EQ(decoded.nodes.size(), 1U);
    const auto& mesh = decoded.nodes[0].streamMesh;
    ASSERT_EQ(mesh.vertices.size(), 3U);
    ASSERT_TRUE(mesh.vertices[0].rawUserAttributesU32.has_value());
    ASSERT_TRUE(mesh.vertices[1].rawUserAttributesU32.has_value());
    ASSERT_TRUE(mesh.vertices[2].rawUserAttributesU32.has_value());
    EXPECT_EQ(*mesh.vertices[0].rawUserAttributesU32, 0U);
    EXPECT_EQ(*mesh.vertices[1].rawUserAttributesU32, 0x12345678U);
    EXPECT_EQ(*mesh.vertices[2].rawUserAttributesU32, 0xFFFFFFFFU);
}

TEST(GrndParser, PreservesRawTriangleMetadataAcrossWindingReversal) {
    const auto bytes = makeSyntheticGrnd();
    const auto decoded = spice::mld::parsing::GrndParser{}.decode(bytes, 0x4000U, Endian::Big);

    ASSERT_TRUE(decoded.decoded);
    ASSERT_EQ(decoded.mesh.indices.size(), 3U);
    EXPECT_EQ(decoded.mesh.indices[0], 2U);
    EXPECT_EQ(decoded.mesh.indices[1], 1U);
    EXPECT_EQ(decoded.mesh.indices[2], 0U);
    ASSERT_EQ(decoded.mesh.triangleMetadata.size(), 1U);
    EXPECT_EQ(decoded.mesh.triangleMetadata[0].rawU16,
        (std::array<std::uint16_t, 3>{ 0x0001U, 0x7FFFU, 0x800AU }));
}

TEST(BlenderIrJsonExporter, EmitsRawTriangleMetadataAndVertexUserAttributesWithoutSemantics) {
    spice::mld::model::BlenderIrScene scene{};
    spice::mld::model::BlenderIrMesh mesh{};
    spice::mld::model::BlenderIrVertex vertex{};
    vertex.rawUserAttributesU32 = 0xFFFFFFFFU;
    mesh.vertices.push_back(vertex);

    spice::mld::model::BlenderIrTriangleSet triangleSet{};
    triangleSet.triangleMetadata.push_back(spice::mld::model::TriangleMetadata{
        .rawU16 = { 0U, 0xFFFFU, 0x8000U },
    });
    mesh.triangleSets.push_back(std::move(triangleSet));
    scene.meshes.push_back(std::move(mesh));

    const auto json = spice::mld::exporting::BlenderIrJsonExporter{}.toJson(scene);
    EXPECT_NE(json.find("\"rawUserAttributesU32\":4294967295"), std::string::npos);
    EXPECT_NE(json.find("\"triangleMetadata\":[{\"rawU16\":[0,65535,32768]}]"), std::string::npos);
    EXPECT_EQ(json.find("collisionTriangles"), std::string::npos);
    EXPECT_EQ(json.find("selectorLow15"), std::string::npos);
    EXPECT_EQ(json.find("encounterTableId"), std::string::npos);
}
