#include "./fuzztest/init_fuzztest.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/flags/flag.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "./fuzztest/internal/configuration.h"
#include "./fuzztest/internal/googletest_adaptor.h"
#include "./fuzztest/internal/registry.h"
#include "./fuzztest/internal/runtime.h"

#define FUZZTEST_FLAG_PREFIX ""
#define FUZZTEST_FLAG_NAME(name) name
#define FUZZTEST_FLAG(name) FLAGS_##name

#define FUZZTEST_DEFINE_FLAG(type, name, default_value, description) \
  ABSL_FLAG(type, FUZZTEST_FLAG_NAME(name), default_value, description)

FUZZTEST_DEFINE_FLAG(
    bool, list_fuzz_tests, false,
    "Prints (to stdout) the list of all available FUZZ_TEST-s in the "
    "binary and exits. I.e., prints the test names that can be run with "
    "the flag `--" FUZZTEST_FLAG_PREFIX "fuzz=<test name>`.");

static constexpr absl::string_view kUnspecified = "<unspecified>";

FUZZTEST_DEFINE_FLAG(
    std::string, fuzz, std::string(kUnspecified),
    "Runs a single FUZZ_TEST in continuous fuzzing mode. "
    "E.g., `--" FUZZTEST_FLAG_PREFIX
    "fuzz=MySuite.MyFuzzTest` runs the given FUZZ_TEST in "
    "fuzzing mode. You can also provide just a part of the name, e.g., "
    "`--" FUZZTEST_FLAG_PREFIX
    "fuzz=MyFuzz`, if it matches only a single FUZZ_TEST. "
    "If you have only one fuzz test in your binary, you can also use "
    "`--" FUZZTEST_FLAG_PREFIX
    "fuzz=` to run it in fuzzing mode (i.e., by setting the "
    "flag to empty string). "
    "In fuzzing mode the selected test runs until a bug is found or "
    "until manually stopped. Fuzzing mode uses coverage feedback to "
    "iteratively build up a corpus of inputs that maximize coverage and "
    "to reach deep bugs. Note that the binary must be compiled with "
    "`--config=fuzztest` for this to work, as it needs coverage "
    "instrumentation.");

FUZZTEST_DEFINE_FLAG(
    absl::Duration, fuzz_for, absl::InfiniteDuration(),
    "Runs all fuzz tests in fuzzing mode for the specified duration. Can "
    "be combined with --" FUZZTEST_FLAG_PREFIX
    "fuzz to select a single fuzz tests, or "
    "with --" FUZZTEST_FLAG_PREFIX
    "filter to select a subset of fuzz tests. Recommended "
    "to use with test sharding.");

FUZZTEST_DEFINE_FLAG(
    std::string, corpus_database, "",
    "Explores the corpus for the Fuzz target in the `corpus_database` where "
    "the corpus directory has the following structure: (1) For each "
    "SuiteName.TestName in the target, there's a sub-directory with the name "
    "of that test ('<corpus_database>/SuiteName.TestName'). (3) For each "
    "FuzzTest, there are three directories containing 'regression`, "
    "`crashing`, `coverage` directory. The files in the `regression` directory "
    "will always be used and it's assumed that they are non-crashing. The "
    "files in `crashing` directory will be used when "
    "--reproduce_corpus_crashes flag is true. And finally all files not in "
    "`crashes` directory will be used when --replay_corpus flag is true.");

FUZZTEST_DEFINE_FLAG(bool, reproduce_findings, false,
                     "When true, the selected tests replay all crashing inputs "
                     "in the database for a given test.");

FUZZTEST_DEFINE_FLAG(
    bool, replay_corpus, false,
    "When true, the selected tests replay all non-crashing inputs in the "
    "database for a given test. This is useful for measuring the coverage of "
    "the corpus built up during previously ran fuzzing sessions.");

