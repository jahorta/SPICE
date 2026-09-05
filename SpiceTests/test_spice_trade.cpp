#include "../SpiceTrade/SpiceTrade.h"
#include "CorpusTestSupport.h"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <set>
#include <string_view>

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

std::array<std::uint8_t, 32U> digestFromHex(const std::string_view hex)
{
    const auto nibble=[](const char value)->std::uint8_t {
        if(value>='0'&&value<='9') return static_cast<std::uint8_t>(value-'0');
        if(value>='a'&&value<='f') return static_cast<std::uint8_t>(value-'a'+10);
        return static_cast<std::uint8_t>(value-'A'+10);
    };
    std::array<std::uint8_t,32U> result{};
    for(std::size_t i=0;i<result.size();++i) result[i]=static_cast<std::uint8_t>((nibble(hex[i*2U])<<4U)|nibble(hex[i*2U+1U]));
    return result;
}

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
    if (!spice::tests::corpusTestsEnabled(spice::tests::CorpusFileType::Alx)) {
        GTEST_SKIP() << spice::tests::corpusTestsOptInMessage(spice::tests::CorpusFileType::Alx);
    }
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

TEST(SpiceTradeHardCut, ImportsExactUsGameCubeFirstBattleRowsAndSourceIdentities)
{
    if (!spice::tests::corpusTestsEnabled(spice::tests::CorpusFileType::Alx)) {
        GTEST_SKIP() << spice::tests::corpusTestsOptInMessage(spice::tests::CorpusFileType::Alx);
    }
    const auto corpus=corpusRoot(); if(corpus.empty()) GTEST_SKIP() << "Private ALX 5.0.0 reference corpus is unavailable";
    const auto root=corpus/"2002-12-19-gc-us-final";
    const std::array requested{
        AlxTableKind::EnemyEvent, AlxTableKind::Enemy, AlxTableKind::EnemyTask,
        AlxTableKind::Character, AlxTableKind::Weapon, AlxTableKind::Armor,
        AlxTableKind::Accessory,
    };
    const auto imported=AlxDatasetImporter{}.importDirectory(root,requested);
    ASSERT_TRUE(imported.ok()) << (imported.diagnostics.empty()?"":imported.diagnostics.front().message);
    ASSERT_TRUE(imported.dataset.has_value());
    ASSERT_EQ(imported.metadata.size(),requested.size());

    struct SourceIdentity { AlxTableKind kind; std::string_view file; std::string_view sha256; };
    constexpr std::array identities{
        SourceIdentity{AlxTableKind::EnemyEvent,"enemyevent.csv","26fce2ddcc1f72d62ff05b5b45284203eb26f5f58eccf3558e4bf88b2c441410"},
        SourceIdentity{AlxTableKind::Enemy,"enemy.csv","f07bb43354f63fa335400c5ab992278be101e8cfc3fde8d1c3a7d1d92c1af446"},
        SourceIdentity{AlxTableKind::EnemyTask,"enemytask.csv","dbb4bcc518622d4faa4ba9863f7e914cc7ce3ac28b2489a7f3d32ae6f888f912"},
        SourceIdentity{AlxTableKind::Character,"character.csv","821346571f7c1fb1ee5b898ca67753d518f2f576f6164e18fac0cd7121bdb136"},
        SourceIdentity{AlxTableKind::Weapon,"weapon.csv","3c0f6ba35a2adb1b2912841c77722da5b80b8ebeeb6ace3e1ab0d7ec6a76830b"},
        SourceIdentity{AlxTableKind::Armor,"armor.csv","1d7a3e3d7f6021f50e6315be939ae6e73d3f6ea637f213928f120a24140cca92"},
        SourceIdentity{AlxTableKind::Accessory,"accessory.csv","68d963c2f7d80170127efc30dda8aec55513e4bd4d81c2bd643b4e4363ef2109"},
    };
    for(const auto& expected:identities){
        const auto metadata=std::find_if(imported.metadata.begin(),imported.metadata.end(),[&](const auto& item){return item.table==expected.kind;});
        ASSERT_NE(metadata,imported.metadata.end());
        EXPECT_EQ(metadata->path,root/expected.file);
        EXPECT_EQ(metadata->rawSourceSize,std::filesystem::file_size(metadata->path));
        EXPECT_EQ(metadata->rawSourceSha256,digestFromHex(expected.sha256));
    }

    const auto& dataset=*imported.dataset;
    ASSERT_TRUE(dataset.enemyEvents&&dataset.enemies&&dataset.enemyTasks&&dataset.characters&&dataset.weapons&&dataset.armor&&dataset.accessories);
    const auto* event=dataset.enemyEvents->find(EnemyEventEntryId{0U}); ASSERT_NE(event,nullptr);
    EXPECT_EQ(event->initiative,14U);
    ASSERT_EQ(event->players[0].character,std::optional{CharacterEntryId{0U}}); EXPECT_EQ(event->players[0].x,4); EXPECT_EQ(event->players[0].z,6);
    ASSERT_EQ(event->players[1].character,std::optional{CharacterEntryId{1U}}); EXPECT_EQ(event->players[1].x,6); EXPECT_EQ(event->players[1].z,6);
    ASSERT_EQ(event->enemies[0].enemy,std::optional{EnemyEntryId{0U}}); EXPECT_EQ(event->enemies[0].x,4); EXPECT_EQ(event->enemies[0].z,2);
    ASSERT_EQ(event->enemies[1].enemy,std::optional{EnemyEntryId{0U}}); EXPECT_EQ(event->enemies[1].x,6); EXPECT_EQ(event->enemies[1].z,2);

    const auto* enemy=dataset.enemies->find(EnemyEntryId{0U}); ASSERT_NE(enemy,nullptr);
    EXPECT_EQ(enemy->width,1); EXPECT_EQ(enemy->depth,1); EXPECT_EQ(enemy->elementId,4);
    EXPECT_EQ(enemy->movementFlags,0x0fc7); EXPECT_EQ(enemy->counterPercent,10); EXPECT_EQ(enemy->maxHp,58);
    EXPECT_EQ(enemy->quick,18); EXPECT_EQ(enemy->attack,43); EXPECT_EQ(enemy->defense,42);
    EXPECT_EQ(enemy->hitPercent,95); EXPECT_EQ(enemy->dodgePercent,15); EXPECT_EQ(enemy->effectId,-1);
    EXPECT_EQ(enemy->elements,(std::array<std::int16_t,6>{10,10,10,10,10,10}));
    EXPECT_EQ(enemy->itemDrops[0],(EnemyItemDrop{1,1,273}));
    EXPECT_EQ(enemy->itemDrops[1],(EnemyItemDrop{1,1,258}));
    EXPECT_EQ(enemy->itemDrops[2],(EnemyItemDrop{-1,-1,-1}));
    EXPECT_EQ(enemy->itemDrops[3],(EnemyItemDrop{-1,-1,-1}));

    constexpr std::array expectedTasks{
        EnemyTaskFields{0,21,3}, EnemyTaskFields{1,550,2}, EnemyTaskFields{1,551,-1},
    };
    for(std::uint32_t entry=1U;entry<=expectedTasks.size();++entry){
        const auto* task=dataset.enemyTasks->find(EnemyTaskEntryId{EnemyEntryId{0U},entry});
        ASSERT_NE(task,nullptr); EXPECT_EQ(*task,expectedTasks[entry-1U]);
    }

    const auto* vyse=dataset.characters->find(CharacterEntryId{0U}); ASSERT_NE(vyse,nullptr);
    EXPECT_EQ(vyse->name,"Vyse"); EXPECT_EQ(vyse->weaponId,0U); EXPECT_EQ(vyse->armorId,80U); EXPECT_EQ(vyse->accessoryId,204U);
    const auto* aika=dataset.characters->find(CharacterEntryId{1U}); ASSERT_NE(aika,nullptr);
    EXPECT_EQ(aika->name,"Aika"); EXPECT_EQ(aika->weaponId,16U); EXPECT_EQ(aika->armorId,81U); EXPECT_EQ(aika->accessoryId,201U);
    const auto* cutlass=dataset.weapons->find(WeaponEntryId{0U}); ASSERT_NE(cutlass,nullptr); EXPECT_EQ(cutlass->name.text,"Cutlass");
    const auto* boomerang=dataset.weapons->find(WeaponEntryId{16U}); ASSERT_NE(boomerang,nullptr); EXPECT_EQ(boomerang->name.text,"Boomerang");
    const auto* vyseArmor=dataset.armor->find(ArmorEntryId{80U}); ASSERT_NE(vyseArmor,nullptr); EXPECT_EQ(vyseArmor->name.text,"Vyse's Uniform");
    const auto* aikaArmor=dataset.armor->find(ArmorEntryId{81U}); ASSERT_NE(aikaArmor,nullptr); EXPECT_EQ(aikaArmor->name.text,"Aika's Shorts");
    const auto* goggles=dataset.accessories->find(AccessoryEntryId{204U}); ASSERT_NE(goggles,nullptr); EXPECT_EQ(goggles->name.text,"Skyseer Goggles");
    const auto* ribbon=dataset.accessories->find(AccessoryEntryId{201U}); ASSERT_NE(ribbon,nullptr); EXPECT_EQ(ribbon->name.text,"Flash Ribbon");
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
