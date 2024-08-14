#include <gtest/gtest.h>

#include "yaze.h"

namespace yaze {
namespace test {

TEST(YazeCLibTest, InitializeAndCleanup) {
  yaze_flags flags;
  yaze_init(&flags);
  yaze_cleanup(&flags);
}

}  // namespace test
}  // namespace yaze