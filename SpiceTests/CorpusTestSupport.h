#pragma once

#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>

namespace spice::tests {

enum class CorpusFileType {
    Mld,
    Pvm,
    Std,
    Ect,
    Mll,
    Alx,
    SstSml,
    Sct,
};

inline constexpr char kCorpusTestsEnvironmentVariable[] = "SPICE_RUN_CORPUS_TESTS";

inline constexpr const char* corpusFileTypeName(const CorpusFileType fileType) noexcept
{
    switch (fileType) {
    case CorpusFileType::Mld: return "MLD";
    case CorpusFileType::Pvm: return "PVM";
    case CorpusFileType::Std: return "STD";
    case CorpusFileType::Ect: return "ECT";
    case CorpusFileType::Mll: return "MLL";
    case CorpusFileType::Alx: return "ALX";
    case CorpusFileType::SstSml: return "SST/SML";
    case CorpusFileType::Sct: return "SCT";
    }
    return "unknown";
}

inline constexpr const char* corpusFileTypeEnvironmentVariable(const CorpusFileType fileType) noexcept
{
    switch (fileType) {
    case CorpusFileType::Mld: return "SPICE_RUN_MLD_CORPUS_TESTS";
    case CorpusFileType::Pvm: return "SPICE_RUN_PVM_CORPUS_TESTS";
    case CorpusFileType::Std: return "SPICE_RUN_STD_CORPUS_TESTS";
    case CorpusFileType::Ect: return "SPICE_RUN_ECT_CORPUS_TESTS";
    case CorpusFileType::Mll: return "SPICE_RUN_MLL_CORPUS_TESTS";
    case CorpusFileType::Alx: return "SPICE_RUN_ALX_CORPUS_TESTS";
    case CorpusFileType::SstSml: return "SPICE_RUN_SST_SML_CORPUS_TESTS";
    case CorpusFileType::Sct: return "SPICE_RUN_SCT_CORPUS_TESTS";
    }
    return "";
}

inline constexpr bool corpusOptInRequested(
    const std::string_view globalValue,
    const std::string_view fileTypeValue) noexcept
{
    return globalValue == "1" || fileTypeValue == "1";
}

inline bool environmentVariableEqualsOne(const char* name) noexcept
{
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t valueLength = 0U;
    if (_dupenv_s(&value, &valueLength, name) != 0) {
        return false;
    }
    const bool enabled = value != nullptr && std::string_view{value} == "1";
    std::free(value);
    return enabled;
#else
    const auto* value = std::getenv(name);
    return value != nullptr && std::string_view{value} == "1";
#endif
}

using CorpusEnvironmentLookup = bool (*)(const char* name);

inline bool corpusTestsEnabled(
    const CorpusFileType fileType,
    const CorpusEnvironmentLookup environmentEnabled) noexcept
{
    return environmentEnabled(kCorpusTestsEnvironmentVariable) ||
        environmentEnabled(corpusFileTypeEnvironmentVariable(fileType));
}

inline bool corpusTestsEnabled(const CorpusFileType fileType) noexcept
{
    return corpusTestsEnabled(fileType, environmentVariableEqualsOne);
}

inline std::string corpusTestsOptInMessage(const CorpusFileType fileType)
{
    return std::string{corpusFileTypeName(fileType)} +
        " corpus test disabled by default; set " +
        corpusFileTypeEnvironmentVariable(fileType) +
        "=1 for this filetype or " + kCorpusTestsEnvironmentVariable +
        "=1 for all corpus tests";
}

} // namespace spice::tests
