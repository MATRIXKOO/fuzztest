#include "./fuzztest/internal/configuration.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "./fuzztest/internal/io.h"

namespace fuzztest {
namespace internal {

std::vector<std::string> Configuration::GetCrashingInputs(
    absl::string_view test_name) const {
  if (!replay_crashing) return {};
  std::vector<std::string> inputs =
      ListDirectory(absl::StrCat(corpus_database, "/", test_name, "/crashing"));
  return inputs;
}

std::vector<std::string> Configuration::GetNonCrashingInputs(
    absl::string_view test_name) const {
  std::vector<std::string> inputs = internal::ListDirectory(
      absl::StrCat(corpus_database, "/", test_name, "/regression"));
  if (replay_non_crashing) {
    std::vector<std::string> the_rest = internal::ListDirectory(
        absl::StrCat(corpus_database, "/", test_name, "/coverage"));
    inputs.insert(inputs.end(), the_rest.begin(), the_rest.end());
  }
  return inputs;
}

}  // namespace internal
}  // namespace fuzztest
