#include "../SpiceTrade/SpiceTrade.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {

using namespace spice::trade::alx;

std::filesystem::path referenceCorpusRoot()
{
    auto cursor = std::filesystem::current_path();
    for (std::size_t depth = 0U; depth < 8U; ++depth) {
        const auto candidate = cursor / "SpiceTrade" / "Alx v5.0.0 corpuses";
        if (std::filesystem::is_directory(candidate)) {
            return candidate;
        }
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) {
            break;
        }
        cursor = cursor.parent_path();
    }
    return {};
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path()
            / ("spice_trade_typed_tests_" + std::to_string(nonce));
        std::filesystem::create_directories(path);
    }

    ~TemporaryDirectory()
    {
        std::error_code error{};
        std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path{};
};

struct ReferenceDataset {
    const char* relativePath{};
    AlxLocale locale{};
    std::size_t enemyRows{};
    std::size_t encounterRows{};
    std::size_t eventRows{};
};

constexpr std::array kReferenceDatasets{
    ReferenceDataset{ "2000-08-28-dc-jp-final/disc-1", AlxLocale::Japanese, 157U, 1462U, 80U },
    ReferenceDataset{ "2000-08-28-dc-jp-final/disc-2", AlxLocale::Japanese, 157U, 1462U, 80U },
    ReferenceDataset{ "2000-09-18-dc-us-final/disc-1", AlxLocale::UnitedStates, 157U, 1462U, 80U },
    ReferenceDataset{ "2000-09-18-dc-us-final/disc-2", AlxLocale::UnitedStates, 157U, 1462U, 80U },
    ReferenceDataset{ "2001-02-19-dc-eu-final/disc-1", AlxLocale::Europe, 157U, 1462U, 80U },
    ReferenceDataset{ "2001-02-19-dc-eu-final/disc-2", AlxLocale::Europe, 157U, 1462U, 80U },
    ReferenceDataset{ "2002-11-12-gc-jp-final", AlxLocale::Japanese, 345U, 1462U, 250U },
    ReferenceDataset{ "2002-12-19-gc-us-final", AlxLocale::UnitedStates, 345U, 1462U, 250U },
    ReferenceDataset{ "2003-03-05-gc-eu-final", AlxLocale::Europe, 345U, 1462U, 250U },
};

template <typename ReadResult>
void expectReferenceFormat(const ReadResult& result, const std::filesystem::path& path)
{
    ASSERT_TRUE(result.ok()) << path.string();
    EXPECT_FALSE(result.format.utf8Bom) << path.string();
    EXPECT_EQ(result.format.lineEnding, CsvLineEnding::CrLf) << path.string();
    EXPECT_TRUE(result.format.finalLineEnding) << path.string();
}

TEST(SpiceTradeTypedSchemas, WhitelistAndCanonicalHeadersMatchAlx500)
{
    ASSERT_EQ(whitelistedTables().size(), 3U);
    EXPECT_EQ(whitelistedTableKind("ENEMY.CSV"), AlxTableKind::Enemy);
    EXPECT_EQ(whitelistedTableKind("nested/EnemyEncounter.csv"), AlxTableKind::EnemyEncounter);
    EXPECT_EQ(whitelistedTableKind("enemyevent.csv"), AlxTableKind::EnemyEvent);
    EXPECT_FALSE(whitelistedTableKind("enemytask.csv").has_value());

    EXPECT_EQ(canonicalHeaders(AlxTableKind::Enemy, AlxLocale::Japanese).size(), 85U);
    EXPECT_EQ(canonicalHeaders(AlxTableKind::Enemy, AlxLocale::UnitedStates).size(), 86U);
    EXPECT_EQ(canonicalHeaders(AlxTableKind::Enemy, AlxLocale::Europe).size(), 86U);
    EXPECT_EQ(canonicalHeaders(AlxTableKind::EnemyEncounter, AlxLocale::Japanese).size(), 20U);
    EXPECT_EQ(canonicalHeaders(AlxTableKind::EnemyEncounter, AlxLocale::UnitedStates).size(), 28U);
    EXPECT_EQ(canonicalHeaders(AlxTableKind::EnemyEncounter, AlxLocale::Europe).size(), 28U);
    EXPECT_EQ(canonicalHeaders(AlxTableKind::EnemyEvent, AlxLocale::Japanese).size(), 52U);
    EXPECT_EQ(canonicalHeaders(AlxTableKind::EnemyEvent, AlxLocale::UnitedStates).size(), 59U);
    EXPECT_EQ(canonicalHeaders(AlxTableKind::EnemyEvent, AlxLocale::Europe).size(), 59U);
}

