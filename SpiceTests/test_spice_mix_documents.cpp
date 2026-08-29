#include "../SpiceMix/Documents/GvrDocumentSession.h"
#include "../SpiceMix/Documents/MldDocumentSession.h"
#include "../SpiceGvm/Encoding/GvrEncoder.h"
#include "../SpiceGvm/Image/PngCodec.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace {

void writeU32Be(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value >> 24U);
    bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 16U);
    bytes[offset + 2U] = static_cast<std::uint8_t>(value >> 8U);
    bytes[offset + 3U] = static_cast<std::uint8_t>(value);
}

void writeFile(const std::filesystem::path& path, const std::span<const std::uint8_t> bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::string readText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
}

std::filesystem::path makeTempDirectory() {
    const auto name = "spice_mix_documents_" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::create_directories(path);
    return path;
}

spice::gvm::model::RgbaImage image(const std::uint32_t width, const std::uint32_t height,
    const std::uint8_t red, const std::uint8_t green) {
    spice::gvm::model::RgbaImage out{};
    out.width = width;
    out.height = height;
    out.rgba8.resize(static_cast<std::size_t>(width) * height * 4U);
    for (std::size_t offset = 0; offset < out.rgba8.size(); offset += 4U) {
        out.rgba8[offset] = red;
        out.rgba8[offset + 1U] = green;
        out.rgba8[offset + 2U] = 40U;
        out.rgba8[offset + 3U] = 255U;
    }
    return out;
}

std::vector<std::uint8_t> makeTwoTextureMld() {
    const auto first = spice::gvm::encoding::encodeGvr(image(4, 4, 255, 0));
    const auto second = spice::gvm::encoding::encodeGvr(image(4, 4, 0, 255));
    constexpr std::size_t textureTable = 0x340U;
    constexpr std::size_t recordSize = 44U;
    const std::size_t archiveHeaderSize = 4U + 2U * recordSize;
    std::vector<std::uint8_t> bytes(textureTable + archiveHeaderSize, 0U);
    writeU32Be(bytes, 0x00U, 1U);
    writeU32Be(bytes, 0x04U, 0x20U);
    writeU32Be(bytes, 0x08U, 0x108U);
    writeU32Be(bytes, 0x0CU, 0x180U);
    writeU32Be(bytes, 0x10U, static_cast<std::uint32_t>(textureTable));
    writeU32Be(bytes, 0x20U, 7U);
    writeU32Be(bytes, 0x24U, 9U);
    writeU32Be(bytes, 0x28U, 0x100U);
    writeU32Be(bytes, 0x2CU, 0x108U);
    writeU32Be(bytes, 0x30U, 0x108U);
    writeU32Be(bytes, 0x34U, 0x118U);
    writeU32Be(bytes, 0x38U, 0x120U);
    writeU32Be(bytes, 0x3CU, 0x128U);
    writeU32Be(bytes, 0x40U, static_cast<std::uint32_t>(textureTable));
    const std::string functionName = "textures";
    std::copy(functionName.begin(), functionName.end(), bytes.begin() + 0x44U);
    writeU32Be(bytes, 0x100U, 0U);
    writeU32Be(bytes, 0x108U, 2U);
    writeU32Be(bytes, 0x10CU, 11U);
    writeU32Be(bytes, 0x110U, 22U);
    writeU32Be(bytes, 0x118U, 0U);
    writeU32Be(bytes, 0x120U, 0U);
    writeU32Be(bytes, 0x128U, 0U);
    writeU32Be(bytes, textureTable, 2U);
    const std::string names[] = { "first", "second" };
    for (std::size_t index = 0; index < 2U; ++index) {
        const auto record = textureTable + 4U + index * recordSize;
        std::copy(names[index].begin(), names[index].end(), bytes.begin() + static_cast<std::ptrdiff_t>(record));
    }
    bytes.insert(bytes.end(), first.begin(), first.end());
    bytes.insert(bytes.end(), second.begin(), second.end());
    return bytes;
}

class TempDirectory final {
public:
    TempDirectory() : path(makeTempDirectory()) {}
    ~TempDirectory() { std::error_code error{}; std::filesystem::remove_all(path, error); }
    std::filesystem::path path;
};

} // namespace

