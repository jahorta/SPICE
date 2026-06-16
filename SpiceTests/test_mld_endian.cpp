#include "../SpiceMLD/SpiceMLD.h"
#include "../SpiceGvm/SpiceGvm.h"
#include "../Compression/Aklz.h"

#include <gtest/gtest.h>

#include <bit>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <array>

namespace {

using spice::core::Endian;
using spice::mld::exporting::MldExportOptions;
using spice::mld::exporting::MldFileExporter;
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

std::filesystem::path findFixturePath(const std::filesystem::path& relativePath) {
    auto current = std::filesystem::current_path();
    while (true) {
        const auto fromRepoRoot = current / "SpiceTests" / relativePath;
        if (std::filesystem::exists(fromRepoRoot)) {
            return fromRepoRoot;
        }

        const auto fromTestsRoot = current / relativePath;
        if (std::filesystem::exists(fromTestsRoot)) {
            return fromTestsRoot;
        }

        const auto parent = current.parent_path();
        if (parent == current || parent.empty()) {
            break;
        }
        current = parent;
    }
    throw std::runtime_error("Could not locate test fixture: " + relativePath.string());
}

std::vector<std::uint8_t> readBinaryFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Could not open binary fixture: " + path.string());
    }
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
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

TEST(MldParser, ParsesWrappedS044SmlEntryObjectIntoBlenderIrGeometry) {
    const auto fixturePath = findFixturePath("fixtures/mld/s044_sml_entry_0.mld");
    const auto bytes = readBinaryFile(fixturePath);

    MldParser parser;
    const auto parsed = parser.parse(bytes);

    ASSERT_EQ(parsed.entryList.size(), 1U);
    ASSERT_EQ(parsed.entryList[0].objectAddresses.size(), 1U);
    EXPECT_EQ(parsed.entryList[0].objectAddresses[0], 0xC0U);

    const auto found = std::find_if(parsed.extractedNjBlocks.begin(), parsed.extractedNjBlocks.end(), [](const auto& block) {
        return block.sourceObjectAddress == 0xC0U;
    });
    ASSERT_NE(found, parsed.extractedNjBlocks.end());
    ASSERT_TRUE(found->modelBlockOffset.has_value());
    ASSERT_TRUE(found->modelReadOffset.has_value());
    EXPECT_EQ(*found->modelBlockOffset, 0x1F0U);
    EXPECT_EQ(*found->modelReadOffset, 0x130U);
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
