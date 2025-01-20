#include <gtest/gtest.h>

#include "app/editor/message/message_data.h"
#include "app/editor/message/message_editor.h"
#include "test/testing.h"

namespace yaze {
namespace test {

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

  Message ID: 2
  Raw: [S:00][3][][:75][:44][CH2I]
  Parsed: [S:00][3][][:75][:44][CH2I]
  Raw Bytes: 7A 00 76 88 8A 75 88 44 68
  Parsed Bytes: 7A 00 76 88 8A 75 88 44 68

 */
TEST_F(MessageTest, VerifySingleMessageFromRomOk) {
  // TODO - Implement this test
}

TEST_F(MessageTest, ParseSingleMessage_CommandParsing) {
  std::vector<uint8_t> mock_data = {0x6A, 0x7F, 0x00};
  int pos = 0;

  auto result = editor::ParseSingleMessage(mock_data, &pos);
  EXPECT_TRUE(result.ok());
  const auto message_data = result.value();

  // Verify that the command was recognized and parsed
  EXPECT_EQ(message_data.ContentsParsed, "[L]");
  EXPECT_EQ(pos, 2);
}

TEST_F(MessageTest, ParseSingleMessage_BasicAscii) {
  std::vector<uint8_t> mock_data = {0x41, 0x42, 0x43, 0x7F,
                                    0x00};  // A, B, C, terminator
  int pos = 0;

  auto result = editor::ParseSingleMessage(mock_data, &pos);
  ASSERT_TRUE(result.ok());
  const auto message_data = result.value();

  EXPECT_EQ(message_data.RawString, "ABC");
  EXPECT_EQ(message_data.ContentsParsed, "ABC");
  EXPECT_EQ(pos, 4);  // consumed all 4 bytes
}

}  // namespace test
}  // namespace yaze
