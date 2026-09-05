#include "../SpiceMLD/SpiceMLD.h"
#include "../SpiceMLD/Export/MldFileWriter.h"
#include "../SpiceMLD/Model/MldGroundEditing.h"
#include "../SpiceMLD/Parsing/MldParser.h"
#include "../SpiceMLD/Parsing/Sa3dBlenderIrBuilder.h"
#include "../Compression/Aklz.h"
#include "../SpiceModeling/SpiceModeling.h"
#include "CorpusTestSupport.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using spice::root::Endian;
using spice::mld::exporting::MldFileWriter;
using spice::mld::parsing::MldParser;

std::string describeDiagnostics(const spice::mld::model::MldFile& file) {
    std::ostringstream out{};
    for (const auto& diagnostic : file.parseDiagnostics) {
        out << '\n' << static_cast<int>(diagnostic.severity) << ": " << diagnostic.message;
        if (diagnostic.sourceOffset.has_value()) {
            out << " at 0x" << std::hex << *diagnostic.sourceOffset << std::dec;
        }
    }
    return out.str();
}

void writeU16(std::vector<std::uint8_t>& bytes, const std::size_t offset,
    const std::uint16_t value, const Endian endian = Endian::Big) {
    if (endian == Endian::Big) {
        bytes[offset] = static_cast<std::uint8_t>(value >> 8U);
        bytes[offset + 1U] = static_cast<std::uint8_t>(value);
    } else {
        bytes[offset] = static_cast<std::uint8_t>(value);
        bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
    }
}

void writeU32(std::vector<std::uint8_t>& bytes, const std::size_t offset,
    const std::uint32_t value, const Endian endian = Endian::Big) {
    if (endian == Endian::Big) {
        bytes[offset] = static_cast<std::uint8_t>(value >> 24U);
        bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 16U);
        bytes[offset + 2U] = static_cast<std::uint8_t>(value >> 8U);
        bytes[offset + 3U] = static_cast<std::uint8_t>(value);
    } else {
        bytes[offset] = static_cast<std::uint8_t>(value);
        bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
        bytes[offset + 2U] = static_cast<std::uint8_t>(value >> 16U);
        bytes[offset + 3U] = static_cast<std::uint8_t>(value >> 24U);
    }
}

void writeF32(std::vector<std::uint8_t>& bytes, const std::size_t offset, const float value,
    const Endian endian = Endian::Big) {
    writeU32(bytes, offset, std::bit_cast<std::uint32_t>(value), endian);
}

void writeTag(std::vector<std::uint8_t>& bytes, const std::size_t offset, const char* tag) {
    for (std::size_t i = 0; i < 4U; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(tag[i]);
    }
}

void writeList(std::vector<std::uint8_t>& bytes, const std::size_t offset,
    const std::span<const std::uint32_t> values) {
    writeU32(bytes, offset, static_cast<std::uint32_t>(values.size()));
    for (std::size_t i = 0; i < values.size(); ++i) {
        writeU32(bytes, offset + 4U + i * 4U, values[i]);
    }
}

std::vector<std::uint8_t> makeBaseMld(const std::uint32_t resourceAddress = 0U,
    const bool resourceIsObject = false) {
    constexpr std::size_t entry = 0x20U;
    constexpr std::size_t groundLinks = 0x100U;
    constexpr std::size_t sharedParams = 0x108U;
    constexpr std::size_t objects = 0x118U;
    constexpr std::size_t grounds = 0x120U;
    constexpr std::size_t motions = 0x128U;
    std::vector<std::uint8_t> bytes(0x340U, 0xCDU);
    writeU32(bytes, 0U, 1U);
    writeU32(bytes, 4U, static_cast<std::uint32_t>(entry));
    writeU32(bytes, 8U, static_cast<std::uint32_t>(sharedParams));
    writeU32(bytes, 0x0CU, resourceAddress == 0U ? 0x180U : resourceAddress);
    writeU32(bytes, 0x10U, 0x320U);
    writeU32(bytes, entry, 7U);
    writeU32(bytes, entry + 4U, 9U);
    writeU32(bytes, entry + 8U, static_cast<std::uint32_t>(groundLinks));
    writeU32(bytes, entry + 0x0CU, static_cast<std::uint32_t>(sharedParams));
    writeU32(bytes, entry + 0x10U, static_cast<std::uint32_t>(sharedParams));
    writeU32(bytes, entry + 0x14U, static_cast<std::uint32_t>(objects));
    writeU32(bytes, entry + 0x18U, static_cast<std::uint32_t>(grounds));
    writeU32(bytes, entry + 0x1CU, static_cast<std::uint32_t>(motions));
    writeU32(bytes, entry + 0x20U, 0U);
    const char name[] = "wall";
    std::copy_n(name, 4U, bytes.begin() + static_cast<std::ptrdiff_t>(entry + 0x24U));
    writeF32(bytes, entry + 0x5CU, 1.0F);
    writeF32(bytes, entry + 0x60U, 1.0F);
    writeF32(bytes, entry + 0x64U, 1.0F);
    const std::array<std::uint32_t, 0> empty{};
    const std::array<std::uint32_t, 2> params{ 11U, 22U };
    writeList(bytes, groundLinks, empty);
    writeList(bytes, sharedParams, params);
    const std::array<std::uint32_t, 1> resource{ resourceAddress };
    writeList(bytes, objects, resourceIsObject && resourceAddress != 0U ? std::span<const std::uint32_t>(resource) : std::span<const std::uint32_t>(empty));
    writeList(bytes, grounds, !resourceIsObject && resourceAddress != 0U ? std::span<const std::uint32_t>(resource) : std::span<const std::uint32_t>(empty));
    writeList(bytes, motions, empty);
    writeU32(bytes, 0x320U, 0U);
    return bytes;
}

std::vector<std::uint8_t> makeGrndMld(
    const std::array<float, 3> translation = {},
    const std::array<float, 2> gridOrigin = {}) {
    constexpr std::uint32_t address = 0x180U;
    auto bytes = makeBaseMld(address, false);
    constexpr std::size_t sets = 0x40U;
    constexpr std::size_t stream = 0x60U;
    constexpr std::size_t vertices = 0x80U;
    constexpr std::size_t registry = 0xC8U;
    constexpr std::size_t table = registry + 4U;
    constexpr std::size_t refs = 0xD4U;
    constexpr std::size_t size = 0xD8U;
    writeTag(bytes, address, "GRND");
    writeU32(bytes, address + 4U, static_cast<std::uint32_t>(size));
    writeU32(bytes, address + 0x10U, static_cast<std::uint32_t>(sets - 0x10U));
    writeU32(bytes, address + 0x14U, static_cast<std::uint32_t>(registry - 0x10U));
    writeF32(bytes, address + 0x18U, gridOrigin[0]);
    writeF32(bytes, address + 0x1CU, gridOrigin[1]);
    writeU16(bytes, address + 0x20U, 1U);
    writeU16(bytes, address + 0x22U, 1U);
    writeU16(bytes, address + 0x24U, 10U);
    writeU16(bytes, address + 0x26U, 10U);
    writeU16(bytes, address + 0x28U, 1U);
    writeU16(bytes, address + 0x2AU, 1U);
    writeF32(bytes, address + sets, translation[0]);
    writeF32(bytes, address + sets + 4U, translation[1]);
    writeF32(bytes, address + sets + 8U, translation[2]);
    writeU32(bytes, address + sets + 0x0CU, static_cast<std::uint32_t>(vertices - (sets + 0x0CU)));
    writeU32(bytes, address + sets + 0x10U, static_cast<std::uint32_t>(stream - (sets + 0x10U)));
    writeU32(bytes, address + sets + 0x14U, 1U);
    for (std::size_t i = 0; i < 3U; ++i) {
        writeU16(bytes, address + stream + i * 4U, static_cast<std::uint16_t>(i * 6U));
        writeU16(bytes, address + stream + i * 4U + 2U, static_cast<std::uint16_t>(i + 1U));
        const auto vertex = address + vertices + i * 24U;
        writeF32(bytes, vertex, static_cast<float>(i == 1U));
        writeF32(bytes, vertex + 4U, 0.0F);
        writeF32(bytes, vertex + 8U, static_cast<float>(i == 2U));
        writeF32(bytes, vertex + 12U, 0.0F);
        writeF32(bytes, vertex + 16U, 1.0F);
        writeF32(bytes, vertex + 20U, 0.0F);
    }
    writeU32(bytes, address + table, 1U);
    writeU32(bytes, address + table + 4U, static_cast<std::uint32_t>(refs - (table + 4U)));
    writeU16(bytes, address + refs, 0U);
    writeU16(bytes, address + refs + 2U, 0U);
    return bytes;
}