TEST(SpiceMixDocuments, NewGvrUsesSafeDefaultsAndRoundTripsThroughSaveAs) {
    TempDirectory temp{};
    const auto png = temp.path / "source.png";
    spice::gvm::image::writePngRgba8(png, image(8, 4, 120, 30));

    auto created = spice::mix::GvrDocumentSession::createFromPng(png);
    ASSERT_TRUE(created.result.ok()) << created.result.message;
    ASSERT_TRUE(created.session);
    const auto snapshot = created.session->snapshot();
    EXPECT_EQ(snapshot.format, "RGBA8");
    EXPECT_FALSE(snapshot.mipmaps);
    EXPECT_FALSE(snapshot.hasGlobalIndex);
    EXPECT_FALSE(snapshot.aklzWrapped);
    EXPECT_TRUE(snapshot.dirty);
    ASSERT_TRUE(created.session->preview().has_value());
    EXPECT_EQ(created.session->preview()->width, 8U);

    const auto output = temp.path / "created.gvr";
    EXPECT_TRUE(created.session->saveAs(output).ok());
    EXPECT_FALSE(created.session->dirty());
    ASSERT_TRUE(std::filesystem::is_regular_file(output));

    auto opened = spice::mix::GvrDocumentSession::open(output);
    ASSERT_TRUE(opened.result.ok()) << opened.result.message;
    EXPECT_EQ(opened.session->snapshot().width, 8U);
    EXPECT_FALSE(opened.session->snapshot().aklzWrapped);
}

TEST(SpiceMixDocuments, GvrReplacementPreservesPropertiesAndCanRevert) {
    TempDirectory temp{};
    const auto sourcePng = temp.path / "source.png";
    const auto replacementPng = temp.path / "replacement.png";
    spice::gvm::image::writePngRgba8(sourcePng, image(4, 4, 10, 20));
    spice::gvm::image::writePngRgba8(replacementPng, image(4, 4, 30, 40));
    auto created = spice::mix::GvrDocumentSession::createFromPng(sourcePng);
    ASSERT_TRUE(created.result.ok());
    const auto output = temp.path / "source.gvr";
    ASSERT_TRUE(created.session->saveAs(output).ok());

    auto opened = spice::mix::GvrDocumentSession::open(output);
    ASSERT_TRUE(opened.result.ok());
    const auto before = opened.session->snapshot();
    EXPECT_TRUE(opened.session->replaceImage(replacementPng).ok());
    EXPECT_TRUE(opened.session->dirty());
    const auto after = opened.session->snapshot();
    EXPECT_EQ(after.format, before.format);
    EXPECT_EQ(after.mipmaps, before.mipmaps);
    EXPECT_EQ(after.hasGlobalIndex, before.hasGlobalIndex);
    EXPECT_TRUE(opened.session->revert().ok());
    EXPECT_FALSE(opened.session->dirty());
}

TEST(SpiceMixDocuments, MldStagesMultipleGvrReplacementsAndProtectsOriginal) {
    TempDirectory temp{};
    const auto source = temp.path / "two_textures.mld";
    writeFile(source, makeTwoTextureMld());
    const auto firstPng = temp.path / "first.png";
    const auto secondPng = temp.path / "second.png";
    spice::gvm::image::writePngRgba8(firstPng, image(4, 4, 10, 80));
    spice::gvm::image::writePngRgba8(secondPng, image(4, 4, 70, 20));

    auto opened = spice::mix::MldDocumentSession::open(source);
    ASSERT_TRUE(opened.result.ok()) << opened.result.message;
    ASSERT_TRUE(opened.session);
    ASSERT_EQ(opened.session->textures().size(), 2U);
    ASSERT_TRUE(opened.session->replaceGvrTexture(0U, firstPng).ok());
    ASSERT_TRUE(opened.session->replaceGvrTexture(1U, secondPng).ok());
    EXPECT_TRUE(opened.session->textures()[0].dirty);
    EXPECT_TRUE(opened.session->textures()[1].dirty);
    ASSERT_TRUE(opened.session->revertTexture(0U).ok());
    EXPECT_FALSE(opened.session->textures()[0].dirty);
    EXPECT_TRUE(opened.session->textures()[1].dirty);

    const auto protectedResult = opened.session->saveAs(source);
    EXPECT_EQ(protectedResult.status, spice::mix::OperationStatus::Failure);
    const auto output = temp.path / "saved.mld";
    ASSERT_TRUE(opened.session->saveAs(output).ok());
    EXPECT_FALSE(opened.session->dirty());
    auto reparsed = spice::mix::MldDocumentSession::open(output);
    ASSERT_TRUE(reparsed.result.ok()) << reparsed.result.message;
    EXPECT_EQ(reparsed.session->textures().size(), 2U);
}

