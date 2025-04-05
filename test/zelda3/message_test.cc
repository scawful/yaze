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

TEST_F(MessageTest, FindMatchingCharacter_Success) {
  EXPECT_EQ(editor::FindMatchingCharacter('A'), 0x00);
  EXPECT_EQ(editor::FindMatchingCharacter('Z'), 0x19);
  EXPECT_EQ(editor::FindMatchingCharacter('a'), 0x1A);
  EXPECT_EQ(editor::FindMatchingCharacter('z'), 0x33);
}

TEST_F(MessageTest, FindMatchingCharacter_Failure) {
  EXPECT_EQ(editor::FindMatchingCharacter('@'), 0xFF);
  EXPECT_EQ(editor::FindMatchingCharacter('#'), 0xFF);
}

TEST_F(MessageTest, FindDictionaryEntry_Success) {
  EXPECT_EQ(editor::FindDictionaryEntry(0x88), 0x00);
  EXPECT_EQ(editor::FindDictionaryEntry(0x90), 0x08);
}

TEST_F(MessageTest, FindDictionaryEntry_Failure) {
  EXPECT_EQ(editor::FindDictionaryEntry(0x00), -1);
  EXPECT_EQ(editor::FindDictionaryEntry(0xFF), -1);
}

TEST_F(MessageTest, ParseMessageToData_Basic) {
  std::string input = "[L][C:01]ABC";
  auto result = editor::ParseMessageToData(input);
  std::vector<uint8_t> expected = {0x6A, 0x77, 0x01, 0x00, 0x01, 0x02};
  EXPECT_EQ(result, expected);
}

TEST_F(MessageTest, ReplaceAllDictionaryWords_Success) {
  std::vector<editor::DictionaryEntry> mock_dict = {
      editor::DictionaryEntry(0x00, "test"),
      editor::DictionaryEntry(0x01, "message")};
  std::string input = "This is a test message.";
  auto result = editor::ReplaceAllDictionaryWords(input, mock_dict);
  EXPECT_EQ(result, "This is a [D:00] [D:01].");
}

TEST_F(MessageTest, ReplaceAllDictionaryWords_NoMatch) {
  std::vector<editor::DictionaryEntry> mock_dict = {
      editor::DictionaryEntry(0x00, "hello")};
  std::string input = "No matching words.";
  auto result = editor::ReplaceAllDictionaryWords(input, mock_dict);
  EXPECT_EQ(result, "No matching words.");
}

TEST_F(MessageTest, ParseTextDataByte_Success) {
  EXPECT_EQ(editor::ParseTextDataByte(0x00), "A");
  EXPECT_EQ(editor::ParseTextDataByte(0x74), "[1]");
  EXPECT_EQ(editor::ParseTextDataByte(0x88), "[D:00]");
}

TEST_F(MessageTest, ParseTextDataByte_Failure) {
  EXPECT_EQ(editor::ParseTextDataByte(0xFF), "");
}

TEST_F(MessageTest, ParseSingleMessage_EmptyData) {
  std::vector<uint8_t> mock_data = {0x7F};
  int pos = 0;

  auto result = editor::ParseSingleMessage(mock_data, &pos);
  ASSERT_TRUE(result.ok());
  const auto message_data = result.value();

  EXPECT_EQ(message_data.ContentsParsed, "");
  EXPECT_EQ(pos, 1);
}

TEST_F(MessageTest, OptimizeMessageForDictionary_Basic) {
  std::vector<editor::DictionaryEntry> mock_dict = {
      editor::DictionaryEntry(0x00, "Link"),
      editor::DictionaryEntry(0x01, "Zelda")};
  std::string input = "[L] rescued Zelda from danger.";

  editor::MessageData message_data;
  std::string optimized =
      message_data.OptimizeMessageForDictionary(input, mock_dict);

  EXPECT_EQ(optimized, "[L] rescued [D:01] from danger.");
}

TEST_F(MessageTest, SetMessage_Success) {
  std::vector<editor::DictionaryEntry> mock_dict = {
      editor::DictionaryEntry(0x00, "item")};
  editor::MessageData message_data;
  std::string input = "You got an item!";

  message_data.SetMessage(input, mock_dict);

  EXPECT_EQ(message_data.RawString, "You got an item!");
  EXPECT_EQ(message_data.ContentsParsed, "You got an [D:00]!");
}

TEST_F(MessageTest, FindMatchingElement_CommandWithArgument) {
  std::string input = "[W:02]";
  editor::ParsedElement result = editor::FindMatchingElement(input);

  EXPECT_TRUE(result.Active);
  EXPECT_EQ(result.Parent.Token, "W");
  EXPECT_EQ(result.Value, 0x02);
}

TEST_F(MessageTest, FindMatchingElement_InvalidCommand) {
  std::string input = "[INVALID]";
  editor::ParsedElement result = editor::FindMatchingElement(input);

  EXPECT_FALSE(result.Active);
}

TEST_F(MessageTest, ImportMessageData_InvalidFile) {
  auto result = editor::ImportMessageData("nonexistent_file.txt");
  EXPECT_TRUE(result.empty());
}

TEST_F(MessageTest, BuildDictionaryEntries_CorrectSize) {
  auto result = editor::BuildDictionaryEntries(rom());
  EXPECT_EQ(result.size(), editor::kNumDictionaryEntries);
  EXPECT_FALSE(result.empty());
}

}  // namespace test
}  // namespace yaze