std::vector<std::uint8_t> makeGobjMld(const bool normalDiffuse = false) {
    constexpr std::uint32_t address = 0x180U;
    auto bytes = makeBaseMld(address, true);
    constexpr std::size_t node = 0x10U;
    constexpr std::size_t attach = 0x50U;
    constexpr std::size_t payload = attach + 0x10U;
    constexpr std::size_t stream = payload + 76U;
    constexpr std::size_t vertices = 0xBCU;
    const std::size_t recordWords = normalDiffuse ? 7U : 6U;
    const std::size_t size = vertices + 8U + 3U * recordWords * 4U;
    writeTag(bytes, address, "GOBJ");
    writeU32(bytes, address + 4U, static_cast<std::uint32_t>(size));
    writeU32(bytes, address + node, static_cast<std::uint32_t>(attach - node));
    writeF32(bytes, address + node + 0x20U, 1.0F);
    writeF32(bytes, address + node + 0x24U, 1.0F);
    writeF32(bytes, address + node + 0x28U, 1.0F);
    writeU32(bytes, address + payload, static_cast<std::uint32_t>(vertices - payload));
    for (std::size_t i = 0; i < 3U; ++i) {
        writeU16(bytes, address + stream + i * 4U, static_cast<std::uint16_t>(2U + i * recordWords));
        writeU16(bytes, address + stream + i * 4U + 2U, static_cast<std::uint16_t>(i + 1U));
    }
    writeU16(bytes, address + stream + 12U, 0xFFFFU);
    writeU16(bytes, address + stream + 14U, 0xFFFFU);
    writeU32(bytes, address + vertices, normalDiffuse ? 0x2AU : 0x29U);
    writeU32(bytes, address + vertices + 4U, 3U << 16U);
    for (std::size_t i = 0; i < 3U; ++i) {
        const auto vertex = address + vertices + 8U + i * recordWords * 4U;
        writeF32(bytes, vertex, static_cast<float>(i == 1U));
        writeF32(bytes, vertex + 4U, 0.0F);
        writeF32(bytes, vertex + 8U, static_cast<float>(i == 2U));
        writeF32(bytes, vertex + 12U, 0.0F);
        writeF32(bytes, vertex + 16U, 1.0F);
        writeF32(bytes, vertex + 20U, 0.0F);
        if (normalDiffuse) {
            constexpr std::array<std::uint32_t, 3> colors{ 0x11223344U, 0x80ABCDEFU, 0xFFFFFFFFU };
            writeU32(bytes, vertex + 24U, colors[i]);
        }
    }
    return bytes;
}

std::filesystem::path findMldFixture(const std::string& name) {
    auto current = std::filesystem::current_path();
    for (;;) {
        const auto candidate = current / "SpiceTests" / "fixtures" / "mld" / name;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (!current.has_parent_path() || current.parent_path() == current) {
            return {};
        }
        current = current.parent_path();
    }
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), {});
}

std::array<std::uint8_t, 32U> digestFromHex(const std::string_view hex) {
    const auto nibble = [](const char value) -> std::uint8_t {
        if (value >= '0' && value <= '9') return static_cast<std::uint8_t>(value - '0');
        if (value >= 'a' && value <= 'f') return static_cast<std::uint8_t>(value - 'a' + 10);
        return static_cast<std::uint8_t>(value - 'A' + 10);
    };
    std::array<std::uint8_t, 32U> digest{};
    for (std::size_t i = 0U; i < digest.size(); ++i) {
        digest[i] = static_cast<std::uint8_t>((nibble(hex[i * 2U]) << 4U) | nibble(hex[i * 2U + 1U]));
    }
    return digest;
}

template <typename VertexRange>
std::array<float, 6> positionBounds(const VertexRange& vertices) {
    std::array<float, 6> bounds{
        std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(),
    };
    for (const auto& vertex : vertices) {
        bounds[0] = std::min(bounds[0], vertex.position.x);
        bounds[1] = std::max(bounds[1], vertex.position.x);
        bounds[2] = std::min(bounds[2], vertex.position.y);
        bounds[3] = std::max(bounds[3], vertex.position.y);
        bounds[4] = std::min(bounds[4], vertex.position.z);
        bounds[5] = std::max(bounds[5], vertex.position.z);
    }
    return bounds;
}

} // namespace

TEST(MldCanonical, ParseBytesOwnsSharedListsAndCompleteSourceRanges) {
    const auto bytes = makeBaseMld();
    const auto file = MldParser{}.parseBytes(bytes);
    ASSERT_EQ(file.parseStatus, spice::mld::model::MldParseStatus::Complete)
        << describeDiagnostics(file);
    EXPECT_EQ(file.assetStatus, spice::mld::model::MldResourceStatus::Empty);
    ASSERT_EQ(file.entries.size(), 1U);
    ASSERT_TRUE(file.entries[0].entry.paramList2);
    ASSERT_TRUE(file.entries[0].entry.functionParameters);
    EXPECT_EQ(file.entries[0].entry.paramList2.get(), file.entries[0].entry.functionParameters.get());
    EXPECT_EQ(file.u32Lists.at(0x108U).get(), file.entries[0].entry.functionParameters.get());
    ASSERT_FALSE(file.sourceRanges.empty());
    std::size_t cursor = 0U;
    for (const auto& range : file.sourceRanges) {
        EXPECT_EQ(range.offset, cursor);
        cursor += range.size;
    }
    EXPECT_EQ(cursor, file.decodedBytes.size());
}

TEST(MldCanonical, WriterReturnsExactSourceAndRelocatesGrowingSharedList) {
    const auto bytes = makeBaseMld();
    auto file = MldParser{}.parseBytes(bytes);
    const auto unchanged = MldFileWriter{}.write(file);
    ASSERT_TRUE(unchanged.ok());
    ASSERT_EQ(unchanged.bytes.size(), bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        ASSERT_EQ(unchanged.bytes[i], bytes[i]) << "first changed byte at 0x" << std::hex << i;
    }

    file.entries[0].entry.functionParameters->values.resize(40U, 0xAABBCCDDU);
    const auto changed = MldFileWriter{}.write(file);
    ASSERT_TRUE(changed.ok());
    const auto reparsed = MldParser{}.parseBytes(changed.bytes);
    ASSERT_EQ(reparsed.entries.size(), 1U);
    ASSERT_TRUE(reparsed.entries[0].entry.paramList2);
    ASSERT_TRUE(reparsed.entries[0].entry.functionParameters);
    EXPECT_EQ(reparsed.entries[0].entry.paramList2.get(), reparsed.entries[0].entry.functionParameters.get());
    EXPECT_EQ(reparsed.entries[0].entry.functionParameters->values.size(), 40U);
}