TEST(SpiceMixDocuments, MldExportsCurrentDocumentWithoutChangingDirtyState) {
    TempDirectory temp{};
    const auto source = temp.path / "a101b_DC.mld";
    writeFile(source, makeTwoTextureMld());
    auto opened = spice::mix::MldDocumentSession::open(source);
    ASSERT_TRUE(opened.result.ok()) << opened.result.message;
    ASSERT_TRUE(opened.session);

    std::vector<spice::mix::OperationEvent> events{};
    const spice::mix::DocumentContext context{
        .report = [&events](const auto& event) { events.push_back(event); },
    };
    const auto cleanBlenderPath = temp.path / "clean.json";
    const auto entryPath = temp.path / "a101b_DC.mld.entries.json";
    ASSERT_TRUE(opened.session->exportBlenderIrJson(cleanBlenderPath, context).ok());
    ASSERT_TRUE(opened.session->exportEntryListJson(entryPath, context).ok());
    EXPECT_FALSE(opened.session->dirty());
    ASSERT_TRUE(std::filesystem::is_regular_file(cleanBlenderPath));
    ASSERT_TRUE(std::filesystem::is_regular_file(entryPath));

    const auto entryJson = readText(entryPath);
    EXPECT_NE(entryJson.find("\"schema\": \"spice_mld_entry_list_v1\""), std::string::npos);
    EXPECT_NE(entryJson.find("\"source\": \""), std::string::npos);
    EXPECT_NE(entryJson.find("\"entry_count\": 1"), std::string::npos);
    EXPECT_NE(entryJson.find("\"entryID\": 7"), std::string::npos);
    EXPECT_NE(entryJson.find("\"tableID\": 9"), std::string::npos);
    EXPECT_NE(entryJson.find("\"texture_names\": ["), std::string::npos);

    const auto replacementPng = temp.path / "replacement.png";
    spice::gvm::image::writePngRgba8(replacementPng, image(8, 8, 45, 90));
    ASSERT_TRUE(opened.session->replaceGvrTexture(0U, replacementPng, {}, true).ok());
    ASSERT_TRUE(opened.session->dirty());
    const auto stagedBlenderPath = temp.path / "staged.json";
    ASSERT_TRUE(opened.session->exportBlenderIrJson(stagedBlenderPath, context).ok());
    EXPECT_TRUE(opened.session->dirty());
    const auto stagedJson = readText(stagedBlenderPath);
    EXPECT_NE(stagedJson.find("\"textureName\":\"first\""), std::string::npos);
    EXPECT_NE(stagedJson.find("\"width\":8"), std::string::npos);
    EXPECT_NE(stagedJson.find("\"height\":8"), std::string::npos);
    EXPECT_TRUE(std::any_of(events.begin(), events.end(), [](const auto& event) {
        return event.level == spice::mix::EventLevel::Progress;
    }));
    EXPECT_TRUE(std::any_of(events.begin(), events.end(), [](const auto& event) {
        return event.level == spice::mix::EventLevel::Info;
    }));
}

TEST(SpiceMixDocuments, MldExportsHonorCancellationAndRemoveFailedTemporaryFiles) {
    TempDirectory temp{};
    const auto source = temp.path / "source.mld";
    writeFile(source, makeTwoTextureMld());
    auto opened = spice::mix::MldDocumentSession::open(source);
    ASSERT_TRUE(opened.result.ok()) << opened.result.message;

    std::stop_source stop{};
    stop.request_stop();
    const auto cancelledPath = temp.path / "cancelled.json";
    const auto cancelled = opened.session->exportBlenderIrJson(cancelledPath,
        spice::mix::DocumentContext{ .stopToken = stop.get_token() });
    EXPECT_EQ(cancelled.status, spice::mix::OperationStatus::Cancelled);
    EXPECT_FALSE(std::filesystem::exists(cancelledPath));

    const auto invalidTarget = temp.path / "existing_directory";
    std::filesystem::create_directories(invalidTarget);
    const auto failed = opened.session->exportEntryListJson(invalidTarget);
    EXPECT_EQ(failed.status, spice::mix::OperationStatus::Failure);
    for (const auto& entry : std::filesystem::directory_iterator(temp.path)) {
        EXPECT_EQ(entry.path().filename().string().find(".spicemix-"), std::string::npos);
    }
}

TEST(SpiceMixDocuments, CancelledOpenDoesNotCreateSession) {
    std::stop_source stop{};
    stop.request_stop();
    const auto opened = spice::mix::GvrDocumentSession::open("missing.gvr",
        spice::mix::DocumentContext{ .stopToken = stop.get_token() });
    EXPECT_FALSE(opened.session);
    EXPECT_EQ(opened.result.status, spice::mix::OperationStatus::Cancelled);
}

TEST(SpiceMixDocuments, DocumentOperationsDeliverStructuredEvents) {
    TempDirectory temp{};
    const auto png = temp.path / "source.png";
    spice::gvm::image::writePngRgba8(png, image(4, 4, 20, 30));
    std::vector<spice::mix::OperationEvent> events{};
    const auto created = spice::mix::GvrDocumentSession::createFromPng(png,
        spice::mix::DocumentContext{ .report = [&events](const auto& event) { events.push_back(event); } });
    ASSERT_TRUE(created.result.ok());
    EXPECT_TRUE(std::any_of(events.begin(), events.end(), [](const auto& event) {
        return event.level == spice::mix::EventLevel::Progress;
    }));
    EXPECT_TRUE(std::any_of(events.begin(), events.end(), [](const auto& event) {
        return event.level == spice::mix::EventLevel::Info;
    }));
}
