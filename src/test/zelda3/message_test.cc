#include <gtest/gtest.h>

#include "app/editor/message/message_data.h"
#include "app/editor/message/message_editor.h"
#include "test/core/testing.h"

namespace yaze {
namespace test {
namespace zelda3 {

class MessageTest : public ::testing::Test, public SharedRom {
 protected:
  void SetUp() override {
#if defined(__linux__)
    GTEST_SKIP();
#endif
  }
  void TearDown() override {}

  editor::MessageEditor message_editor_;
  std::vector<editor::DictionaryEntry> dictionary_;
};

TEST_F(MessageTest, LoadMessagesFromRomOk) {
  EXPECT_OK(rom()->LoadFromFile("zelda3.sfc"));
  EXPECT_OK(message_editor_.Initialize());
}

/**
 * @test Verify that a single message can be loaded from the ROM.
 *
 * @details The message is loaded from the ROM and the message is parsed.
 *
 * Message #1 at address 0x0E000B
  RawString:
    [S:00][3][][:75][:44][CH2I]

  Parsed:
  [S:##]A
  [3]give
  [2]give >[CH2I]
 */
TEST_F(MessageTest, VerifySingleMessageFromRomOk) {
  // TODO - Implement this test
}

}  // namespace zelda3
}  // namespace test
}  // namespace yaze