TEST(MldCanonical, WriterReusesVacatedKnownRangesBeforeGrowingOutput) {
    const auto source = makeBaseMld();
    auto file = MldParser{}.parseBytes(source);
    auto& entry = file.entries[0].entry;
    ASSERT_TRUE(entry.functionParameters);
    ASSERT_TRUE(entry.objectAddresses);
    entry.functionParameters->values.clear();
    entry.objectAddresses->values = { 0x300U };

    const auto written = MldFileWriter{}.write(file);
    ASSERT_TRUE(written.ok());
    EXPECT_EQ(written.bytes.size(), source.size());
    const auto objectListLayout = std::find_if(written.layout.begin(), written.layout.end(), [](const auto& item) {
        return item.kind == "u32-list" && item.sourceOffset == 0x118U;
    });
    ASSERT_NE(objectListLayout, written.layout.end());
    EXPECT_EQ(objectListLayout->outputOffset, 0x10CU);

    const auto reparsed = MldParser{}.parseBytes(written.bytes);
    ASSERT_TRUE(reparsed.entries[0].entry.functionParameters);
    ASSERT_TRUE(reparsed.entries[0].entry.objectAddresses);
    EXPECT_TRUE(reparsed.entries[0].entry.functionParameters->values.empty());
    EXPECT_EQ(reparsed.entries[0].entry.objectAddresses->values, (std::vector<std::uint32_t>{ 0x300U }));
}

TEST(MldCanonical, WriterRebuildsEditedGrndTopology) {
    auto file = MldParser{}.parseBytes(makeGrndMld());
    auto& resource = file.groundResources.at(0x180U);
    ASSERT_TRUE(resource.grnd.has_value());
    auto& data = *resource.grnd;
    ASSERT_EQ(data.mesh.indices.size(), 3U);
    data.mesh.indices.insert(data.mesh.indices.end(), data.mesh.indices.begin(), data.mesh.indices.begin() + 3);
    data.mesh.triangleMetadata.push_back(data.mesh.triangleMetadata.front());
    data.cells[0].references.push_back(spice::mld::model::GrndTriangleReference{ .meshTriangleIndex = 1U });
    const auto written = MldFileWriter{}.write(file);
    ASSERT_TRUE(written.ok());
    const auto reparsed = MldParser{}.parseBytes(written.bytes);
    ASSERT_TRUE(reparsed.groundResources.at(reparsed.entries[0].entry.groundAddresses->values[0]).grnd.has_value());
    EXPECT_EQ(reparsed.groundResources.at(reparsed.entries[0].entry.groundAddresses->values[0]).grnd->mesh.indices.size(), 6U);
}

TEST(MldCanonical, WriterPreservesBakedGrndPlacementWhenCanonicalizingToZeroTranslation) {
    const auto source = makeGrndMld({ 10.0F, 20.0F, -30.0F }, { -100.5F, 200.25F });
    auto file = MldParser{}.parseBytes(source);
    auto& data = *file.groundResources.at(0x180U).grnd;
    ASSERT_EQ(data.triangleSets.size(), 1U);
    EXPECT_FLOAT_EQ(data.triangleSets[0].localToResourceTranslation.x, 10.0F);
    EXPECT_FLOAT_EQ(data.triangleSets[0].verticesByFloatIndex.at(0U).position.x, 0.0F);
    ASSERT_EQ(data.mesh.vertices.size(), 3U);
    EXPECT_FLOAT_EQ(data.mesh.vertices[0].position.x, 10.0F);
    EXPECT_FLOAT_EQ(data.mesh.vertices[0].position.y, 20.0F);
    EXPECT_FLOAT_EQ(data.mesh.vertices[0].position.z, -30.0F);

    const auto unchanged = MldFileWriter{}.write(file);
    ASSERT_TRUE(unchanged.ok());
    EXPECT_EQ(unchanged.bytes, source);

    const auto converted = MldFileWriter{}.write(file, spice::mld::exporting::MldWriteOptions{
        .platform = spice::mld::model::TargetPlatform::Dreamcast,
    });
    ASSERT_TRUE(converted.ok());
    const auto convertedFile = MldParser{}.parseBytes(converted.bytes);
    const auto convertedAddress = convertedFile.entries[0].entry.groundAddresses->values[0];
    const auto& convertedData = *convertedFile.groundResources.at(convertedAddress).grnd;
    ASSERT_EQ(convertedData.triangleSets.size(), 1U);
    EXPECT_FLOAT_EQ(convertedData.triangleSets[0].localToResourceTranslation.x, 0.0F);
    EXPECT_FLOAT_EQ(convertedData.triangleSets[0].localToResourceTranslation.y, 0.0F);
    EXPECT_FLOAT_EQ(convertedData.triangleSets[0].localToResourceTranslation.z, 0.0F);
    EXPECT_EQ(positionBounds(convertedData.mesh.vertices), positionBounds(data.mesh.vertices));
    EXPECT_FLOAT_EQ(convertedData.gridOriginX, -100.5F);
    EXPECT_FLOAT_EQ(convertedData.gridOriginZ, 200.25F);

    data.mesh.triangleMetadata[0].rawU16[0] ^= 1U;
    const auto changed = MldFileWriter{}.write(file);
    ASSERT_TRUE(changed.ok());
    const auto reparsed = MldParser{}.parseBytes(changed.bytes);
    const auto address = reparsed.entries[0].entry.groundAddresses->values[0];
    const auto& rebuilt = *reparsed.groundResources.at(address).grnd;
    ASSERT_EQ(rebuilt.triangleSets.size(), 1U);
    EXPECT_FLOAT_EQ(rebuilt.triangleSets[0].localToResourceTranslation.x, 0.0F);
    EXPECT_FLOAT_EQ(rebuilt.triangleSets[0].localToResourceTranslation.y, 0.0F);
    EXPECT_FLOAT_EQ(rebuilt.triangleSets[0].localToResourceTranslation.z, 0.0F);
    EXPECT_FLOAT_EQ(rebuilt.gridOriginX, -100.5F);
    EXPECT_FLOAT_EQ(rebuilt.gridOriginZ, 200.25F);
    ASSERT_EQ(rebuilt.mesh.vertices.size(), 3U);
    EXPECT_FLOAT_EQ(rebuilt.mesh.vertices[0].position.x, 10.0F);
    EXPECT_FLOAT_EQ(rebuilt.mesh.vertices[0].position.y, 20.0F);
    EXPECT_FLOAT_EQ(rebuilt.mesh.vertices[0].position.z, -30.0F);
    EXPECT_EQ(rebuilt.mesh.triangleMetadata[0].rawU16[0], data.mesh.triangleMetadata[0].rawU16[0]);
}

TEST(MldCanonical, GrndGridHelperAssignsEveryTriangleForWriting) {
    auto file = MldParser{}.parseBytes(makeGrndMld());
    auto& data = *file.groundResources.at(0x180U).grnd;
    data.cells.clear();
    std::vector<std::string> diagnostics{};
    ASSERT_TRUE(spice::mld::model::assignGrndTrianglesToIntersectingCells(
        data,
        spice::mld::model::GrndGridAssignmentOptions{ .originX = 0.0F, .originZ = 0.0F },
        &diagnostics));
    ASSERT_EQ(data.cells.size(), 1U);
    ASSERT_EQ(data.cells[0].references.size(), 1U);
    EXPECT_EQ(data.cells[0].references[0].meshTriangleIndex, 0U);
    EXPECT_FLOAT_EQ(data.gridOriginX, 0.0F);
    EXPECT_FLOAT_EQ(data.gridOriginZ, 0.0F);
    EXPECT_TRUE(MldFileWriter{}.write(file).ok());
}

