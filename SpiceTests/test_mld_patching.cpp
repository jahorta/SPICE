#include "../SpiceMLD/SpiceMLD.h"
#include "../SpiceMLD/Parsing/GobjParser.h"
#include "../SpiceMLD/Parsing/GrndParser.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using spice::core::Endian;
using spice::mld::model::MldFile;
using spice::mld::model::MldGroundResource;
using spice::mld::patching::DreamcastTriangleSelectorEdit;
using spice::mld::patching::TriangleResourceKind;

constexpr std::size_t kGrndAddress = 0x100U;
constexpr std::size_t kGobjAddress = 0x300U;
constexpr std::size_t kGrndStreamOffset = 0x60U;
constexpr std::size_t kGobjPolyOffset = 0xACU;

void writeU16(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void writeU32(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

void writeF32(std::vector<std::uint8_t>& bytes, const std::size_t offset, const float value) {
    writeU32(bytes, offset, std::bit_cast<std::uint32_t>(value));
}

void writeTag(std::vector<std::uint8_t>& bytes, const std::size_t offset, const char* tag) {
    for (std::size_t i = 0; i < 4U; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(tag[i]);
    }
}

std::vector<std::uint8_t> makeDreamcastGrnd() {
    constexpr std::size_t innerHeader = 0x10U;
    constexpr std::size_t triangleSetsOffset = 0x40U;
    constexpr std::size_t vertexOffset = 0x80U;
    constexpr std::size_t quadRegistryOffset = 0xC8U;
    constexpr std::size_t quadTableOffset = quadRegistryOffset + 4U;
    constexpr std::size_t refListOffset = 0xDCU;
    constexpr std::size_t declaredSize = 0xE0U;

    std::vector<std::uint8_t> bytes(declaredSize, 0U);
    writeTag(bytes, 0U, "GRND");
    writeU32(bytes, 4U, static_cast<std::uint32_t>(declaredSize));
    writeU32(bytes, innerHeader, static_cast<std::uint32_t>(triangleSetsOffset - innerHeader));
    writeU32(bytes, innerHeader + 4U, static_cast<std::uint32_t>(quadRegistryOffset - innerHeader));
    writeU16(bytes, innerHeader + 0x10U, 1U);
    writeU16(bytes, innerHeader + 0x12U, 1U);
    writeU16(bytes, innerHeader + 0x14U, 1U);
    writeU16(bytes, innerHeader + 0x16U, 1U);
    writeU16(bytes, innerHeader + 0x18U, 1U);
    writeU16(bytes, innerHeader + 0x1AU, 1U);

    writeU32(bytes, triangleSetsOffset + 0x0CU,
        static_cast<std::uint32_t>(vertexOffset - (triangleSetsOffset + 0x0CU)));
    writeU32(bytes, triangleSetsOffset + 0x10U,
        static_cast<std::uint32_t>(kGrndStreamOffset - (triangleSetsOffset + 0x10U)));
    writeU32(bytes, triangleSetsOffset + 0x14U, 1U);
    constexpr std::array<std::uint16_t, 3> flags{ 1U, 2U, 0x800AU };
    for (std::size_t i = 0; i < 3U; ++i) {
        writeU16(bytes, kGrndStreamOffset + i * 4U, static_cast<std::uint16_t>(i * 6U));
        writeU16(bytes, kGrndStreamOffset + i * 4U + 2U, flags[i]);
        const auto vertex = vertexOffset + i * 24U;
        writeF32(bytes, vertex + 0U, static_cast<float>(i));
        writeF32(bytes, vertex + 4U, static_cast<float>(i == 1U));
        writeF32(bytes, vertex + 8U, static_cast<float>(i == 2U));
        writeF32(bytes, vertex + 16U, 1.0F);
    }
    writeU32(bytes, quadTableOffset, 1U);
    writeU32(bytes, quadTableOffset + 4U,
        static_cast<std::uint32_t>(refListOffset - (quadTableOffset + 4U)));
    writeU16(bytes, refListOffset, 0U);
    writeU16(bytes, refListOffset + 2U, 0U);
    return bytes;
}

std::vector<std::uint8_t> makeDreamcastGobj() {
    constexpr std::size_t nodeOffset = 0x10U;
    constexpr std::size_t attachOffset = 0x50U;
    constexpr std::size_t payloadOffset = attachOffset + 0x10U;
    constexpr std::size_t vertexOffset = 0xC0U;
    constexpr std::size_t vertexCount = 4U;
    constexpr std::size_t recordWords = 3U;
    const auto declaredSize = vertexOffset + 8U + vertexCount * recordWords * 4U;

    std::vector<std::uint8_t> bytes(declaredSize, 0U);
    writeTag(bytes, 0U, "GOBJ");
    writeU32(bytes, 4U, static_cast<std::uint32_t>(declaredSize));
    writeU32(bytes, nodeOffset, static_cast<std::uint32_t>(attachOffset - nodeOffset));
    writeF32(bytes, nodeOffset + 0x20U, 1.0F);
    writeF32(bytes, nodeOffset + 0x24U, 1.0F);
    writeF32(bytes, nodeOffset + 0x28U, 1.0F);
    writeU32(bytes, payloadOffset, static_cast<std::uint32_t>(vertexOffset - payloadOffset));

    constexpr std::array<std::uint16_t, 4> flags{ 1U, 2U, 0x800AU, 70U };
    for (std::size_t i = 0; i < vertexCount; ++i) {
        writeU16(bytes, kGobjPolyOffset + i * 4U, static_cast<std::uint16_t>(2U + i * recordWords));
        writeU16(bytes, kGobjPolyOffset + i * 4U + 2U, flags[i]);
    }
    writeU16(bytes, kGobjPolyOffset + vertexCount * 4U, 0xFFFFU);
    writeU16(bytes, kGobjPolyOffset + vertexCount * 4U + 2U, 0xFFFFU);

    writeU32(bytes, vertexOffset, 0x22U);
    writeU32(bytes, vertexOffset + 4U, static_cast<std::uint32_t>(vertexCount << 16U));
    for (std::size_t i = 0; i < vertexCount; ++i) {
        const auto vertex = vertexOffset + 8U + i * recordWords * 4U;
        writeF32(bytes, vertex + 0U, static_cast<float>(i));
        writeF32(bytes, vertex + 4U, static_cast<float>(i + 1U));
        writeF32(bytes, vertex + 8U, static_cast<float>(i + 2U));
    }
    return bytes;
}

MldFile makeMixedDreamcastFile() {
    const auto grndBytes = makeDreamcastGrnd();
    const auto gobjBytes = makeDreamcastGobj();
    MldFile file{};
    file.parseStatus = spice::mld::model::MldParseStatus::Complete;
    file.sourcePlatform = spice::mld::model::TargetPlatform::Dreamcast;
    file.endian = Endian::Little;
    file.sourceBytes.assign(kGobjAddress + gobjBytes.size() + 0x20U, 0xCCU);
    std::copy(grndBytes.begin(), grndBytes.end(), file.sourceBytes.begin() + static_cast<std::ptrdiff_t>(kGrndAddress));
    std::copy(gobjBytes.begin(), gobjBytes.end(), file.sourceBytes.begin() + static_cast<std::ptrdiff_t>(kGobjAddress));
    file.decodedBytes = file.sourceBytes;
    file.originalBytes = file.sourceBytes;

    auto grnd = spice::mld::parsing::GrndParser{}.decode(grndBytes, static_cast<std::uint32_t>(kGrndAddress), Endian::Little);
    EXPECT_TRUE(grnd.decoded);
    MldGroundResource grndResource{};
    grndResource.kind = MldGroundResource::Kind::Grnd;
    grndResource.sourceAddress = static_cast<std::uint32_t>(kGrndAddress);
    grndResource.blockSize = grndBytes.size();
    grndResource.tag = "GRND";
    grndResource.rawBytes = grndBytes;
    grndResource.grnd = std::move(grnd.data);
    grndResource.originalSemanticHash = spice::mld::model::semanticHash(*grndResource.grnd);
    file.groundResources.emplace(grndResource.sourceAddress, std::move(grndResource));

    auto gobj = spice::mld::parsing::GobjParser{}.decode(gobjBytes, static_cast<std::uint32_t>(kGobjAddress), Endian::Little);
    EXPECT_TRUE(gobj.decoded);
    MldGroundResource gobjResource{};
    gobjResource.kind = MldGroundResource::Kind::Gobj;
    gobjResource.sourceAddress = static_cast<std::uint32_t>(kGobjAddress);
    gobjResource.blockSize = gobjBytes.size();
    gobjResource.tag = "GOBJ";
    gobjResource.rawBytes = gobjBytes;
    gobjResource.gobj = std::move(gobj.data);
    gobjResource.originalSemanticHash = spice::mld::model::semanticHash(*gobjResource.gobj);
    file.groundResources.emplace(gobjResource.sourceAddress, std::move(gobjResource));
    return file;
}

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    input.seekg(0, std::ios::end);
    const auto length = input.tellg();
    input.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    return bytes;
}

std::uint8_t differentValidSelector(const std::uint16_t rawWord) {
    const auto low = static_cast<std::uint16_t>(rawWord & 0x7FFFU);
    const auto current = static_cast<std::uint16_t>((low / 10U) % 10U);
    for (std::uint16_t digit = 0U; digit <= 9U; ++digit) {
        const auto replacement = static_cast<std::uint32_t>(low) - current * 10U + digit * 10U;
        if (digit != current && replacement <= 0x7FFFU) {
            return static_cast<std::uint8_t>(digit);
        }
    }
    return static_cast<std::uint8_t>(current);
}

void hashWord(std::uint64_t& hash, const std::uint64_t value) {
    for (unsigned shift = 0U; shift < 64U; shift += 8U) {
        hash ^= static_cast<std::uint8_t>(value >> shift);
        hash *= 1099511628211ULL;
    }
}

} // namespace