TEST(SpiceTradeTypedCorpus, AllFinalReleaseTablesSemanticallyRoundTrip)
{
    const auto corpus = referenceCorpusRoot();
    if (corpus.empty()) {
        GTEST_SKIP() << "Private ALX 5.0.0 reference corpus is unavailable";
    }

    bool sawAbsentBgm = false;
    for (const auto& dataset : kReferenceDatasets) {
        const auto root = corpus / dataset.relativePath;

        const auto enemies = EnemyCsvCodec{}.readFile(root / "enemy.csv");
        expectReferenceFormat(enemies, root / "enemy.csv");
        ASSERT_EQ(enemies.locale, dataset.locale);
        ASSERT_EQ(enemies.table->records.size(), dataset.enemyRows);
        const auto enemyBytes = EnemyCsvCodec{}.write(*enemies.table, *enemies.locale, enemies.format);
        ASSERT_TRUE(enemyBytes.ok()) << root.string();
        const auto reparsedEnemies = EnemyCsvCodec{}.parse(enemyBytes.bytes);
        ASSERT_TRUE(reparsedEnemies.ok()) << root.string();
        EXPECT_EQ(reparsedEnemies.table, enemies.table) << root.string();
        EXPECT_EQ(reparsedEnemies.locale, enemies.locale) << root.string();

        const auto encounters = EnemyEncounterCsvCodec{}.readFile(root / "enemyencounter.csv");
        expectReferenceFormat(encounters, root / "enemyencounter.csv");
        ASSERT_EQ(encounters.locale, dataset.locale);
        ASSERT_EQ(encounters.table->records.size(), dataset.encounterRows);
        const auto encounterBytes = EnemyEncounterCsvCodec{}.write(
            *encounters.table, *encounters.locale, encounters.format);
        ASSERT_TRUE(encounterBytes.ok()) << root.string();
        const auto reparsedEncounters = EnemyEncounterCsvCodec{}.parse(encounterBytes.bytes);
        ASSERT_TRUE(reparsedEncounters.ok()) << root.string();
        EXPECT_EQ(reparsedEncounters.table, encounters.table) << root.string();
        EXPECT_EQ(reparsedEncounters.locale, encounters.locale) << root.string();

        const auto events = EnemyEventCsvCodec{}.readFile(root / "enemyevent.csv");
        expectReferenceFormat(events, root / "enemyevent.csv");
        ASSERT_EQ(events.locale, dataset.locale);
        ASSERT_EQ(events.table->records.size(), dataset.eventRows);
        sawAbsentBgm = sawAbsentBgm || std::any_of(
            events.table->records.begin(), events.table->records.end(), [](const auto& event) {
                return !event.bgmId.has_value();
            });
        const auto eventBytes = EnemyEventCsvCodec{}.write(*events.table, *events.locale, events.format);
        ASSERT_TRUE(eventBytes.ok()) << root.string();
        const auto reparsedEvents = EnemyEventCsvCodec{}.parse(eventBytes.bytes);
        ASSERT_TRUE(reparsedEvents.ok()) << root.string();
        EXPECT_EQ(reparsedEvents.table, events.table) << root.string();
        EXPECT_EQ(reparsedEvents.locale, events.locale) << root.string();
    }
    EXPECT_TRUE(sawAbsentBgm);
}