TEST(MldCanonical, WriterRebuildsEditedGobjTopologyAndUpdatesObjectPointer) {
    auto file = MldParser{}.parseBytes(makeGobjMld());
    auto& resource = file.groundResources.at(0x180U);
    ASSERT_TRUE(resource.gobj.has_value());
    auto& mesh = resource.gobj->nodes[0].streamMesh;
    ASSERT_EQ(mesh.indices.size(), 3U);
    mesh.indices.insert(mesh.indices.end(), mesh.indices.begin(), mesh.indices.begin() + 3);
    mesh.triangleMetadata.push_back(mesh.triangleMetadata.front());
    const auto written = MldFileWriter{}.write(file);
    ASSERT_TRUE(written.ok());
    const auto reparsed = MldParser{}.parseBytes(written.bytes);
    const auto newAddress = reparsed.entries[0].entry.objectAddresses->values[0];
    ASSERT_TRUE(reparsed.groundResources.at(newAddress).gobj.has_value());
    EXPECT_EQ(reparsed.groundResources.at(newAddress).gobj->nodes[0].streamMesh.indices.size(), 6U);
}

TEST(MldCanonical, WriterPreservesAndRebuildsNormalDiffuseGobj) {
    const auto source = makeGobjMld(true);
    auto file = MldParser{}.parseBytes(source);
    auto& resource = file.groundResources.at(0x180U);
    ASSERT_TRUE(resource.gobj.has_value());
    auto& node = resource.gobj->nodes[0];
    ASSERT_TRUE(node.attach.has_value());
    EXPECT_EQ(node.attach->vertexChunk.chunkType, 0x2AU);
    ASSERT_EQ(node.streamMesh.vertices.size(), 3U);
    ASSERT_TRUE(node.streamMesh.vertices[0].diffuseColor.has_value());

    const auto unchanged = MldFileWriter{}.write(file);
    ASSERT_TRUE(unchanged.ok());
    EXPECT_EQ(unchanged.bytes, source);

    node.streamMesh.vertices[0].diffuseColor = spice::mld::model::ColorRgba8{
        .r = 1U, .g = 2U, .b = 3U, .a = 4U,
    };
    const auto written = MldFileWriter{}.write(file);
    ASSERT_TRUE(written.ok());
    const auto reparsed = MldParser{}.parseBytes(written.bytes);
    const auto address = reparsed.entries[0].entry.objectAddresses->values[0];
    const auto& rebuiltNode = reparsed.groundResources.at(address).gobj->nodes[0];
    ASSERT_TRUE(rebuiltNode.attach.has_value());
    EXPECT_EQ(rebuiltNode.attach->vertexChunk.chunkType, 0x2AU);
    ASSERT_TRUE(rebuiltNode.streamMesh.vertices[0].diffuseColor.has_value());
    EXPECT_EQ(rebuiltNode.streamMesh.vertices[0].diffuseColor->r, 1U);
    EXPECT_EQ(rebuiltNode.streamMesh.vertices[0].diffuseColor->g, 2U);
    EXPECT_EQ(rebuiltNode.streamMesh.vertices[0].diffuseColor->b, 3U);
    EXPECT_EQ(rebuiltNode.streamMesh.vertices[0].diffuseColor->a, 4U);

    const auto gameCubeWritten = MldFileWriter{}.write(file, spice::mld::exporting::MldWriteOptions{
        .platform = spice::mld::model::TargetPlatform::GameCube,
        .compressAklz = false,
    });
    ASSERT_TRUE(gameCubeWritten.ok());
    const auto gameCubeFile = MldParser{}.parseBytes(gameCubeWritten.bytes);
    EXPECT_EQ(gameCubeFile.endian, Endian::Big);
    const auto gameCubeAddress = gameCubeFile.entries[0].entry.objectAddresses->values[0];
    const auto& gameCubeNode = gameCubeFile.groundResources.at(gameCubeAddress).gobj->nodes[0];
    ASSERT_TRUE(gameCubeNode.streamMesh.vertices[0].diffuseColor.has_value());
    EXPECT_EQ(gameCubeNode.streamMesh.vertices[0].diffuseColor->r, 1U);
    EXPECT_EQ(gameCubeNode.streamMesh.vertices[0].diffuseColor->g, 2U);
    EXPECT_EQ(gameCubeNode.streamMesh.vertices[0].diffuseColor->b, 3U);
    EXPECT_EQ(gameCubeNode.streamMesh.vertices[0].diffuseColor->a, 4U);

    const auto scene = spice::mld::parsing::Sa3dBlenderIrBuilder{}.build(reparsed);
    const auto mesh = std::find_if(scene.meshes.begin(), scene.meshes.end(), [](const auto& candidate) {
        return candidate.label.starts_with("GOBJ_");
    });
    ASSERT_NE(mesh, scene.meshes.end());
    ASSERT_FALSE(mesh->triangleSets.empty());
    ASSERT_FALSE(mesh->triangleSets[0].corners.empty());
    EXPECT_TRUE(mesh->triangleSets[0].corners[0].hasColor);

    auto invalid = reparsed;
    auto& invalidVertices = invalid.groundResources.at(address).gobj->nodes[0].streamMesh.vertices;
    invalidVertices[0].diffuseColor.reset();
    EXPECT_FALSE(MldFileWriter{}.write(invalid).ok());
}

TEST(MldCanonical, WrappedRealFixtureSeparatesTextureListHealthAndProjectsVisibleGeometry) {
    const auto fixture = findMldFixture("s044_sml_entry_0.mld");
    if (fixture.empty()) {
        GTEST_SKIP() << "Private S044 MLD fixture is unavailable";
    }
    const auto file = MldParser{}.parseBytes(readBytes(fixture));
    ASSERT_EQ(file.parseStatus, spice::mld::model::MldParseStatus::Complete)
        << describeDiagnostics(file);
    EXPECT_EQ(file.assetStatus, spice::mld::model::MldResourceStatus::Partial);
    const auto textureList = file.textureListResources.find(0x9CU);
    ASSERT_NE(textureList, file.textureListResources.end());
    EXPECT_EQ(textureList->second.status, spice::mld::model::MldResourceStatus::Complete);
    EXPECT_TRUE(textureList->second.diagnostics.empty());
    EXPECT_FALSE(file.objectResources.contains(0x9CU));
    ASSERT_FALSE(file.entries.empty());
    ASSERT_TRUE(file.entries[0].entry.objectAddresses);
    EXPECT_NE(std::find(file.entries[0].entry.objectAddresses->values.begin(),
        file.entries[0].entry.objectAddresses->values.end(), 0xC0U),
        file.entries[0].entry.objectAddresses->values.end());
    const auto object = file.objectResources.find(0xC0U);
    ASSERT_NE(object, file.objectResources.end());
    ASSERT_TRUE(object->second.modelBlockOffset.has_value());
    EXPECT_EQ(*object->second.modelBlockOffset, 0x1F0U);
    ASSERT_TRUE(object->second.model);
    const auto scene = spice::mld::parsing::Sa3dBlenderIrBuilder{}.build(file);
    EXPECT_FALSE(scene.objectTrees.empty());
    EXPECT_FALSE(scene.meshes.empty());
}

TEST(MldCanonical, WriterPreservesAklzSourceAndSupportsDreamcastProjection) {
    const auto source = makeGrndMld();
    const auto compressed = spice::compression::aklz::compress(source);
    ASSERT_TRUE(compressed.ok());
    const auto compressedFile = MldParser{}.parseBytes(compressed.bytes);
    const auto preserved = MldFileWriter{}.write(compressedFile);
    ASSERT_TRUE(preserved.ok());
    EXPECT_EQ(preserved.bytes, compressed.bytes);

    const auto file = MldParser{}.parseBytes(source);
    const auto converted = MldFileWriter{}.write(file, spice::mld::exporting::MldWriteOptions{
        .platform = spice::mld::model::TargetPlatform::Dreamcast,
        .compressAklz = false,
    });
    ASSERT_TRUE(converted.ok());
    const auto reparsed = MldParser{}.parseBytes(converted.bytes);
    EXPECT_EQ(reparsed.endian, Endian::Little);
    const auto address = reparsed.entries[0].entry.groundAddresses->values[0];
    ASSERT_TRUE(reparsed.groundResources.at(address).grnd.has_value());
    EXPECT_EQ(reparsed.groundResources.at(address).grnd->mesh.indices.size(), 3U);
}

