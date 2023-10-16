
#include "./fuzztest/internal/googletest_adaptor.h"

#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "./fuzztest/internal/configuration.h"
#include "./fuzztest/internal/registry.h"
#include "./fuzztest/internal/runtime.h"

namespace fuzztest::internal {

namespace {

absl::string_view GetFileName(absl::string_view filepath) {
  std::vector<absl::string_view> parts = absl::StrSplit(filepath, '/');
  if (parts.empty()) return filepath;
  return parts.back();
}

template <typename T>
void RegisterTest(int* argc, char*** argv, FuzzTest& test,
                  const Configuration& configuration,
                  absl::string_view suffix = "") {
  auto fixture_factory = [argc, argv, &test,
                          configuration = configuration]() -> T* {
    return new ::fuzztest::internal::GTest_TestAdaptor(test, argc, argv,
                                                       configuration);
  };
  const std::string test_name_with_suffix =
      absl::StrCat(test.test_name(), suffix);
  ::testing::RegisterTest(test.suite_name(), test_name_with_suffix.c_str(),
                          nullptr, nullptr, test.file(), test.line(),
                          std::move(fixture_factory));
}

template <typename T>
void RegisterTests(int* argc, char*** argv, FuzzTest& test,
                   const Configuration& configuration) {
  RegisterTest<T>(argc, argv, test, configuration);
  for (const std::string& input :
       configuration.GetCrashingInputs(test.full_name())) {
    Configuration updated_configuration = configuration;
    updated_configuration.crashing_input_to_reproduce = input;
    const std::string suffix = absl::StrCat("/replay/", GetFileName(input));
    RegisterTest<T>(argc, argv, test, updated_configuration, suffix);
  }
}

}  // namespace

void RegisterFuzzTestsAsGoogleTests(int* argc, char*** argv,
                                    const Configuration& configuration) {
  ::fuzztest::internal::ForEachTest([&](auto& test) {
    if (test.uses_fixture()) {
      RegisterTests<::fuzztest::internal::GTest_TestAdaptor>(argc, argv, test,
                                                             configuration);
    } else {
      RegisterTests<::testing::Test>(argc, argv, test, configuration);
    }
  });

  ::testing::UnitTest::GetInstance()->listeners().Append(
      new ::fuzztest::internal::GTest_EventListener<
          ::testing::EmptyTestEventListener, ::testing::TestPartResult>());
}

}  // namespace fuzztest::internal