TEST(SpiceTradeTypedCodecs, EditsAndFullVectorOperationsRoundTrip)
{
    const auto corpus = referenceCorpusRoot();
    if (corpus.empty()) {
        GTEST_SKIP() << "Private ALX 5.0.0 reference corpus is unavailable";
    }
    const auto root = corpus / "2002-12-19-gc-us-final";

    auto enemies = EnemyCsvCodec{}.readFile(root / "enemy.csv");
    ASSERT_TRUE(enemies.ok());
    const auto enemyCount = enemies.table->records.size();
    enemies.table->records.front().maxHp += 123;
    enemies.table->records.front().name.localized = "Edited Enemy";
    enemies.table->records.front().filters = { "*", "epevent.evp" };
    enemies.table->records.front().movementFlags = 0x123;
    enemies.table->records.front().itemDrops[0].itemName = "Edited Item";
    enemies.table->records.erase(enemies.table->records.begin() + 1);
    enemies.table->records.push_back(enemies.table->records.front());
    std::swap(enemies.table->records.front(), enemies.table->records.back());
    ASSERT_EQ(enemies.table->records.size(), enemyCount);
    const auto enemyWrite = EnemyCsvCodec{}.write(*enemies.table, *enemies.locale, enemies.format);
    ASSERT_TRUE(enemyWrite.ok());
    const auto enemyReparse = EnemyCsvCodec{}.parse(enemyWrite.bytes);
    ASSERT_TRUE(enemyReparse.ok());
    EXPECT_EQ(enemyReparse.table, enemies.table);

    auto encounters = EnemyEncounterCsvCodec{}.readFile(root / "enemyencounter.csv");
    ASSERT_TRUE(encounters.ok());
    const auto encounterCount = encounters.table->records.size();
    encounters.table->records.front().initiative = 77U;
    encounters.table->records.front().enemies[7].enemyId = 42U;
    encounters.table->records.front().enemies[7].name.localized = "Edited Reference";
    encounters.table->records.erase(encounters.table->records.begin() + 1);
    encounters.table->records.push_back(encounters.table->records.front());
    std::swap(encounters.table->records.front(), encounters.table->records.back());
    ASSERT_EQ(encounters.table->records.size(), encounterCount);
    const auto encounterWrite = EnemyEncounterCsvCodec{}.write(
        *encounters.table, *encounters.locale, encounters.format);
    ASSERT_TRUE(encounterWrite.ok());
    const auto encounterReparse = EnemyEncounterCsvCodec{}.parse(encounterWrite.bytes);
    ASSERT_TRUE(encounterReparse.ok());
    EXPECT_EQ(encounterReparse.table, encounters.table);

    auto events = EnemyEventCsvCodec{}.readFile(root / "enemyevent.csv");
    ASSERT_TRUE(events.ok());
    const auto eventCount = events.table->records.size();
    events.table->records.front().magicExperience = 19U;
    events.table->records.front().players[3].characterName = "Edited Player";
    events.table->records.front().enemies[6].enemy.name.localized = "Edited Event Enemy";
    events.table->records.front().bgmId = std::nullopt;
    events.table->records.erase(events.table->records.begin() + 1);
    events.table->records.push_back(events.table->records.front());
    std::swap(events.table->records.front(), events.table->records.back());
    ASSERT_EQ(events.table->records.size(), eventCount);
    const auto eventWrite = EnemyEventCsvCodec{}.write(*events.table, *events.locale, events.format);
    ASSERT_TRUE(eventWrite.ok());
    const auto eventReparse = EnemyEventCsvCodec{}.parse(eventWrite.bytes);
    ASSERT_TRUE(eventReparse.ok());
    EXPECT_EQ(eventReparse.table, events.table);
}

TEST(SpiceTradeTypedCodecs, RejectsNoncanonicalHeadersAndUnrepresentableModels)
{
    const auto corpus = referenceCorpusRoot();
    if (corpus.empty()) {
        GTEST_SKIP() << "Private ALX 5.0.0 reference corpus is unavailable";
    }
    const auto root = corpus / "2002-12-19-gc-us-final";
    const auto parsed = CsvReader{}.readFile(root / "enemy.csv");
    ASSERT_TRUE(parsed.ok());

    auto reordered = *parsed.document;
    std::swap(reordered.headers[0], reordered.headers[1]);
    const auto reorderedBytes = CsvWriter{}.write(reordered, parsed.format);
    ASSERT_TRUE(reorderedBytes.ok());
    EXPECT_FALSE(EnemyCsvCodec{}.parse(reorderedBytes.bytes).ok());

    auto overflow = *parsed.document;
    const auto widthColumn = overflow.columnIndex("Width");
    ASSERT_TRUE(widthColumn.has_value());
    overflow.rows.front()[*widthColumn] = "128";
    const auto overflowBytes = CsvWriter{}.write(overflow, parsed.format);
    ASSERT_TRUE(overflowBytes.ok());
    EXPECT_FALSE(EnemyCsvCodec{}.parse(overflowBytes.bytes).ok());

    auto enemies = EnemyCsvCodec{}.readFile(root / "enemy.csv");
    ASSERT_TRUE(enemies.ok());
    enemies.table->records.front().unknown1 = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(EnemyCsvCodec{}.write(*enemies.table, *enemies.locale, enemies.format).ok());

    enemies = EnemyCsvCodec{}.readFile(root / "enemy.csv");
    ASSERT_TRUE(enemies.ok());
    EXPECT_FALSE(EnemyCsvCodec{}.write(*enemies.table, AlxLocale::Japanese, enemies.format).ok());

    enemies.table->records.front().filters = { "invalid;filter" };
    EXPECT_FALSE(EnemyCsvCodec{}.write(*enemies.table, *enemies.locale, enemies.format).ok());
}

