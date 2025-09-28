#include <gtest/gtest.h>
#include <filesystem>

#include "app/editor/message/message_data.h"
#include "app/editor/message/message_editor.h"
#include "testing.h"

namespace yaze {
namespace test {

class MessageRomTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Skip tests if ROM is not available
    if (getenv("YAZE_SKIP_ROM_TESTS")) {
      GTEST_SKIP() << "ROM tests disabled";
    }

    // Check if ROM file exists
    std::string rom_path = "zelda3.sfc";
    if (!std::filesystem::exists(rom_path)) {
      GTEST_SKIP() << "Test ROM not found: " << rom_path;
    }

    EXPECT_OK(rom_.LoadFromFile(rom_path));
    dictionary_ = editor::BuildDictionaryEntries(&rom_);
  }
  void TearDown() override {}

  Rom rom_;
  editor::MessageEditor message_editor_;
  std::vector<editor::DictionaryEntry> dictionary_;
};

TEST_F(MessageRomTest, ParseSingleMessage_CommandParsing) {
  std::vector<uint8_t> mock_data = {0x6A, 0x7F, 0x00};
  int pos = 0;

  auto result = editor::ParseSingleMessage(mock_data, &pos);
  EXPECT_TRUE(result.ok());
  const auto message_data = result.value();

  // Verify that the command was recognized and parsed
  EXPECT_EQ(message_data.ContentsParsed, "[L]");
  EXPECT_EQ(pos, 2);
}