TEST(MldCanonical, WriterRejectsRelocationReferencedByUnknownRange) {
    auto bytes = makeBaseMld();
    writeU32(bytes, 0x150U, 0x108U);
    auto file = MldParser{}.parseBytes(bytes);
    file.entries[0].entry.functionParameters->values.resize(40U, 1U);
    const auto written = MldFileWriter{}.write(file);
    EXPECT_FALSE(written.ok());
    EXPECT_TRUE(written.bytes.empty());
}

TEST(MldCanonical, WriterRejectsReplacedReadOnlySa3dModel) {
    const auto fixture = findMldFixture("s044_sml_entry_0.mld");
    if (fixture.empty()) {
        GTEST_SKIP() << "Private S044 MLD fixture is unavailable";
    }
    auto file = MldParser{}.parseBytes(readBytes(fixture));
    ASSERT_TRUE(file.objectResources.at(0xC0U).model);
    file.objectResources.at(0xC0U).model.reset();
    const auto written = MldFileWriter{}.write(file);
    EXPECT_FALSE(written.ok());
    ASSERT_FALSE(written.diagnostics.empty());
    EXPECT_EQ(written.diagnostics[0].sourceOffset, 0xC0U);
}

TEST(MldCanonical, WriterRejectsReplacedReadOnlySa3dMotion) {
    auto file = MldParser{}.parseBytes(makeBaseMld());
    const auto original = std::make_shared<const spice::modeling::Animation::Motion>();
    spice::mld::model::MldMotionResource resource{};
    resource.sourceAddress = 0x200U;
    resource.blockOffset = 0x200U;
    resource.blockSize = 0x20U;
    resource.variants.push_back(spice::mld::model::MldMotionVariant{
        .motion = std::make_shared<const spice::modeling::Animation::Motion>(),
        .originalMotion = original,
    });
    file.motionResources.emplace(resource.sourceAddress, std::move(resource));

    const auto written = MldFileWriter{}.write(file);
    EXPECT_FALSE(written.ok());
    ASSERT_FALSE(written.diagnostics.empty());
    EXPECT_NE(written.diagnostics.front().message.find("read-only"), std::string::npos);
}

TEST(MldCanonical, CompatibilityParseMatchesExplicitProjection) {
    const auto bytes = makeGrndMld();
    const MldParser parser{};
    const auto file = parser.parseBytes(bytes);
    const auto projected = parser.project(file);
    const auto compatibility = parser.parse(bytes);
    ASSERT_EQ(projected.entryList.size(), compatibility.entryList.size());
    ASSERT_EQ(projected.world.grndSurfaces.size(), compatibility.world.grndSurfaces.size());
    ASSERT_TRUE(projected.blenderIrScene.has_value());
    ASSERT_TRUE(compatibility.blenderIrScene.has_value());
    EXPECT_EQ(
        spice::mld::exporting::BlenderIrJsonExporter{}.toJson(*projected.blenderIrScene),
        spice::mld::exporting::BlenderIrJsonExporter{}.toJson(*compatibility.blenderIrScene));
}

TEST(GrndTranslationCorpus, A103bCanonicalWorldAndBlenderMeshesUseAuthoredSetTranslations) {
    const std::filesystem::path path =
        "D:/SoAGC/2002-12-19-gc-us-final_Skies_of_Arcadia_Legends/field/a103b.mld";
    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "US GameCube A103B.MLD is not available on this machine";
    }

    const auto source = readBytes(path);
    const MldParser parser{};
    const auto file = parser.parseBytes(source);
    ASSERT_EQ(file.sourcePlatform, spice::mld::model::TargetPlatform::GameCube);
    const auto& lower = *file.groundResources.at(0x00009580U).grnd;
    const auto& ramp = *file.groundResources.at(0x00010560U).grnd;
    const auto& upper = *file.groundResources.at(0x00016160U).grnd;

    ASSERT_GE(lower.triangleSets.size(), 4U);
    ASSERT_GE(ramp.triangleSets.size(), 5U);
    EXPECT_NEAR(lower.triangleSets[2].localToResourceTranslation.y, 83.5783691F, 0.0001F);
    EXPECT_FLOAT_EQ(ramp.triangleSets[2].localToResourceTranslation.x, -95.0F);
    EXPECT_FLOAT_EQ(ramp.triangleSets[2].localToResourceTranslation.y, 133.0F);
    EXPECT_FLOAT_EQ(ramp.triangleSets[2].localToResourceTranslation.z, -231.5F);
    EXPECT_FLOAT_EQ(upper.triangleSets[2].localToResourceTranslation.y, 0.0F);

    const auto lowerBounds = positionBounds(lower.mesh.vertices);
    const auto rampBounds = positionBounds(ramp.mesh.vertices);
    const auto upperBounds = positionBounds(upper.mesh.vertices);
    EXPECT_NEAR(lowerBounds[2], 80.0F, 0.001F);
    EXPECT_NEAR(lowerBounds[3], 80.0F, 0.001F);
    EXPECT_NEAR(rampBounds[0], -154.808319F, 0.001F);
    EXPECT_NEAR(rampBounds[1], 165.0F, 0.001F);
    EXPECT_NEAR(rampBounds[2], 80.0F, 0.001F);
    EXPECT_NEAR(rampBounds[3], 136.0F, 0.001F);
    EXPECT_NEAR(rampBounds[4], -294.5F, 0.001F);
    EXPECT_NEAR(rampBounds[5], -124.973503F, 0.001F);
    EXPECT_NEAR(upperBounds[2], 136.0F, 0.001F);
    EXPECT_NEAR(upperBounds[3], 136.0F, 0.001F);

    const auto projected = parser.project(file);
    const auto surface = std::find_if(projected.world.grndSurfaces.begin(), projected.world.grndSurfaces.end(),
        [](const auto& item) { return item.id == 0x00010560U; });
    ASSERT_NE(surface, projected.world.grndSurfaces.end());
    const auto surfaceBounds = positionBounds(surface->mesh.vertices);
    EXPECT_EQ(surfaceBounds, rampBounds);

    ASSERT_TRUE(projected.blenderIrScene.has_value());
    const auto& scene = *projected.blenderIrScene;
    const auto irMesh = std::find_if(scene.meshes.begin(), scene.meshes.end(),
        [](const auto& item) { return item.sourceObjectAddress == 0x00010560U; });
    ASSERT_NE(irMesh, scene.meshes.end());
    const auto irBounds = positionBounds(irMesh->vertices);
    EXPECT_EQ(irBounds, rampBounds);

    const auto unchanged = MldFileWriter{}.write(file);
    ASSERT_TRUE(unchanged.ok());
    EXPECT_EQ(unchanged.bytes, source);
}

