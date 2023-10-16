#ifndef FUZZTEST_FUZZTEST_INTERNAL_CONFIGURATION_H_
#define FUZZTEST_FUZZTEST_INTERNAL_CONFIGURATION_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace fuzztest {
namespace internal {

class Configuration {
 public:
  explicit Configuration(absl::string_view corpus_database,
                         bool replay_non_crashing, bool replay_crashing)
      : corpus_database(corpus_database),
        replay_non_crashing(replay_non_crashing),
        replay_crashing(replay_crashing) {}

  // Returns set of all non-crashing inputs from `corpus_database` for a
  // FuzzTest when `replay_non_crashing` is true. Otherwise, returns the set of
  // "regression" inputs for the FuzzTest.
  std::vector<std::string> GetNonCrashingInputs(
      absl::string_view test_name) const;

  // Returns set of all crashing inputs from `corpus_database` for a FuzzTest.
  std::vector<std::string> GetCrashingInputs(absl::string_view test_name) const;

  // When set, replays only one input.
  std::optional<std::string> crashing_input_to_reproduce;

 private:
  // Root of a directory that contains the target corpus.
  std::string corpus_database;
  // Replay all non-crashing inputs in corpus.
  bool replay_non_crashing = false;
  // Replay crashing inputs in corpus.
  bool replay_crashing = false;
};

}  // namespace internal
}  // namespace fuzztest

#endif  // FUZZTEST_FUZZTEST_INTERNAL_CONFIGURATION_H_
