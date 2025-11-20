#ifndef YAZE_TEST_CORE_TESTING_H
#define YAZE_TEST_CORE_TESTING_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

#define EXPECT_OK(expr) EXPECT_EQ((expr), absl::OkStatus())

#define ASSERT_OK(expr) ASSERT_EQ((expr), absl::OkStatus())

#define ASSERT_OK_AND_ASSIGN(lhs, rexpr)              \
  if (auto rexpr_value = (rexpr); rexpr_value.ok()) { \
    lhs = std::move(rexpr_value).value();             \
  } else {                                            \
    FAIL() << "error: " << rexpr_value.status();      \
  }

namespace yaze {
namespace test {

// StatusIs is a matcher that matches a status that has the same code and
// message as the expected status.
MATCHER_P(StatusIs, status, "") { return arg.code() == status; }

// Support for testing absl::StatusOr.
template <typename T>
::testing::AssertionResult IsOkAndHolds(const absl::StatusOr<T>& status_or,
                                        const T& value) {
  if (!status_or.ok()) {
    return ::testing::AssertionFailure()
           << "Expected status to be OK, but got: " << status_or.status();
  }
  if (status_or.value() != value) {
    return ::testing::AssertionFailure() << "Expected value to be " << value
                                         << ", but got: " << status_or.value();
  }
  return ::testing::AssertionSuccess();
}

MATCHER_P(IsOkAndHolds, value, "") { return IsOkAndHolds(arg, value); }

// Helper to test if a StatusOr contains an error with a specific message
MATCHER_P(StatusIsWithMessage, message, "") {
  return !arg.ok() && arg.status().message() == message;
}

// Helper to test if a StatusOr contains an error with a specific code
MATCHER_P(StatusIsWithCode, code, "") {
  return !arg.ok() && arg.status().code() == code;
}

// Helper to test if a StatusOr is OK and contains a value that matches a
// matcher
template <typename T, typename Matcher>
::testing::AssertionResult IsOkAndMatches(const absl::StatusOr<T>& status_or,
                                          const Matcher& matcher) {
  if (!status_or.ok()) {
    return ::testing::AssertionFailure()
           << "Expected status to be OK, but got: " << status_or.status();
  }
  if (!::testing::Matches(matcher)(status_or.value())) {
    return ::testing::AssertionFailure()
           << "Value does not match expected matcher";
  }
  return ::testing::AssertionSuccess();
}

// Helper to test if two StatusOr values are equal
template <typename T>
::testing::AssertionResult StatusOrEqual(const absl::StatusOr<T>& a,
                                         const absl::StatusOr<T>& b) {
  if (a.ok() != b.ok()) {
    return ::testing::AssertionFailure()
           << "One status is OK while the other is not";
  }
  if (!a.ok()) {
    return ::testing::AssertionSuccess();
  }
  if (a.value() != b.value()) {
    return ::testing::AssertionFailure()
           << "Values are not equal: " << a.value() << " vs " << b.value();
  }
  return ::testing::AssertionSuccess();
}

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_CORE_TESTING_H