TEST(GrndTranslationCorpus, DreamcastA103bArchivesRetainNonzeroSetTranslationsAndNoEditBytes) {
    const std::array paths{
        std::filesystem::path{ R"(D:\SoADC\SoA(Eu)Disc1Assets\FIELD\A103B.MLD)" },
        std::filesystem::path{ R"(D:\SoADC\SoA(Usa)Disc1Assets\FIELD\A103B.MLD)" },
    };
    if (!std::filesystem::exists(paths[0]) || !std::filesystem::exists(paths[1])) {
        GTEST_SKIP() << "Dreamcast A103B.MLD corpus files are not available on this machine";
    }

    std::size_t nonzeroTranslations = 0U;
    std::size_t translatedTriangles = 0U;
    for (const auto& path : paths) {
        const auto source = readBytes(path);
        const auto file = MldParser{}.parseBytes(source);
        ASSERT_EQ(file.sourcePlatform, spice::mld::model::TargetPlatform::Dreamcast);
        for (const auto& [address, resource] : file.groundResources) {
            (void)address;
            if (!resource.grnd.has_value()) {
                continue;
            }
            for (const auto& set : resource.grnd->triangleSets) {
                if (set.localToResourceTranslation.x != 0.0F ||
                    set.localToResourceTranslation.y != 0.0F ||
                    set.localToResourceTranslation.z != 0.0F) {
                    ++nonzeroTranslations;
                    translatedTriangles += set.declaredTriangleCount;
                }
            }
        }
        const auto unchanged = MldFileWriter{}.write(file);
        ASSERT_TRUE(unchanged.ok());
        EXPECT_EQ(unchanged.bytes, source);
    }
    EXPECT_GT(nonzeroTranslations, 0U);
    EXPECT_GT(translatedTriangles, 0U);
    std::cout << "Dreamcast A103B translated sets=" << nonzeroTranslations
              << " declared triangles=" << translatedTriangles << '\n';
}

TEST(MldDocument, ImportsNeutralEditableStructureAndWritesWithExplicitTarget) {
    const auto source = makeBaseMld();
    auto imported = spice::mld::MldDocumentImporter::importBytes(source);
    ASSERT_TRUE(imported.ok());
    ASSERT_TRUE(imported.document.has_value());
    EXPECT_EQ(imported.receipt.platform, spice::mld::MldPlatform::GameCube);
    EXPECT_EQ(imported.receipt.wrapper, spice::mld::MldWrapper::Raw);
    ASSERT_EQ(imported.document->entries.size(), 1U);
    EXPECT_TRUE(imported.document->entries.front().id);
    EXPECT_EQ(imported.document->allocateEntryId().value, 2U);
    EXPECT_FALSE(imported.document->layout.empty());

    const auto projection = spice::mld::MldBlenderIrProjector::project(*imported.document);
    EXPECT_TRUE(projection.ok());

    imported.document->entries.front().functionName = "edited";
    const spice::mld::MldWriteTarget target{
        .platform = spice::mld::MldPlatform::GameCube,
        .wrapper = spice::mld::MldWrapper::Raw,
    };
    const auto validation = spice::mld::MldDocumentValidator::validate(
        *imported.document, target, &imported.receipt);
    ASSERT_TRUE(validation.ok());
    const auto written = spice::mld::MldDocumentWriter::write(
        *imported.document, target, &imported.receipt);
    ASSERT_TRUE(written.ok());

    const auto reparsed = spice::mld::MldDocumentImporter::importBytes(written.bytes);
    ASSERT_TRUE(reparsed.ok());
    ASSERT_TRUE(reparsed.document.has_value());
    EXPECT_EQ(reparsed.document->entries.front().functionName, "edited");
}

TEST(MldDocument, RequiresReceiptForOpaquePreservingWrites) {
    spice::mld::MldDocument document{};
    document.entries.push_back({ .id = spice::mld::MldEntryId{ 1U } });
    document.opaqueMembers.push_back({
        .id = spice::mld::MldOpaqueMemberId{ 1U },
        .role = "unknown",
        .payload = { { 1U, 2U, 3U } },
    });
    document.layout.push_back(document.entries.front().id);
    document.layout.push_back(document.opaqueMembers.front().id);
    const auto result = spice::mld::MldDocumentWriter::write(document, {
        .platform = spice::mld::MldPlatform::Dreamcast,
        .wrapper = spice::mld::MldWrapper::Raw,
    });
    EXPECT_FALSE(result.ok());
}

TEST(MldDocument, ConstructivelyWritesFullyDecodedContentWithoutAReceipt) {
    spice::mld::MldDocument document{};
    spice::mld::MldEntry entry{};
    entry.id = spice::mld::MldEntryId{ 1U };
    entry.entryId = 42U;
    entry.tableId = -7;
    entry.functionName = "constructive";
    document.entries.push_back(entry);
    document.layout.push_back(entry.id);

    const auto dreamcast = spice::mld::MldDocumentWriter::write(document, {
        .platform = spice::mld::MldPlatform::Dreamcast,
        .wrapper = spice::mld::MldWrapper::Raw,
    });
    ASSERT_TRUE(dreamcast.ok());
    const auto dreamcastParsed = spice::mld::MldDocumentImporter::importBytes(dreamcast.bytes);
    ASSERT_TRUE(dreamcastParsed.ok());
    ASSERT_TRUE(dreamcastParsed.document.has_value());
    ASSERT_EQ(dreamcastParsed.document->entries.size(), 1U);
    EXPECT_EQ(dreamcastParsed.document->entries.front().entryId, 42U);
    EXPECT_EQ(dreamcastParsed.document->entries.front().tableId, -7);
    EXPECT_EQ(dreamcastParsed.document->entries.front().functionName, "constructive");

    const auto gameCube = spice::mld::MldDocumentWriter::write(document, {
        .platform = spice::mld::MldPlatform::GameCube,
        .wrapper = spice::mld::MldWrapper::Aklz,
    });
    ASSERT_TRUE(gameCube.ok());
    const auto gameCubeParsed = spice::mld::MldDocumentImporter::importBytes(gameCube.bytes);
    ASSERT_TRUE(gameCubeParsed.ok());
    ASSERT_TRUE(gameCubeParsed.document.has_value());
    ASSERT_EQ(gameCubeParsed.document->entries.size(), 1U);
    EXPECT_EQ(gameCubeParsed.document->entries.front().entryId, 42U);
    EXPECT_EQ(gameCubeParsed.document->entries.front().tableId, -7);
    EXPECT_EQ(gameCubeParsed.document->entries.front().functionName, "constructive");
}

TEST(MldDocument, EditsSourceNeutralGrndContentAndRebuildsIt) {
    auto imported = spice::mld::MldDocumentImporter::importBytes(makeGrndMld());
    ASSERT_TRUE(imported.ok());
    ASSERT_TRUE(imported.document.has_value());
    ASSERT_EQ(imported.document->grounds.size(), 1U);
    auto* ground = std::get_if<spice::mld::MldGrndDocument>(&imported.document->grounds.front().payload);
    ASSERT_NE(ground, nullptr);
    ASSERT_EQ(ground->mesh.indices.size(), 3U);
    const auto originalIndices = ground->mesh.indices;
    ground->mesh.indices.insert(ground->mesh.indices.end(), originalIndices.begin(), originalIndices.end());
    ground->mesh.triangleMetadata.push_back(ground->mesh.triangleMetadata.front());
    ASSERT_FALSE(ground->cells.empty());
    ground->cells.front().triangleIndices.push_back(1U);

    const auto written = spice::mld::MldDocumentWriter::write(*imported.document, {
        .platform = spice::mld::MldPlatform::GameCube,
        .wrapper = spice::mld::MldWrapper::Raw,
    }, &imported.receipt);
    ASSERT_TRUE(written.ok());
    const auto reparsed = spice::mld::MldDocumentImporter::importBytes(written.bytes);
    ASSERT_TRUE(reparsed.ok());
    ASSERT_TRUE(reparsed.document.has_value());
    ASSERT_EQ(reparsed.document->grounds.size(), 1U);
    const auto* rebuilt = std::get_if<spice::mld::MldGrndDocument>(&reparsed.document->grounds.front().payload);
    ASSERT_NE(rebuilt, nullptr);
    EXPECT_EQ(rebuilt->mesh.indices.size(), 6U);
}

