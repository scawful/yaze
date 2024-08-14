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
MATCHER_P(StatusIs, status, "") { 
  return arg.code() == status;  
}

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_CORE_TESTING_H