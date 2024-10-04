#include <gtest/gtest.h>

#include "app/editor/message/message_data.h"
#include "app/editor/message/message_editor.h"
#include "test/core/testing.h"

namespace yaze {
namespace test {
namespace zelda3 {

class MessageTest : public ::testing::Test, public app::SharedRom {
 protected:
  void SetUp() override {
#if defined(__linux__)
    GTEST_SKIP();
#endif
  }
  void TearDown() override {}

  app::editor::MessageEditor message_editor_;
  std::vector<app::editor::DictionaryEntry> dictionary_;
};

TEST_F(MessageTest, LoadMessagesFromRomOk) {
  EXPECT_OK(rom()->LoadFromFile("zelda3.sfc"));
  EXPECT_OK(message_editor_.Initialize());
}

TEST_F(MessageTest, FindMatchingCharacterOk) {}

}  // namespace zelda3
}  // namespace test
}  // namespace yaze