TEST(MldPatching, ParsersRecordExactTriangleMetadataOffsets) {
    const auto grndBytes = makeDreamcastGrnd();
    const auto grnd = spice::mld::parsing::GrndParser{}.decode(
        grndBytes, static_cast<std::uint32_t>(kGrndAddress), Endian::Little);
    ASSERT_EQ(grnd.data.triangleSources.size(), 1U);
    EXPECT_EQ(grnd.data.triangleSources[0].triangleSet, 0U);
    EXPECT_EQ(grnd.data.triangleSources[0].streamIndex, 0U);
    EXPECT_EQ(grnd.data.triangleSources[0].flagSourceOffsets,
        (std::array<std::size_t, 3>{ kGrndAddress + kGrndStreamOffset + 2U,
            kGrndAddress + kGrndStreamOffset + 6U,
            kGrndAddress + kGrndStreamOffset + 10U }));

    const auto gobjBytes = makeDreamcastGobj();
    const auto gobj = spice::mld::parsing::GobjParser{}.decode(
        gobjBytes, static_cast<std::uint32_t>(kGobjAddress), Endian::Little);
    ASSERT_EQ(gobj.data.nodes.size(), 1U);
    ASSERT_EQ(gobj.data.nodes[0].streamTriangleSources.size(), 2U);
    EXPECT_EQ(gobj.data.nodes[0].streamTriangleSources[0].flagSourceOffsets[2],
        kGobjAddress + kGobjPolyOffset + 10U);
    EXPECT_EQ(gobj.data.nodes[0].streamTriangleSources[1].flagSourceOffsets[2],
        kGobjAddress + kGobjPolyOffset + 14U);
}

