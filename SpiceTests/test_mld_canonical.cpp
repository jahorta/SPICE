#include "../SpiceMLD/SpiceMLD.h"
#include "../Compression/Aklz.h"
#include "../Sa3Dport/Sa3Dport.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <vector>

namespace {

using spice::core::Endian;
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

void writeF32(std::vector<std::uint8_t>& bytes, const std::size_t offset, const float value) {
    writeU32(bytes, offset, std::bit_cast<std::uint32_t>(value));
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

std::vector<std::uint8_t> makeGrndMld() {
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
    writeU16(bytes, address + 0x20U, 1U);
    writeU16(bytes, address + 0x22U, 1U);
    writeU16(bytes, address + 0x24U, 10U);
    writeU16(bytes, address + 0x26U, 10U);
    writeU16(bytes, address + 0x28U, 1U);
    writeU16(bytes, address + 0x2AU, 1U);
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

std::vector<std::uint8_t> makeGobjMld() {
    constexpr std::uint32_t address = 0x180U;
    auto bytes = makeBaseMld(address, true);
    constexpr std::size_t node = 0x10U;
    constexpr std::size_t attach = 0x50U;
    constexpr std::size_t payload = attach + 0x10U;
    constexpr std::size_t stream = payload + 76U;
    constexpr std::size_t vertices = 0xBCU;
    constexpr std::size_t size = vertices + 8U + 3U * 24U;
    writeTag(bytes, address, "GOBJ");
    writeU32(bytes, address + 4U, static_cast<std::uint32_t>(size));
    writeU32(bytes, address + node, static_cast<std::uint32_t>(attach - node));
    writeF32(bytes, address + node + 0x20U, 1.0F);
    writeF32(bytes, address + node + 0x24U, 1.0F);
    writeF32(bytes, address + node + 0x28U, 1.0F);
    writeU32(bytes, address + payload, static_cast<std::uint32_t>(vertices - payload));
    for (std::size_t i = 0; i < 3U; ++i) {
        writeU16(bytes, address + stream + i * 4U, static_cast<std::uint16_t>(2U + i * 6U));
        writeU16(bytes, address + stream + i * 4U + 2U, static_cast<std::uint16_t>(i + 1U));
    }
    writeU16(bytes, address + stream + 12U, 0xFFFFU);
    writeU16(bytes, address + stream + 14U, 0xFFFFU);
    writeU32(bytes, address + vertices, 0x29U);
    writeU32(bytes, address + vertices + 4U, 3U << 16U);
    for (std::size_t i = 0; i < 3U; ++i) {
        const auto vertex = address + vertices + 8U + i * 24U;
        writeF32(bytes, vertex, static_cast<float>(i == 1U));
        writeF32(bytes, vertex + 4U, 0.0F);
        writeF32(bytes, vertex + 8U, static_cast<float>(i == 2U));
        writeF32(bytes, vertex + 12U, 0.0F);
        writeF32(bytes, vertex + 16U, 1.0F);
        writeF32(bytes, vertex + 20U, 0.0F);
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

} // namespace

TEST(MldCanonical, ParseBytesOwnsSharedListsAndCompleteSourceRanges) {
    const auto bytes = makeBaseMld();
    const auto file = MldParser{}.parseBytes(bytes);
    ASSERT_EQ(file.parseStatus, spice::mld::model::MldParseStatus::Complete)
        << describeDiagnostics(file);
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

TEST(MldCanonical, WrappedRealFixtureRetainsPartialDiagnosticsAndProjectsVisibleGeometry) {
    const auto fixture = findMldFixture("s044_sml_entry_0.mld");
    ASSERT_FALSE(fixture.empty());
    const auto file = MldParser{}.parseBytes(readBytes(fixture));
    ASSERT_EQ(file.parseStatus, spice::mld::model::MldParseStatus::Partial)
        << describeDiagnostics(file);
    EXPECT_TRUE(std::any_of(file.parseDiagnostics.begin(), file.parseDiagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.severity == spice::mld::model::MldDiagnostic::Severity::Warning
            && diagnostic.sourceOffset == 0x9CU;
    }));
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
    ASSERT_FALSE(fixture.empty());
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
    const auto original = std::make_shared<const Sa3Dport::Animation::Motion>();
    spice::mld::model::MldMotionResource resource{};
    resource.sourceAddress = 0x200U;
    resource.blockOffset = 0x200U;
    resource.blockSize = 0x20U;
    resource.variants.push_back(spice::mld::model::MldMotionVariant{
        .motion = std::make_shared<const Sa3Dport::Animation::Motion>(),
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
