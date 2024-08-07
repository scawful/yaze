#include <gtest/gtest.h>

#include "yaze.h"

namespace yaze_test {

TEST(YazeCLibTest, InitializeAndCleanup) {
  yaze_initialize();
  yaze_cleanup();
}

}  // namespace yaze_test