TEST(MldPatching, PlansAndAppliesMixedGrndAndGobjSelectorEdits) {
    const auto file = makeMixedDreamcastFile();
    const std::array edits{
        DreamcastTriangleSelectorEdit{
            .resourceKind = TriangleResourceKind::Grnd,
            .resourceAddress = static_cast<std::uint32_t>(kGrndAddress),
            .triangleIndex = 0U,
            .selectorDigit = 7U,
        },
        DreamcastTriangleSelectorEdit{
            .resourceKind = TriangleResourceKind::Gobj,
            .resourceAddress = static_cast<std::uint32_t>(kGobjAddress),
            .gobjNodeIndex = 0U,
            .triangleIndex = 0U,
            .selectorDigit = 9U,
        },
    };
    const auto plan = spice::mld::patching::planDreamcastTriangleSelectorPatches(file, edits);
    ASSERT_TRUE(plan.ok());
    ASSERT_EQ(plan.patches.size(), 2U);

    auto patched = file.sourceBytes;
    const auto applied = spice::mld::patching::applyMldPatchPlan(patched, plan);
    ASSERT_TRUE(applied.ok());
    EXPECT_EQ(applied.appliedPatchCount, 2U);
    EXPECT_EQ(patched.size(), file.sourceBytes.size());
    for (std::size_t i = 0; i < patched.size(); ++i) {
        const bool inPatch = std::any_of(plan.patches.begin(), plan.patches.end(), [&](const auto& patch) {
            return i >= patch.fileOffset && i < patch.fileOffset + 2U;
        });
        if (!inPatch) {
            EXPECT_EQ(patched[i], file.sourceBytes[i]) << "unexpected change at " << i;
        }
    }

    const auto reparsedGrnd = spice::mld::parsing::GrndParser{}.decode(
        std::span<const std::uint8_t>(patched).subspan(kGrndAddress, makeDreamcastGrnd().size()),
        static_cast<std::uint32_t>(kGrndAddress), Endian::Little);
    ASSERT_EQ(reparsedGrnd.data.mesh.triangleMetadata.size(), 1U);
    EXPECT_EQ((reparsedGrnd.data.mesh.triangleMetadata[0].rawU16[2] >> 15U) & 1U, 1U);
    EXPECT_EQ(((reparsedGrnd.data.mesh.triangleMetadata[0].rawU16[2] & 0x7FFFU) / 10U) % 10U, 7U);

    const auto reparsedGobj = spice::mld::parsing::GobjParser{}.decode(
        std::span<const std::uint8_t>(patched).subspan(kGobjAddress, makeDreamcastGobj().size()),
        static_cast<std::uint32_t>(kGobjAddress), Endian::Little);
    ASSERT_EQ(reparsedGobj.data.nodes[0].streamMesh.triangleMetadata.size(), 2U);
    EXPECT_EQ(((reparsedGobj.data.nodes[0].streamMesh.triangleMetadata[0].rawU16[2] & 0x7FFFU) / 10U) % 10U, 9U);
    EXPECT_EQ(((reparsedGobj.data.nodes[0].streamMesh.triangleMetadata[1].rawU16[2] & 0x7FFFU) / 10U) % 10U, 7U);
}

