#include "CorpusTestSupport.h"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>

namespace {

using spice::tests::CorpusFileType;

TEST(CorpusTestSupport, MapsEveryFileTypeToItsOptInVariable)
{
    struct Expected {
        CorpusFileType fileType;
        std::string_view name;
        std::string_view environmentVariable;
    };
    constexpr std::array expected{
        Expected{CorpusFileType::Mld, "MLD", "SPICE_RUN_MLD_CORPUS_TESTS"},
        Expected{CorpusFileType::Pvm, "PVM", "SPICE_RUN_PVM_CORPUS_TESTS"},
        Expected{CorpusFileType::Std, "STD", "SPICE_RUN_STD_CORPUS_TESTS"},
        Expected{CorpusFileType::Ect, "ECT", "SPICE_RUN_ECT_CORPUS_TESTS"},
        Expected{CorpusFileType::Mll, "MLL", "SPICE_RUN_MLL_CORPUS_TESTS"},
        Expected{CorpusFileType::Alx, "ALX", "SPICE_RUN_ALX_CORPUS_TESTS"},
        Expected{CorpusFileType::SstSml, "SST/SML", "SPICE_RUN_SST_SML_CORPUS_TESTS"},
        Expected{CorpusFileType::Sct, "SCT", "SPICE_RUN_SCT_CORPUS_TESTS"},
    };

    for (const auto& item : expected) {
        EXPECT_EQ(std::string_view{spice::tests::corpusFileTypeName(item.fileType)}, item.name);
        EXPECT_EQ(std::string_view{spice::tests::corpusFileTypeEnvironmentVariable(item.fileType)}, item.environmentVariable);
        const auto message = spice::tests::corpusTestsOptInMessage(item.fileType);
        EXPECT_NE(message.find(item.environmentVariable), std::string::npos);
        EXPECT_NE(message.find(spice::tests::kCorpusTestsEnvironmentVariable), std::string::npos);
    }
}

TEST(CorpusTestSupport, RequiresExactOneFromGlobalOrFileTypeValue)
{
    EXPECT_FALSE(spice::tests::corpusOptInRequested("", ""));
    EXPECT_FALSE(spice::tests::corpusOptInRequested("0", "0"));
    EXPECT_FALSE(spice::tests::corpusOptInRequested("true", "TRUE"));
    EXPECT_TRUE(spice::tests::corpusOptInRequested("1", ""));
    EXPECT_TRUE(spice::tests::corpusOptInRequested("", "1"));
    EXPECT_TRUE(spice::tests::corpusOptInRequested("1", "0"));
}

TEST(CorpusTestSupport, GlobalOptInOverridesFileTypeAndFileTypeOptInIsIsolated)
{
    const auto globalOnly = [](const char* name) noexcept {
        return std::string_view{name} == spice::tests::kCorpusTestsEnvironmentVariable;
    };
    EXPECT_TRUE(spice::tests::corpusTestsEnabled(CorpusFileType::Mld, globalOnly));
    EXPECT_TRUE(spice::tests::corpusTestsEnabled(CorpusFileType::Alx, globalOnly));

    const auto mldOnly = [](const char* name) noexcept {
        return std::string_view{name} == "SPICE_RUN_MLD_CORPUS_TESTS";
    };
    EXPECT_TRUE(spice::tests::corpusTestsEnabled(CorpusFileType::Mld, mldOnly));
    EXPECT_FALSE(spice::tests::corpusTestsEnabled(CorpusFileType::Pvm, mldOnly));
    EXPECT_FALSE(spice::tests::corpusTestsEnabled(CorpusFileType::Std, mldOnly));
}

} // namespace