TEST(MldDocument, ConstructivelyWritesAndRelocatesEditedTextureLists) {
    spice::mld::MldDocument document{};
    spice::mld::MldEntry entry{};
    entry.id = spice::mld::MldEntryId{ 1U };
    entry.entryId = 9U;
    entry.textureList = spice::mld::MldTextureListId{ 1U };
    document.entries.push_back(entry);
    document.textureLists.push_back({
        .id = spice::mld::MldTextureListId{ 1U },
        .names = { "first", "second" },
    });
    document.layout = { entry.id, spice::mld::MldTextureListId{ 1U } };

    const auto initial = spice::mld::MldDocumentWriter::write(document, {
        .platform = spice::mld::MldPlatform::Dreamcast,
        .wrapper = spice::mld::MldWrapper::Raw,
    });
    ASSERT_TRUE(initial.ok());
    auto imported = spice::mld::MldDocumentImporter::importBytes(initial.bytes);
    ASSERT_TRUE(imported.ok());
    ASSERT_TRUE(imported.document.has_value());
    ASSERT_EQ(imported.document->textureLists.size(), 1U);
    EXPECT_EQ(imported.document->textureLists.front().names,
        (std::vector<std::string>{ "first", "second" }));

    imported.document->textureLists.front().names.front() = std::string(80U, 'x');
    const auto edited = spice::mld::MldDocumentWriter::write(*imported.document, {
        .platform = spice::mld::MldPlatform::Dreamcast,
        .wrapper = spice::mld::MldWrapper::Raw,
    }, &imported.receipt);
    ASSERT_TRUE(edited.ok());
    const auto reparsed = spice::mld::MldDocumentImporter::importBytes(edited.bytes);
    ASSERT_TRUE(reparsed.ok());
    ASSERT_TRUE(reparsed.document.has_value());
    ASSERT_EQ(reparsed.document->textureLists.size(), 1U);
    EXPECT_EQ(reparsed.document->textureLists.front().names.front(), std::string(80U, 'x'));
    EXPECT_EQ(reparsed.document->textureLists.front().names.back(), "second");
}

TEST(MldMotionFrameProjector, ReportsIntrinsicSlotFailuresWithoutInferringAnOwner) {
    spice::mld::MldDocument document{};
    document.entries.push_back({
        .id = spice::mld::MldEntryId{ 1U },
        .motionSlots = {
            std::nullopt,
            spice::mld::MldMotionId{ 99U },
            spice::mld::MldMotionId{ 1U },
            spice::mld::MldMotionId{ 2U },
        },
    });
    document.motions.push_back({
        .id = spice::mld::MldMotionId{ 1U },
        .payload = spice::mld::MldOpaquePayload{ { 1U } },
    });
    document.motions.push_back({
        .id = spice::mld::MldMotionId{ 2U },
        .payload = spice::mld::MldDecodedMotion{ .kind = spice::modeling::MotionKind::Node },
    });

    const auto absent = spice::mld::MldMotionFrameProjector::project(
        document, spice::mld::MldEntryId{ 2U });
    EXPECT_FALSE(absent.entryId.has_value());
    EXPECT_TRUE(absent.slots.empty());
    EXPECT_FALSE(absent.ok());

    const auto projected = spice::mld::MldMotionFrameProjector::project(
        document, spice::mld::MldEntryId{ 1U });
    ASSERT_EQ(projected.slots.size(), 4U);
    EXPECT_EQ(projected.slots[0].status, spice::mld::MldMotionFrameSlotStatus::EmptySlot);
    EXPECT_EQ(projected.slots[1].status, spice::mld::MldMotionFrameSlotStatus::MissingMotion);
    EXPECT_EQ(projected.slots[2].status, spice::mld::MldMotionFrameSlotStatus::OpaqueMotion);
    EXPECT_EQ(projected.slots[3].status, spice::mld::MldMotionFrameSlotStatus::NoDecodedVariants);
    EXPECT_FALSE(projected.ok());
}

TEST(MldDocumentCorpus, ImportsAndReemitsRequestedBattleModelsAcrossPlatformsAndRegions) {
    if (!spice::tests::corpusTestsEnabled(spice::tests::CorpusFileType::Mld)) {
        GTEST_SKIP() << spice::tests::corpusTestsOptInMessage(spice::tests::CorpusFileType::Mld);
    }

    struct Case {
        std::filesystem::path path;
        spice::mld::MldPlatform platform;
    };
    const std::array cases{
        Case{ R"(D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends\bchara\ma000.mld)", spice::mld::MldPlatform::GameCube },
        Case{ R"(D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends\bchara\ma001.mld)", spice::mld::MldPlatform::GameCube },
        Case{ R"(D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends\bchara\MB000.mld)", spice::mld::MldPlatform::GameCube },
        Case{ R"(D:\SoAGC\2003-03-05-gc-eu-final_Skies_of_Arcadia_Legends\bchara\ma000.mld)", spice::mld::MldPlatform::GameCube },
        Case{ R"(D:\SoAGC\2003-03-05-gc-eu-final_Skies_of_Arcadia_Legends\bchara\ma001.mld)", spice::mld::MldPlatform::GameCube },
        Case{ R"(D:\SoAGC\2003-03-05-gc-eu-final_Skies_of_Arcadia_Legends\bchara\MB000.mld)", spice::mld::MldPlatform::GameCube },
        Case{ R"(D:\SoAGC\2002-11-12-gc-jp-final_Eternal_Arcadia_Legends\bchara\ma000.mld)", spice::mld::MldPlatform::GameCube },
        Case{ R"(D:\SoAGC\2002-11-12-gc-jp-final_Eternal_Arcadia_Legends\bchara\ma001.mld)", spice::mld::MldPlatform::GameCube },
        Case{ R"(D:\SoAGC\2002-11-12-gc-jp-final_Eternal_Arcadia_Legends\bchara\MB000.mld)", spice::mld::MldPlatform::GameCube },
        Case{ R"(D:\SoADC\SoA(Usa)Disc1Assets\BCHARA\MA000.MLD)", spice::mld::MldPlatform::Dreamcast },
        Case{ R"(D:\SoADC\SoA(Usa)Disc1Assets\BCHARA\MA001.MLD)", spice::mld::MldPlatform::Dreamcast },
        Case{ R"(D:\SoADC\SoA(Usa)Disc1Assets\BCHARA\MB000.MLD)", spice::mld::MldPlatform::Dreamcast },
        Case{ R"(D:\SoADC\SoA(Eu)Disc1Assets\BCHARA\MA000.MLD)", spice::mld::MldPlatform::Dreamcast },
        Case{ R"(D:\SoADC\SoA(Eu)Disc1Assets\BCHARA\MA001.MLD)", spice::mld::MldPlatform::Dreamcast },
        Case{ R"(D:\SoADC\SoA(Eu)Disc1Assets\BCHARA\MB000.MLD)", spice::mld::MldPlatform::Dreamcast },
        Case{ R"(D:\SoADC\SoA(JP)Disc1\Track 03\ETERNAL_ARCADIA_DISC1\BCHARA\MA000.MLD)", spice::mld::MldPlatform::Dreamcast },
        Case{ R"(D:\SoADC\SoA(JP)Disc1\Track 03\ETERNAL_ARCADIA_DISC1\BCHARA\MA001.MLD)", spice::mld::MldPlatform::Dreamcast },
        Case{ R"(D:\SoADC\SoA(JP)Disc1\Track 03\ETERNAL_ARCADIA_DISC1\BCHARA\MB000.MLD)", spice::mld::MldPlatform::Dreamcast },
    };
    if (std::any_of(cases.begin(), cases.end(), [](const auto& item) {
            return !std::filesystem::exists(item.path);
        })) {
        GTEST_SKIP() << "The six requested battle-model corpus roots are not all available";
    }

    for (const auto& item : cases) {
        SCOPED_TRACE(item.path.string());
        const auto source = readBytes(item.path);
        const auto imported = spice::mld::MldDocumentImporter::importBytes(source);
        ASSERT_TRUE(imported.ok());
        ASSERT_TRUE(imported.document.has_value());
        EXPECT_EQ(imported.receipt.platform, item.platform);
        const auto written = spice::mld::MldDocumentWriter::write(
            *imported.document,
            { .platform = item.platform, .wrapper = imported.receipt.wrapper },
            &imported.receipt);
        ASSERT_TRUE(written.ok());
        if (item.platform == spice::mld::MldPlatform::GameCube) {
            const auto sourceDecoded = spice::compression::aklz::decompress(source);
            const auto writtenDecoded = spice::compression::aklz::decompress(written.bytes);
            ASSERT_TRUE(sourceDecoded.ok());
            ASSERT_TRUE(writtenDecoded.ok());
            EXPECT_EQ(writtenDecoded.bytes, sourceDecoded.bytes);
        } else {
            EXPECT_EQ(written.bytes, source);
        }
    }
}