TEST_F(MessageRomTest, ParseSingleMessage_BasicAscii) {
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

TEST_F(MessageRomTest, FindMatchingCharacter_Success) {
  EXPECT_EQ(editor::FindMatchingCharacter('A'), 0x00);
  EXPECT_EQ(editor::FindMatchingCharacter('Z'), 0x19);
  EXPECT_EQ(editor::FindMatchingCharacter('a'), 0x1A);
  EXPECT_EQ(editor::FindMatchingCharacter('z'), 0x33);
}

TEST_F(MessageRomTest, FindMatchingCharacter_Failure) {
  EXPECT_EQ(editor::FindMatchingCharacter('@'), 0xFF);
  EXPECT_EQ(editor::FindMatchingCharacter('#'), 0xFF);
}

TEST_F(MessageRomTest, FindDictionaryEntry_Success) {
  EXPECT_EQ(editor::FindDictionaryEntry(0x88), 0x00);
  EXPECT_EQ(editor::FindDictionaryEntry(0x90), 0x08);
}

TEST_F(MessageRomTest, FindDictionaryEntry_Failure) {
  EXPECT_EQ(editor::FindDictionaryEntry(0x00), -1);
  EXPECT_EQ(editor::FindDictionaryEntry(0xFF), -1);
}

TEST_F(MessageRomTest, ParseMessageToData_Basic) {
  std::string input = "[L][C:01]ABC";
  auto result = editor::ParseMessageToData(input);
  std::vector<uint8_t> expected = {0x6A, 0x77, 0x01, 0x00, 0x01, 0x02};
  EXPECT_EQ(result, expected);
}

TEST_F(MessageRomTest, ReplaceAllDictionaryWords_Success) {
  std::vector<editor::DictionaryEntry> mock_dict = {
      editor::DictionaryEntry(0x00, "test"),
      editor::DictionaryEntry(0x01, "message")};
  std::string input = "This is a test message.";
  auto result = editor::ReplaceAllDictionaryWords(input, mock_dict);
  EXPECT_EQ(result, "This is a [D:00] [D:01].");
}

TEST_F(MessageRomTest, ReplaceAllDictionaryWords_NoMatch) {
  std::vector<editor::DictionaryEntry> mock_dict = {
      editor::DictionaryEntry(0x00, "hello")};
  std::string input = "No matching words.";
  auto result = editor::ReplaceAllDictionaryWords(input, mock_dict);
  EXPECT_EQ(result, "No matching words.");
}

TEST_F(MessageRomTest, ParseTextDataByte_Success) {
  EXPECT_EQ(editor::ParseTextDataByte(0x00), "A");
  EXPECT_EQ(editor::ParseTextDataByte(0x74), "[1]");
  EXPECT_EQ(editor::ParseTextDataByte(0x88), "[D:00]");
}

TEST_F(MessageRomTest, ParseTextDataByte_Failure) {
  EXPECT_EQ(editor::ParseTextDataByte(0xFF), "");
}

TEST_F(MessageRomTest, ParseSingleMessage_SpecialCharacters) {
  std::vector<uint8_t> mock_data = {0x4D, 0x4E, 0x4F, 0x50, 0x7F};
  int pos = 0;

  auto result = editor::ParseSingleMessage(mock_data, &pos);
  ASSERT_TRUE(result.ok());
  const auto message_data = result.value();

  EXPECT_EQ(message_data.ContentsParsed, "[UP][DOWN][LEFT][RIGHT]");
  EXPECT_EQ(pos, 5);
}

TEST_F(MessageRomTest, ParseSingleMessage_DictionaryReference) {
  std::vector<uint8_t> mock_data = {0x88, 0x89, 0x7F};
  int pos = 0;

  auto result = editor::ParseSingleMessage(mock_data, &pos);
  ASSERT_TRUE(result.ok());
  const auto message_data = result.value();

  EXPECT_EQ(message_data.ContentsParsed, "[D:00][D:01]");
  EXPECT_EQ(pos, 3);
}

TEST_F(MessageRomTest, ParseSingleMessage_InvalidTerminator) {
  std::vector<uint8_t> mock_data = {0x00, 0x01, 0x02};  // No terminator
  int pos = 0;

  auto result = editor::ParseSingleMessage(mock_data, &pos);
  EXPECT_FALSE(result.ok());
}

TEST_F(MessageRomTest, ParseSingleMessage_EmptyData) {
  std::vector<uint8_t> mock_data = {0x7F};
  int pos = 0;

  auto result = editor::ParseSingleMessage(mock_data, &pos);
  ASSERT_TRUE(result.ok());
  const auto message_data = result.value();

  EXPECT_EQ(message_data.ContentsParsed, "");
  EXPECT_EQ(pos, 1);
}

TEST_F(MessageRomTest, OptimizeMessageForDictionary_Basic) {
  std::vector<editor::DictionaryEntry> mock_dict = {
      editor::DictionaryEntry(0x00, "Link"),
      editor::DictionaryEntry(0x01, "Zelda")};
  std::string input = "[L] rescued Zelda from danger.";

  editor::MessageData message_data;
  std::string optimized =
      message_data.OptimizeMessageForDictionary(input, mock_dict);

  EXPECT_EQ(optimized, "[L] rescued [D:01] from danger.");
}

TEST_F(MessageRomTest, SetMessage_Success) {
  std::vector<editor::DictionaryEntry> mock_dict = {
      editor::DictionaryEntry(0x00, "item")};
  editor::MessageData message_data;
  std::string input = "You got an item!";

  message_data.SetMessage(input, mock_dict);

  EXPECT_EQ(message_data.RawString, "You got an item!");
  EXPECT_EQ(message_data.ContentsParsed, "You got an [D:00]!");
}

TEST_F(MessageRomTest, FindMatchingElement_CommandWithArgument) {
  std::string input = "[W:02]";
  editor::ParsedElement result = editor::FindMatchingElement(input);

  EXPECT_TRUE(result.Active);
  EXPECT_EQ(result.Parent.Token, "W");
  EXPECT_EQ(result.Value, 0x02);
}

TEST_F(MessageRomTest, FindMatchingElement_InvalidCommand) {
  std::string input = "[INVALID]";
  editor::ParsedElement result = editor::FindMatchingElement(input);

  EXPECT_FALSE(result.Active);
}

TEST_F(MessageRomTest, BuildDictionaryEntries_CorrectSize) {
  auto result = editor::BuildDictionaryEntries(&rom_);
  EXPECT_EQ(result.size(), editor::kNumDictionaryEntries);
  EXPECT_FALSE(result.empty());
}

}  // namespace test
}  // namespace yaze
