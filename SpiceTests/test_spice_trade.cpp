#include "../SpiceTrade/SpiceTrade.h"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <set>

namespace {
using namespace spice::trade::alx;

std::filesystem::path corpusRoot()
{
    auto cursor = std::filesystem::current_path();
    for (std::size_t depth = 0; depth < 8; ++depth) {
        const auto candidate = cursor / "SpiceTrade" / "Alx v5.0.0 corpuses";
        if (std::filesystem::is_directory(candidate)) return candidate;
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) break;
        cursor = cursor.parent_path();
    }
    return {};
}

struct DatasetCase { const char* path; AlxLocale locale; std::size_t enemies; std::size_t tasks; std::size_t events; };
constexpr std::array kDatasets{
    DatasetCase{ "2000-08-28-dc-jp-final/disc-1", AlxLocale::Japanese, 157, 2308, 80 },
    DatasetCase{ "2000-08-28-dc-jp-final/disc-2", AlxLocale::Japanese, 157, 2308, 80 },
    DatasetCase{ "2000-09-18-dc-us-final/disc-1", AlxLocale::UnitedStates, 157, 2308, 80 },
    DatasetCase{ "2000-09-18-dc-us-final/disc-2", AlxLocale::UnitedStates, 157, 2308, 80 },
    DatasetCase{ "2001-02-19-dc-eu-final/disc-1", AlxLocale::Europe, 157, 2308, 80 },
    DatasetCase{ "2001-02-19-dc-eu-final/disc-2", AlxLocale::Europe, 157, 2308, 80 },
    DatasetCase{ "2002-11-12-gc-jp-final", AlxLocale::Japanese, 245, 5123, 250 },
    DatasetCase{ "2002-12-19-gc-us-final", AlxLocale::UnitedStates, 245, 5123, 250 },
    DatasetCase{ "2003-03-05-gc-eu-final", AlxLocale::Europe, 245, 5123, 250 },
};

std::size_t taskCount(const EnemyTaskTable& table) { std::size_t n=0; for (const auto& g:table.groups()) n+=g.records().size(); return n; }
std::size_t encounterCount(const EnemyEncounterTable& table) { std::size_t n=0; for (const auto& g:table.groups()) n+=g.records().size(); return n; }

TEST(SpiceTradeHardCut, WhitelistAndStableIdentitiesAreCanonical)
{
    ASSERT_EQ(whitelistedTables().size(), 15U);
    EXPECT_TRUE(whitelistedTableKind("ENEMYTASK.CSV").has_value());
    EXPECT_EQ(canonicalIdentity(EnemyEntryId{42}), "enemy.42");
    EXPECT_EQ(canonicalIdentity(EnemyTaskEntryId{EnemyEntryId{42},3}), "enemytask.42.3");
    EXPECT_EQ(canonicalIdentity(EnemyEncounterEntryId{"MA000.ENP",7}), "enemyencounter.ma000.enp.7");
}

TEST(SpiceTradeHardCut, ImportsAllFifteenTablesAtomicallyAcrossAllFinalDatasets)
{
    const auto corpus=corpusRoot(); if(corpus.empty()) GTEST_SKIP() << "Private ALX 5.0.0 reference corpus is unavailable";
    for(const auto& expected:kDatasets){
        const auto path=corpus/expected.path; auto imported=AlxDatasetImporter{}.importWhitelistedDirectory(path);
        ASSERT_TRUE(imported.ok()) << path.string() << (imported.diagnostics.empty()?"":imported.diagnostics.front().message);
        ASSERT_EQ(imported.locale,expected.locale); ASSERT_EQ(imported.metadata.size(),15U);
        const auto& d=*imported.dataset;
        ASSERT_TRUE(d.enemies&&d.enemyEncounters&&d.enemyEvents&&d.enemyTasks&&d.enemyMagic&&d.accessories&&d.armor&&d.usableItems&&d.weapons&&d.weaponEffects&&d.experienceCurves&&d.characters&&d.characterMagic&&d.characterSuperMoves&&d.magicExperienceCurves);
        EXPECT_EQ(d.enemies->records().size(),expected.enemies)<<path.string(); EXPECT_EQ(taskCount(*d.enemyTasks),expected.tasks)<<path.string();
        EXPECT_EQ(d.enemyEvents->records().size(),expected.events); EXPECT_EQ(d.enemyEncounters->groups().size(),50U); EXPECT_EQ(encounterCount(*d.enemyEncounters),1412U);
        EXPECT_EQ(d.enemyMagic->records().size(),36U); EXPECT_EQ(d.accessories->records().size(),80U); EXPECT_EQ(d.armor->records().size(),80U);
        EXPECT_EQ(d.usableItems->records().size(),80U); EXPECT_EQ(d.weapons->records().size(),80U); EXPECT_EQ(d.weaponEffects->records().size(),21U);
        EXPECT_EQ(d.experienceCurves->records().size(),6U); EXPECT_EQ(d.characters->records().size(),6U); EXPECT_EQ(d.characterMagic->records().size(),36U);
        EXPECT_EQ(d.characterSuperMoves->records().size(),26U); EXPECT_EQ(d.magicExperienceCurves->records().size(),6U);
    }
}