TEST(MldDocumentCorpus, ProjectsExactFirstBattleMotionFrameCountsFromExplicitOwners) {
    if (!spice::tests::corpusTestsEnabled(spice::tests::CorpusFileType::Mld)) {
        GTEST_SKIP() << spice::tests::corpusTestsOptInMessage(spice::tests::CorpusFileType::Mld);
    }

    using Expected = std::pair<std::size_t, std::uint32_t>;
    const std::vector<Expected> ma000{
        {0U,33U},{1U,20U},{2U,20U},{3U,43U},{4U,61U},{5U,30U},{6U,61U},{7U,30U},{8U,30U},{9U,32U},
        {10U,40U},{11U,68U},{12U,64U},{13U,81U},{14U,32U},{15U,20U},{16U,86U},{17U,70U},{18U,60U},{19U,80U},
        {20U,21U},{21U,83U},{22U,21U},{23U,86U},{24U,86U},{25U,86U},{26U,86U},{27U,86U},{28U,86U},{29U,86U},
        {30U,86U},{31U,86U},{32U,86U},{33U,86U},{34U,86U},{35U,86U},{36U,86U},{37U,86U},{38U,86U},{39U,86U},
        {40U,86U},{41U,86U},{42U,86U},{43U,86U},{44U,86U},{45U,86U},{46U,86U},{47U,86U},{48U,86U},{49U,86U},
        {50U,86U},{51U,86U},{52U,86U},{53U,86U},{54U,86U},{55U,86U},{56U,86U},{57U,86U},{58U,86U},{59U,214U},
        {60U,183U},{61U,151U},{62U,100U},{63U,102U},{64U,60U},{65U,112U},{66U,41U},{67U,21U},{68U,33U},{69U,33U},
        {70U,20U},{71U,20U},{72U,33U},{73U,20U},
    };
    const std::vector<Expected> ma001{
        {0U,32U},{1U,20U},{2U,20U},{3U,39U},{4U,61U},{5U,30U},{6U,51U},{7U,30U},{8U,30U},{9U,21U},
        {10U,40U},{11U,46U},{12U,75U},{13U,70U},{14U,32U},{15U,20U},{16U,86U},{17U,51U},{18U,79U},{19U,71U},
        {20U,21U},{21U,81U},{22U,21U},{23U,86U},{24U,86U},{25U,86U},{26U,86U},{27U,86U},{28U,86U},{29U,86U},
        {30U,86U},{31U,86U},{32U,86U},{33U,86U},{34U,86U},{35U,86U},{36U,86U},{37U,86U},{38U,86U},{39U,86U},
        {40U,86U},{41U,86U},{42U,86U},{43U,86U},{44U,86U},{45U,86U},{46U,86U},{47U,86U},{48U,86U},{49U,86U},
        {50U,86U},{51U,86U},{52U,86U},{53U,86U},{54U,86U},{55U,86U},{56U,86U},{57U,86U},{58U,86U},{59U,376U},
        {60U,190U},{61U,81U},{62U,100U},{63U,95U},{64U,78U},{65U,41U},{66U,21U},{67U,32U},{68U,32U},{69U,20U},
        {70U,20U},{71U,32U},{72U,20U},
    };
    const std::vector<Expected> mb000{
        {0U,32U},{1U,20U},{2U,20U},{3U,40U},{4U,50U},{5U,31U},{6U,40U},{7U,31U},{8U,31U},{9U,35U},
        {10U,40U},{11U,20U},{12U,60U},{13U,61U},{14U,61U},{15U,61U},{16U,32U},{17U,32U},{18U,20U},{19U,20U},
        {20U,32U},{21U,20U},
    };
    struct Case {
        std::filesystem::path path;
        std::string_view sha256;
        const std::vector<Expected>* expected;
    };
    const std::array cases{
        Case{ R"(D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends\bchara\ma000.mld)", "69e602e7a445111dbb5ee1c7062909769d443797b1076c7c9c4c4929f2a35f71", &ma000 },
        Case{ R"(D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends\bchara\ma001.mld)", "592e6873eca7778175522e19a730e4280808575bde28c1f2fb4d18ee5a442871", &ma001 },
        Case{ R"(D:\SoAGC\2002-12-19-gc-us-final_Skies_of_Arcadia_Legends\bchara\MB000.mld)", "6e1338b26e70fca208d81932eb93ec126b9fa231fb77dae19dea104fcfa67dd7", &mb000 },
    };
    if (std::any_of(cases.begin(), cases.end(), [](const auto& item) { return !std::filesystem::exists(item.path); })) {
        GTEST_SKIP() << "The US GameCube first-battle MLD corpus is unavailable";
    }

    for (const auto& item : cases) {
        SCOPED_TRACE(item.path.string());
        const auto imported = spice::mld::MldDocumentImporter::importFile(item.path);
        ASSERT_TRUE(imported.ok());
        ASSERT_TRUE(imported.document.has_value());
        ASSERT_EQ(imported.document->entries.size(), 1U);
        EXPECT_EQ(imported.receipt.sourceSha256, digestFromHex(item.sha256));

        const auto projection = spice::mld::MldMotionFrameProjector::project(
            *imported.document, imported.document->entries.front().id);
        ASSERT_TRUE(projection.entryId.has_value());
        ASSERT_EQ(projection.slots.size(), item.expected->size());
        EXPECT_TRUE(projection.ok());
        for (std::size_t index = 0U; index < item.expected->size(); ++index) {
            const auto& expected = item.expected->at(index);
            const auto& actual = projection.slots[index];
            EXPECT_EQ(actual.slotOrdinal, expected.first);
            ASSERT_TRUE(actual.motionId.has_value());
            ASSERT_TRUE(actual.agreedDeclaredFrameCount.has_value());
            EXPECT_EQ(*actual.agreedDeclaredFrameCount, expected.second);
            EXPECT_EQ(actual.status, spice::mld::MldMotionFrameSlotStatus::Resolved);
            ASSERT_FALSE(actual.variants.empty());
            for (const auto& variant : actual.variants) {
                EXPECT_TRUE(variant.variantId);
                EXPECT_EQ(variant.declaredFrameCount, expected.second);
            }
        }
        std::set<std::uint64_t> variantIds{};
        for (const auto& motion : imported.document->motions) {
            const auto* decoded = std::get_if<spice::mld::MldDecodedMotion>(&motion.payload);
            if (decoded == nullptr) continue;
            for (const auto& variant : decoded->variants) {
                EXPECT_TRUE(variant.id);
                EXPECT_TRUE(variantIds.insert(variant.id.value).second);
            }
        }
        ASSERT_FALSE(variantIds.empty());
        EXPECT_EQ(imported.document->allocateMotionVariantId().value, *variantIds.rbegin() + 1U);
    }
}
