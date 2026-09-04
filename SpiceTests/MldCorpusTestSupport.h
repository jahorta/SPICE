#pragma once

#include <cstdlib>
#include <string_view>

namespace spice::tests {

inline constexpr char kCorpusTestsEnvironmentVariable[] = "SPICE_RUN_CORPUS_TESTS";
inline constexpr char kCorpusTestsOptInMessage[] =
    "Whole-corpus test disabled by default; set SPICE_RUN_CORPUS_TESTS=1 to enable it";

inline bool corpusTestsEnabled() noexcept
{
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t valueLength = 0U;
    if (_dupenv_s(&value, &valueLength, kCorpusTestsEnvironmentVariable) != 0) {
        return false;
    }
    const bool enabled = value != nullptr && std::string_view{value} == "1";
    std::free(value);
    return enabled;
#else
    const auto* value = std::getenv(kCorpusTestsEnvironmentVariable);
    return value != nullptr && std::string_view{value} == "1";
#endif
}

} // namespace spice::tests
