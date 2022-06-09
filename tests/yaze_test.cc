#define SDL_MAIN_HANDLED
#include <gtest/gtest.h>

namespace YazeTests {

TEST(YazeApplicationTests, TemplateTest) {
  int i = 0;
  ASSERT_EQ(i, 0);
}

}  // namespace YazeTests

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}