TEST(MldPatching, AllowsEverySelectorDigitWithoutAreaOrResourcePolicy) {
    const auto file = makeMixedDreamcastFile();
    for (std::uint8_t digit = 0U; digit <= 9U; ++digit) {
        const DreamcastTriangleSelectorEdit edit{
            .resourceKind = TriangleResourceKind::Gobj,
            .resourceAddress = static_cast<std::uint32_t>(kGobjAddress),
            .gobjNodeIndex = 0U,
            .triangleIndex = 0U,
            .selectorDigit = digit,
        };
        const auto plan = spice::mld::patching::planDreamcastTriangleSelectorPatches(file, std::span{ &edit, 1U });
        EXPECT_TRUE(plan.ok()) << "selector digit " << static_cast<unsigned>(digit);
    }
}

TEST(MldPatching, DeduplicatesIdenticalEditsAndRejectsConflicts) {
    const auto file = makeMixedDreamcastFile();
    const DreamcastTriangleSelectorEdit first{
        .resourceKind = TriangleResourceKind::Gobj,
        .resourceAddress = static_cast<std::uint32_t>(kGobjAddress),
        .gobjNodeIndex = 0U,
        .triangleIndex = 0U,
        .selectorDigit = 4U,
    };
    const std::array duplicates{ first, first };
    const auto duplicatePlan = spice::mld::patching::planDreamcastTriangleSelectorPatches(file, duplicates);
    ASSERT_TRUE(duplicatePlan.ok());
    EXPECT_EQ(duplicatePlan.patches.size(), 1U);

    auto second = first;
    second.selectorDigit = 5U;
    const std::array conflicts{ first, second };
    const auto conflictPlan = spice::mld::patching::planDreamcastTriangleSelectorPatches(file, conflicts);
    EXPECT_FALSE(conflictPlan.ok());
}