namespace fuzztest {

std::vector<std::string> ListRegisteredTests() {
  std::vector<std::string> result;
  internal::ForEachTest(
      [&](const auto& test) { result.push_back(test.full_name()); });
  return result;
}

std::string GetMatchingFuzzTestOrExit(std::string_view name) {
  const std::string partial_name(name);
  const std::vector<std::string> full_names = ListRegisteredTests();
  std::vector<const std::string*> matches;
  for (const std::string& full_name : full_names) {
    if (absl::StrContains(full_name, partial_name)) {
      if (full_name == partial_name) {
        // In case of an exact match, we end the search and use it. This is to
        // handle the case when we want to select `MySuite.MyTest`, but the
        // binary has both `MySuite.MyTest` and `MySuite.MyTestX`.
        return full_name;
      } else {
        matches.push_back(&full_name);
      }
    }
  }

  if (matches.empty()) {
    absl::FPrintF(stderr, "\n\nNo FUZZ_TEST matches the name: %s\n\n", name);
    absl::FPrintF(stderr, "Valid tests:\n");
    for (const std::string& full_name : full_names) {
      absl::FPrintF(stderr, " %s\n", full_name);
    }
    exit(1);
  } else if (matches.size() > 1) {
    absl::FPrintF(stderr, "\n\nMultiple FUZZ_TESTs match the name: %s\n\n",
                  name);
    absl::FPrintF(stderr, "Please select one. Matching tests:\n");
    for (const std::string* full_name : matches) {
      absl::FPrintF(stderr, " %s\n", *full_name);
    }
    exit(1);
  }
  return *matches[0];
}

void RunSpecifiedFuzzTest(std::string_view name) {
  const std::string matching_fuzz_test = GetMatchingFuzzTestOrExit(name);
  internal::ForEachTest([&](auto& test) {
    // TODO(b/301965259): Properly initialize the configuration.
    internal::Configuration configuration(/*corpus_database=*/"",
                                          /*replay_non_crashing=*/false,
                                          /*replay_crashing=*/false);
    if (test.full_name() == matching_fuzz_test) {
      exit(std::move(test).make()->RunInFuzzingMode(/*argc=*/nullptr,
                                                    /*argv=*/nullptr,
                                                    configuration));
    }
  });
}

void InitFuzzTest(int* argc, char*** argv) {
  const bool is_listing = absl::GetFlag(FUZZTEST_FLAG(list_fuzz_tests));
  if (is_listing) {
    for (const auto& name : ListRegisteredTests()) {
      std::cout << "[*] Fuzz test: " << name << '\n';
    }
    std::exit(0);
  }

  const auto test_to_fuzz = absl::GetFlag(FUZZTEST_FLAG(fuzz));
  const bool is_test_to_fuzz_specified = test_to_fuzz != kUnspecified;
  if (is_test_to_fuzz_specified) {
    const std::string matching_fuzz_test =
        GetMatchingFuzzTestOrExit(test_to_fuzz);
    // Delegate the test to GoogleTest.
    GTEST_FLAG_SET(filter, matching_fuzz_test);
  }

  const auto duration = absl::GetFlag(FUZZTEST_FLAG(fuzz_for));
  const bool is_duration_specified =
      absl::ZeroDuration() < duration && duration < absl::InfiniteDuration();
  if (is_duration_specified) {
    internal::Runtime::instance().SetFuzzTimeLimit(duration);
  }

  // TODO(b/301965259): Use the default corpus when corpus_path is empty.

  internal::Configuration configuration{
      absl::GetFlag(FUZZTEST_FLAG(corpus_database)),
      absl::GetFlag(FUZZTEST_FLAG(replay_corpus)),
      absl::GetFlag(FUZZTEST_FLAG(reproduce_findings)),
  };
  internal::RegisterFuzzTestsAsGoogleTests(argc, argv, configuration);

  const RunMode run_mode = is_test_to_fuzz_specified || is_duration_specified
                               ? RunMode::kFuzz
                               : RunMode::kUnitTest;
  internal::Runtime::instance().SetRunMode(run_mode);
}

}  // namespace fuzztest
