#include <gtest/gtest.h>

#include "yaze.h"

namespace yaze_test {

TEST(YazeCLibTest, InitializeAndCleanup) {
  yaze_flags flags;
  yaze_init(&flags);
  yaze_cleanup();
}

}  // namespace yaze_test