TEST(MldPatching, OmitsNoOpsAndRejectsLow15Overflow) {
    auto file = makeMixedDreamcastFile();
    DreamcastTriangleSelectorEdit edit{
        .resourceKind = TriangleResourceKind::Grnd,
        .resourceAddress = static_cast<std::uint32_t>(kGrndAddress),
        .triangleIndex = 0U,
        .selectorDigit = 1U,
    };
    const auto noOpPlan = spice::mld::patching::planDreamcastTriangleSelectorPatches(
        file, std::span{ &edit, 1U });
    ASSERT_TRUE(noOpPlan.ok());
    EXPECT_TRUE(noOpPlan.patches.empty());

    auto& resource = file.groundResources.at(static_cast<std::uint32_t>(kGrndAddress));
    auto& grnd = *resource.grnd;
    const auto absoluteOffset = grnd.triangleSources[0].flagSourceOffsets[2];
    const auto resourceOffset = absoluteOffset - resource.sourceAddress;
    grnd.mesh.triangleMetadata[0].rawU16[2] = 0x7FFFU;
    writeU16(file.sourceBytes, absoluteOffset, 0x7FFFU);
    writeU16(file.decodedBytes, absoluteOffset, 0x7FFFU);
    writeU16(resource.rawBytes, resourceOffset, 0x7FFFU);
    edit.selectorDigit = 9U;
    EXPECT_FALSE(spice::mld::patching::planDreamcastTriangleSelectorPatches(
        file, std::span{ &edit, 1U }).ok());
}

TEST(MldPatching, AppliesAtomicallyWhenExpectedBytesAreStale) {
    const auto file = makeMixedDreamcastFile();
    const std::array edits{
        DreamcastTriangleSelectorEdit{
            .resourceKind = TriangleResourceKind::Grnd,
            .resourceAddress = static_cast<std::uint32_t>(kGrndAddress),
            .triangleIndex = 0U,
            .selectorDigit = 7U,
        },
        DreamcastTriangleSelectorEdit{
            .resourceKind = TriangleResourceKind::Gobj,
            .resourceAddress = static_cast<std::uint32_t>(kGobjAddress),
            .gobjNodeIndex = 0U,
            .triangleIndex = 0U,
            .selectorDigit = 8U,
        },
    };
    const auto plan = spice::mld::patching::planDreamcastTriangleSelectorPatches(file, edits);
    ASSERT_TRUE(plan.ok());
    ASSERT_EQ(plan.patches.size(), 2U);

    auto stale = file.sourceBytes;
    stale[plan.patches[1].fileOffset] ^= 0x01U;
    const auto before = stale;
    const auto applied = spice::mld::patching::applyMldPatchPlan(stale, plan);
    EXPECT_FALSE(applied.ok());
    EXPECT_EQ(applied.appliedPatchCount, 0U);
    EXPECT_EQ(stale, before);
}

