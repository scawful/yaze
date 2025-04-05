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
    EXPECT_OK(rom()->LoadFromFile("zelda3.sfc"));
    dictionary_ = editor::BuildDictionaryEntries(rom());
  }
  void TearDown() override {}

  editor::MessageEditor message_editor_;
  std::vector<editor::DictionaryEntry> dictionary_;
};

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
  // A, B, C, terminator
  std::vector<uint8_t> mock_data = {0x00, 0x01, 0x02, 0x7F, 0x00};
  int pos = 0;

  auto result = editor::ParseSingleMessage(mock_data, &pos);
  ASSERT_TRUE(result.ok());
  const auto message_data = result.value();
  EXPECT_EQ(pos, 4);  // consumed all 4 bytes

  std::vector<editor::MessageData> message_data_vector = {message_data};
  auto parsed = editor::ParseMessageData(message_data_vector, dictionary_);

  EXPECT_THAT(parsed, ::testing::ElementsAre("ABC"));
}

}  // namespace test
}  // namespace yaze
