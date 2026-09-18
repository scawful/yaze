#ifndef YAZE_TEST_UNIQUE_TEMP_PATH_H
#define YAZE_TEST_UNIQUE_TEMP_PATH_H

#include <gtest/gtest.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <string>

namespace yaze::test {

// Returns a path under the system temp directory that no other test process
// will use:
//
//   <temp>/<stem>_<Suite>_<Test>_<steady_clock stamp>_<counter><extension>
//
// CI runs ctest with several jobs, and gtest_discover_tests gives every test
// case its own process, so a fixed name such as
// temp_directory_path() / "report.json" is written, read and deleted by
// several cases at once. A different case fails on each run, which reads as
// generic flakiness. Neither the test name alone (a case can run concurrently
// with itself under --repeat or two checkouts) nor rand() (unseeded, so the
// same value in every process) is enough; the stamp and counter make the name
// unique per call.
//
// The path is not created. Call it once per file and keep the result: two
// calls with the same stem return different paths.
inline std::filesystem::path UniqueTempPath(const std::string& stem,
                                            const std::string& extension = "") {
  static std::atomic<unsigned> counter{0};
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  std::string test_name =
      info ? std::string(info->test_suite_name()) + "_" + info->name()
           : std::string("no_test");
  // Parameterized suites put '/' in their names.
  for (char& c : test_name) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
      c = '_';
    }
  }
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         (stem + "_" + test_name + "_" + std::to_string(stamp) + "_" +
          std::to_string(counter++) + extension);
}

}  // namespace yaze::test

#endif  // YAZE_TEST_UNIQUE_TEMP_PATH_H