TEST(MldPatching, RejectsUnsupportedFilesAndStructurallyInvalidEdits) {
    auto file = makeMixedDreamcastFile();
    const DreamcastTriangleSelectorEdit edit{
        .resourceKind = TriangleResourceKind::Grnd,
        .resourceAddress = static_cast<std::uint32_t>(kGrndAddress),
        .triangleIndex = 0U,
        .selectorDigit = 10U,
    };
    EXPECT_FALSE(spice::mld::patching::planDreamcastTriangleSelectorPatches(file, std::span{ &edit, 1U }).ok());

    file.sourcePlatform = spice::mld::model::TargetPlatform::GameCube;
    file.endian = Endian::Big;
    EXPECT_FALSE(spice::mld::patching::planDreamcastTriangleSelectorPatches(file, {}).ok());

    file.sourcePlatform = spice::mld::model::TargetPlatform::Dreamcast;
    file.endian = Endian::Little;
    file.sourceWasCompressedAklz = true;
    EXPECT_FALSE(spice::mld::patching::planDreamcastTriangleSelectorPatches(file, {}).ok());
}

TEST(MldPatching, RejectsWrongResourceShapeAndOverlappingPatchRecords) {
    const auto file = makeMixedDreamcastFile();
    const std::array invalidEdits{
        DreamcastTriangleSelectorEdit{
            .resourceKind = TriangleResourceKind::Grnd,
            .resourceAddress = static_cast<std::uint32_t>(kGrndAddress),
            .gobjNodeIndex = 0U,
            .triangleIndex = 0U,
            .selectorDigit = 2U,
        },
        DreamcastTriangleSelectorEdit{
            .resourceKind = TriangleResourceKind::Gobj,
            .resourceAddress = static_cast<std::uint32_t>(kGobjAddress),
            .triangleIndex = 0U,
            .selectorDigit = 2U,
        },
    };
    EXPECT_FALSE(spice::mld::patching::planDreamcastTriangleSelectorPatches(file, invalidEdits).ok());

    spice::mld::patching::MldPatchPlan overlap{};
    overlap.patches.push_back(spice::mld::patching::MldBytePatch{
        .fileOffset = 4U,
        .expectedBytes = { 0U, 0U },
        .replacementBytes = { 1U, 0U },
    });
    overlap.patches.push_back(spice::mld::patching::MldBytePatch{
        .fileOffset = 5U,
        .expectedBytes = { 0U, 0U },
        .replacementBytes = { 2U, 0U },
    });
    std::vector<std::uint8_t> bytes(8U, 0U);
    const auto before = bytes;
    EXPECT_FALSE(spice::mld::patching::applyMldPatchPlan(bytes, overlap).ok());
    EXPECT_EQ(bytes, before);
}

TEST(MldPatching, RejectsStaleProvenanceAfterSemanticModelEdits) {
    auto file = makeMixedDreamcastFile();
    auto& grnd = *file.groundResources.at(static_cast<std::uint32_t>(kGrndAddress)).grnd;
    grnd.mesh.vertices[0].position.x += 1.0F;
    const DreamcastTriangleSelectorEdit edit{
        .resourceKind = TriangleResourceKind::Grnd,
        .resourceAddress = static_cast<std::uint32_t>(kGrndAddress),
        .triangleIndex = 0U,
        .selectorDigit = 2U,
    };
    EXPECT_FALSE(spice::mld::patching::planDreamcastTriangleSelectorPatches(
        file, std::span{ &edit, 1U }).ok());
}