TEST(SpiceTradeTypedWorkspace, LoadsIndependentSubsetsAndWritesOnlyChangedTables)
{
    const auto corpus = referenceCorpusRoot();
    if (corpus.empty()) {
        GTEST_SKIP() << "Private ALX 5.0.0 reference corpus is unavailable";
    }
    const auto source = corpus / "2002-12-19-gc-us-final";
    const std::array requested{ AlxTableKind::EnemyEncounter };
    auto read = AlxWorkspaceReader{}.read(source, requested);
    ASSERT_TRUE(read.ok());
    ASSERT_TRUE(read.workspace->enemyEncounters.has_value());
    EXPECT_FALSE(read.workspace->enemies.has_value());
    EXPECT_FALSE(read.workspace->enemyEvents.has_value());
    EXPECT_TRUE(read.workspace->changedTables().empty());

    read.workspace->enemyEncounters->current.records.front().initiative = 99U;
    EXPECT_EQ(
        read.workspace->changedTables(),
        std::vector<AlxTableKind>{ AlxTableKind::EnemyEncounter });

    TemporaryDirectory temp{};
    const auto write = AlxWorkspaceWriter{}.writeChanged(*read.workspace, temp.path / "output");
    ASSERT_TRUE(write.ok());
    EXPECT_EQ(write.writtenTables,
              std::vector<AlxTableKind>{ AlxTableKind::EnemyEncounter });
    EXPECT_TRUE(std::filesystem::exists(temp.path / "output" / "enemyencounter.csv"));
    EXPECT_FALSE(std::filesystem::exists(temp.path / "output" / "enemy.csv"));
    EXPECT_FALSE(std::filesystem::exists(temp.path / "output" / "enemyevent.csv"));
    EXPECT_TRUE(read.workspace->changedTables().empty());

    const auto output = EnemyEncounterCsvCodec{}.readFile(
        temp.path / "output" / "enemyencounter.csv");
    ASSERT_TRUE(output.ok());
    EXPECT_EQ(output.table->records.front().initiative, 99U);
}

TEST(SpiceTradeTypedWorkspace, RequestedImportIsAllOrNothing)
{
    const auto corpus = referenceCorpusRoot();
    if (corpus.empty()) {
        GTEST_SKIP() << "Private ALX 5.0.0 reference corpus is unavailable";
    }
    TemporaryDirectory temp{};
    const auto corpusFile = corpus
        / "2002-12-19-gc-us-final" / "enemy.csv";
    std::filesystem::copy_file(corpusFile, temp.path / "enemy.csv");

    const std::array requested{
        AlxTableKind::Enemy,
        AlxTableKind::EnemyEncounter,
    };
    const auto missing = AlxWorkspaceReader{}.read(temp.path, requested);
    EXPECT_FALSE(missing.ok());
    EXPECT_FALSE(missing.workspace.has_value());

    const std::array duplicate{
        AlxTableKind::Enemy,
        AlxTableKind::Enemy,
    };
    EXPECT_FALSE(AlxWorkspaceReader{}.read(temp.path, duplicate).ok());
    EXPECT_FALSE(AlxWorkspaceReader{}.read(temp.path, {}).ok());
}

} // namespace