TEST(SpiceTradeHardCut, FieldsAreMutableWhileIdentityAndOrderStayStable)
{
    const auto corpus=corpusRoot(); if(corpus.empty()) GTEST_SKIP();
    auto imported=AlxDatasetImporter{}.importWhitelistedDirectory(corpus/"2002-12-19-gc-us-final"); ASSERT_TRUE(imported.ok());
    auto& enemies=*imported.dataset->enemies; const auto id=enemies.records().front().id(); const auto count=enemies.records().size();
    ASSERT_NE(enemies.edit(id),nullptr); enemies.edit(id)->maxHp+=123; EXPECT_EQ(enemies.records().front().id(),id); EXPECT_EQ(enemies.records().size(),count);
    auto& tasks=*imported.dataset->enemyTasks; const auto task=tasks.groups().front().records().front().id();
    ASSERT_NE(tasks.edit(task),nullptr); tasks.edit(task)->parameterId=77; EXPECT_EQ(tasks.groups().front().records().front().id(),task);
}

TEST(SpiceTradeHardCut, LocaleNeutralTablesRequireContextAndDatasetCanInferIt)
{
    const auto corpus=corpusRoot(); if(corpus.empty()) GTEST_SKIP(); const auto root=corpus/"2002-12-19-gc-us-final";
    EXPECT_FALSE(ExpCurveCsvImporter{}.importFile(root/"expcurve.csv").ok());
    EXPECT_TRUE(ExpCurveCsvImporter{}.importFile(root/"expcurve.csv",{.localeHint=AlxLocale::UnitedStates}).ok());
    EXPECT_FALSE(EnemyCsvImporter{}.importFile(root/"enemy.csv",{.localeHint=AlxLocale::Europe}).ok());
    const std::array requested{AlxTableKind::Enemy,AlxTableKind::ExpCurve,AlxTableKind::MagicExpCurve};
    const auto dataset=AlxDatasetImporter{}.importDirectory(root,requested); EXPECT_TRUE(dataset.ok()); EXPECT_EQ(dataset.locale,AlxLocale::UnitedStates);
}

TEST(SpiceTradeHardCut, EuropeanImportedMessageTextIsSemanticAndEditable)
{
    const auto corpus=corpusRoot(); if(corpus.empty()) GTEST_SKIP();
    auto imported=AccessoryCsvImporter{}.importFile(corpus/"2003-03-05-gc-eu-final"/"accessory.csv"); ASSERT_TRUE(imported.ok());
    const auto id=imported.table->records().front().id(); auto* fields=imported.table->edit(id); ASSERT_NE(fields,nullptr); ASSERT_TRUE(fields->name.messageId);
    fields->name.text="Edited GB name"; fields->description="Edited GB description"; EXPECT_EQ(imported.table->find(id)->name.text,"Edited GB name");
}

TEST(SpiceTradeHardCut, DerivedViewsRebuildKnownReferencesFromCurrentIds)
{
    const auto corpus=corpusRoot(); if(corpus.empty()) GTEST_SKIP();
    const std::array requested{AlxTableKind::Enemy,AlxTableKind::EnemyEvent,AlxTableKind::Character};
    auto imported=AlxDatasetImporter{}.importDirectory(corpus/"2002-12-19-gc-us-final",requested); ASSERT_TRUE(imported.ok());
    const auto id=imported.dataset->enemyEvents->records().front().id(); auto* event=imported.dataset->enemyEvents->edit(id); ASSERT_NE(event,nullptr);
    event->enemies[0].enemy=EnemyEntryId{0}; auto* enemy=imported.dataset->enemies->edit(EnemyEntryId{0}); ASSERT_NE(enemy,nullptr);
    enemy->japaneseName="Edited current enemy";
    const auto view=AlxDerivedViewBuilder{}.build(*imported.dataset,imported.derivedContext,AlxTableKind::EnemyEvent);
    ASSERT_FALSE(view.rows.empty()); EXPECT_EQ(view.rows.front().cells.at("[EC1 JP Name]"),"Edited current enemy");
    EXPECT_FALSE(view.diagnostics.empty()); EXPECT_EQ(view.diagnostics.front().severity,AlxDiagnosticSeverity::Warning);
}

TEST(SpiceTradeHardCut, MissingRequestedTablePublishesNothing)
{
    const auto corpus=corpusRoot(); if(corpus.empty()) GTEST_SKIP(); const std::array requested{AlxTableKind::Enemy,AlxTableKind::EnemyTask};
    const auto result=AlxDatasetImporter{}.importDirectory(corpus/"does-not-exist",requested,{.localeHint=AlxLocale::UnitedStates});
    EXPECT_FALSE(result.ok()); EXPECT_FALSE(result.dataset); EXPECT_TRUE(result.metadata.empty());
}

} // namespace