TEST(MldPatchingCorpus, ResolvesRealDreamcastGrndAndGobjPatchOffsets) {
    const std::array roots{
        std::filesystem::path{ R"(D:\SoADC\SoA(Eu)Disc1Assets\FIELD)" },
        std::filesystem::path{ R"(D:\SoADC\SoA(Usa)Disc1Assets\FIELD)" },
    };
    if (!std::filesystem::exists(roots[0]) || !std::filesystem::exists(roots[1])) {
        GTEST_SKIP() << "Dreamcast field corpus roots are not available on this machine";
    }

    std::size_t filesChecked = 0U;
    std::size_t grndTriangles = 0U;
    std::size_t gobjTriangles = 0U;
    std::size_t a099aGobjPatches = 0U;
    std::size_t broaderFilesChecked = 0U;
    std::uint64_t provenanceHash = 1469598103934665603ULL;
    std::uint64_t patchHash = 1469598103934665603ULL;
    for (const auto& root : roots) {
        for (const auto* filename : { "A099A.MLD", "A106A.MLD", "A106C.MLD" }) {
            const auto path = root / filename;
            if (!std::filesystem::exists(path)) {
                continue;
            }
            const auto source = readFile(path);
            const auto file = spice::mld::parsing::MldParser{}.parseBytes(source);
            ASSERT_EQ(file.sourcePlatform, spice::mld::model::TargetPlatform::Dreamcast) << path.string();
            ASSERT_EQ(file.endian, Endian::Little) << path.string();
            ASSERT_FALSE(file.sourceWasCompressedAklz) << path.string();
            ++filesChecked;

            for (const auto& [address, resource] : file.groundResources) {
                if (resource.grnd.has_value()) {
                    const auto& grnd = *resource.grnd;
                    ASSERT_EQ(grnd.triangleSources.size(), grnd.mesh.triangleMetadata.size()) << path.string();
                    grndTriangles += grnd.triangleSources.size();
                    for (std::size_t triangle = 0; triangle < grnd.triangleSources.size(); ++triangle) {
                        hashWord(provenanceHash, 0U);
                        hashWord(provenanceHash, address);
                        hashWord(provenanceHash, triangle);
                        hashWord(provenanceHash, grnd.mesh.triangleMetadata[triangle].rawU16[2]);
                        hashWord(provenanceHash, grnd.triangleSources[triangle].flagSourceOffsets[2]);
                    }
                    if (grnd.triangleSources.empty()) {
                        continue;
                    }
                    const auto raw = grnd.mesh.triangleMetadata[0].rawU16[2];
                    const DreamcastTriangleSelectorEdit edit{
                        .resourceKind = TriangleResourceKind::Grnd,
                        .resourceAddress = address,
                        .triangleIndex = 0U,
                        .selectorDigit = differentValidSelector(raw),
                    };
                    const auto plan = spice::mld::patching::planDreamcastTriangleSelectorPatches(
                        file, std::span{ &edit, 1U });
                    ASSERT_TRUE(plan.ok()) << path.string();
                    ASSERT_EQ(plan.patches.size(), 1U) << path.string();
                    hashWord(patchHash, plan.patches[0].fileOffset);
                    hashWord(patchHash, plan.patches[0].expectedBytes[0]);
                    hashWord(patchHash, plan.patches[0].expectedBytes[1]);
                    hashWord(patchHash, plan.patches[0].replacementBytes[0]);
                    hashWord(patchHash, plan.patches[0].replacementBytes[1]);
                }
                if (resource.gobj.has_value()) {
                    const auto& gobj = *resource.gobj;
                    for (std::size_t nodeIndex = 0; nodeIndex < gobj.nodes.size(); ++nodeIndex) {
                        const auto& node = gobj.nodes[nodeIndex];
                        ASSERT_EQ(node.streamTriangleSources.size(), node.streamMesh.triangleMetadata.size()) << path.string();
                        gobjTriangles += node.streamTriangleSources.size();
                        for (std::size_t triangle = 0; triangle < node.streamTriangleSources.size(); ++triangle) {
                            hashWord(provenanceHash, 1U);
                            hashWord(provenanceHash, address);
                            hashWord(provenanceHash, nodeIndex);
                            hashWord(provenanceHash, triangle);
                            hashWord(provenanceHash, node.streamMesh.triangleMetadata[triangle].rawU16[2]);
                            hashWord(provenanceHash, node.streamTriangleSources[triangle].flagSourceOffsets[2]);
                        }
                        if (node.streamTriangleSources.empty()) {
                            continue;
                        }
                        const auto raw = node.streamMesh.triangleMetadata[0].rawU16[2];
                        const DreamcastTriangleSelectorEdit edit{
                            .resourceKind = TriangleResourceKind::Gobj,
                            .resourceAddress = address,
                            .gobjNodeIndex = nodeIndex,
                            .triangleIndex = 0U,
                            .selectorDigit = differentValidSelector(raw),
                        };
                        const auto plan = spice::mld::patching::planDreamcastTriangleSelectorPatches(
                            file, std::span{ &edit, 1U });
                        ASSERT_TRUE(plan.ok()) << path.string();
                        ASSERT_EQ(plan.patches.size(), 1U) << path.string();
                        hashWord(patchHash, plan.patches[0].fileOffset);
                        hashWord(patchHash, plan.patches[0].expectedBytes[0]);
                        hashWord(patchHash, plan.patches[0].expectedBytes[1]);
                        hashWord(patchHash, plan.patches[0].replacementBytes[0]);
                        hashWord(patchHash, plan.patches[0].replacementBytes[1]);
                        auto patched = source;
                        ASSERT_TRUE(spice::mld::patching::applyMldPatchPlan(patched, plan).ok()) << path.string();
                        EXPECT_EQ(patched.size(), source.size());
                        if (std::string(filename) == "A099A.MLD") {
                            ++a099aGobjPatches;
                        }
                    }
                }
            }
        }

        std::vector<std::filesystem::path> broader{};
        for (const auto& item : std::filesystem::directory_iterator(root)) {
            if (item.is_regular_file() && item.path().extension() == ".MLD") {
                broader.push_back(item.path());
            }
        }
        std::sort(broader.begin(), broader.end());
        std::size_t rootBroaderFilesChecked = 0U;
        for (const auto& path : broader) {
            if (rootBroaderFilesChecked >= 12U) {
                break;
            }
            const auto file = spice::mld::parsing::MldParser{}.parseBytes(readFile(path));
            if (file.sourcePlatform != spice::mld::model::TargetPlatform::Dreamcast) {
                continue;
            }
            for (const auto& [address, resource] : file.groundResources) {
                (void)address;
                if (resource.grnd.has_value()) {
                    EXPECT_EQ(resource.grnd->triangleSources.size(), resource.grnd->mesh.triangleMetadata.size()) << path.string();
                }
                if (resource.gobj.has_value()) {
                    for (const auto& node : resource.gobj->nodes) {
                        EXPECT_EQ(node.streamTriangleSources.size(), node.streamMesh.triangleMetadata.size()) << path.string();
                    }
                }
            }
            ++rootBroaderFilesChecked;
            ++broaderFilesChecked;
        }
    }
    EXPECT_EQ(filesChecked, 6U);
    EXPECT_GT(grndTriangles, 0U);
    EXPECT_GT(gobjTriangles, 0U);
    EXPECT_GT(a099aGobjPatches, 0U);
    EXPECT_EQ(broaderFilesChecked, 24U);
    std::cout << "Dreamcast GRND/GOBJ patch corpus files=" << filesChecked
              << " grndTriangles=" << grndTriangles
              << " gobjTriangles=" << gobjTriangles
              << " a099aGobjTargets=" << a099aGobjPatches
              << " broaderFiles=" << broaderFilesChecked
              << " provenanceHash=" << std::hex << provenanceHash
              << " patchHash=" << patchHash << std::dec << '\n';
}
