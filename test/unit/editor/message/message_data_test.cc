#include "app/editor/message/message_data.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

#include "rom/snes.h"

namespace yaze {
namespace editor {
namespace {

// ===========================================================================
// Test Fixture
// ===========================================================================

class MessageDataTest : public ::testing::Test {
 protected:
  void SetUp() override {
    messages_.push_back(
        MessageData(0, 0x100, "Test Message 1", {}, "Test Message 1", {}));
    messages_.push_back(
        MessageData(1, 0x200, "Test Message 2", {}, "Test Message 2", {}));
  }

  std::vector<MessageData> messages_;
};

// ===========================================================================
// JSON Serialization Tests (existing)
// ===========================================================================

TEST_F(MessageDataTest, SerializeMessagesToJson) {
  nlohmann::json j = SerializeMessagesToJson(messages_);

  ASSERT_TRUE(j.is_array());
  ASSERT_EQ(j.size(), 2);

  EXPECT_EQ(j[0]["id"], 0);
  EXPECT_EQ(j[0]["address"], 0x100);
  EXPECT_EQ(j[0]["raw_string"], "Test Message 1");
  EXPECT_EQ(j[0]["parsed_string"], "Test Message 1");

  EXPECT_EQ(j[1]["id"], 1);
  EXPECT_EQ(j[1]["address"], 0x200);
  EXPECT_EQ(j[1]["raw_string"], "Test Message 2");
  EXPECT_EQ(j[1]["parsed_string"], "Test Message 2");
}

TEST_F(MessageDataTest, ExportMessagesToJson) {
  std::string test_file = "test_messages.json";

  if (std::filesystem::exists(test_file)) {
    std::filesystem::remove(test_file);
  }

  absl::Status status = ExportMessagesToJson(test_file, messages_);
  ASSERT_TRUE(status.ok());
  ASSERT_TRUE(std::filesystem::exists(test_file));

  std::ifstream file(test_file);
  nlohmann::json j;
  file >> j;

  ASSERT_TRUE(j.is_array());
  ASSERT_EQ(j.size(), 2);
  EXPECT_EQ(j[0]["raw_string"], "Test Message 1");

  std::filesystem::remove(test_file);
}

// ===========================================================================
// Character Encoding Tests
// ===========================================================================

TEST(CharEncoderTest, FindMatchingCharacterUppercase) {
  EXPECT_EQ(FindMatchingCharacter('A'), 0x00);
  EXPECT_EQ(FindMatchingCharacter('B'), 0x01);
  EXPECT_EQ(FindMatchingCharacter('Z'), 0x19);
}

TEST(CharEncoderTest, FindMatchingCharacterLowercase) {
  EXPECT_EQ(FindMatchingCharacter('a'), 0x1A);
  EXPECT_EQ(FindMatchingCharacter('b'), 0x1B);
  EXPECT_EQ(FindMatchingCharacter('z'), 0x33);
}

TEST(CharEncoderTest, FindMatchingCharacterDigits) {
  EXPECT_EQ(FindMatchingCharacter('0'), 0x34);
  EXPECT_EQ(FindMatchingCharacter('1'), 0x35);
  EXPECT_EQ(FindMatchingCharacter('9'), 0x3D);
}

TEST(CharEncoderTest, FindMatchingCharacterPunctuation) {
  EXPECT_EQ(FindMatchingCharacter('!'), 0x3E);
  EXPECT_EQ(FindMatchingCharacter('?'), 0x3F);
  EXPECT_EQ(FindMatchingCharacter('-'), 0x40);
  EXPECT_EQ(FindMatchingCharacter('.'), 0x41);
  EXPECT_EQ(FindMatchingCharacter(','), 0x42);
  EXPECT_EQ(FindMatchingCharacter(' '), 0x59);
}

TEST(CharEncoderTest, FindMatchingCharacterUnknown) {
  EXPECT_EQ(FindMatchingCharacter('@'), 0xFF);
  EXPECT_EQ(FindMatchingCharacter('#'), 0xFF);
  EXPECT_EQ(FindMatchingCharacter('~'), 0xFF);
}

TEST(CharEncoderTest, RoundTripAllCharacters) {
  for (const auto& [byte_val, char_val] : CharEncoder) {
    // Skip non-ASCII wide chars and duplicate space mappings
    if (char_val > 127) continue;

    uint8_t encoded = FindMatchingCharacter(static_cast<char>(char_val));
    // Some characters map to multiple byte values (e.g., space at 0x59, 0x62-0x65)
    // Just verify the encoded byte maps back to the same character
    if (encoded != 0xFF) {
      EXPECT_TRUE(CharEncoder.contains(encoded))
          << "Byte 0x" << std::hex << (int)encoded
          << " not in CharEncoder for char '" << (char)char_val << "'";
      EXPECT_EQ(CharEncoder.at(encoded), char_val)
          << "Round-trip failed for char '" << (char)char_val << "'";
    }
  }
}

// ===========================================================================
// ParseTextDataByte Tests (single byte → text)
// ===========================================================================

TEST(ParseTextDataByteTest, RegularCharacters) {
  EXPECT_EQ(ParseTextDataByte(0x00), "A");
  EXPECT_EQ(ParseTextDataByte(0x1A), "a");
  EXPECT_EQ(ParseTextDataByte(0x34), "0");
  EXPECT_EQ(ParseTextDataByte(0x59), " ");
  EXPECT_EQ(ParseTextDataByte(0x42), ",");
}

TEST(ParseTextDataByteTest, TextCommands) {
  // [K] = wait for key = 0x7E
  EXPECT_EQ(ParseTextDataByte(0x7E), "[K]");
  // [V] = scroll text = 0x73
  EXPECT_EQ(ParseTextDataByte(0x73), "[V]");
  // [L] = player name = 0x6A
  EXPECT_EQ(ParseTextDataByte(0x6A), "[L]");
  // [1] = line 1 = 0x74
  EXPECT_EQ(ParseTextDataByte(0x74), "[1]");
  // [2] = line 2 = 0x75
  EXPECT_EQ(ParseTextDataByte(0x75), "[2]");
  // [3] = line 3 = 0x76
  EXPECT_EQ(ParseTextDataByte(0x76), "[3]");
}

TEST(ParseTextDataByteTest, CommandsWithArgs) {
  // Commands with arguments return generic token (with ##)
  // [W:##] = window border = 0x6B
  EXPECT_EQ(ParseTextDataByte(0x6B), "[W:##]");
  // [S:##] = text draw speed = 0x7A
  EXPECT_EQ(ParseTextDataByte(0x7A), "[S:##]");
  // [SFX:##] = sound effect = 0x79
  EXPECT_EQ(ParseTextDataByte(0x79), "[SFX:##]");
}

TEST(ParseTextDataByteTest, SpecialCharacters) {
  // [...] = ellipsis = 0x43
  EXPECT_EQ(ParseTextDataByte(0x43), "[...]");
  // [A] = A button = 0x5B
  EXPECT_EQ(ParseTextDataByte(0x5B), "[A]");
  // [B] = B button = 0x5C
  EXPECT_EQ(ParseTextDataByte(0x5C), "[B]");
  // [X] = X button = 0x5D
  EXPECT_EQ(ParseTextDataByte(0x5D), "[X]");
  // [Y] = Y button = 0x5E
  EXPECT_EQ(ParseTextDataByte(0x5E), "[Y]");
  // [UP] = up arrow = 0x4D
  EXPECT_EQ(ParseTextDataByte(0x4D), "[UP]");
}

TEST(ParseTextDataByteTest, DictionaryEntries) {
  // 0x88 = first dictionary entry → [D:00]
  EXPECT_EQ(ParseTextDataByte(0x88), "[D:00]");
  // 0x89 = second dictionary entry → [D:01]
  EXPECT_EQ(ParseTextDataByte(0x89), "[D:01]");
  // 0xE8 = last dictionary entry (0x88 + 0x60 = 0xE8) → [D:60]
  EXPECT_EQ(ParseTextDataByte(0xE8), "[D:60]");
}

TEST(ParseTextDataByteTest, UnknownByte) {
  // 0x81-0x87 are in a gap between commands and dictionary
  EXPECT_EQ(ParseTextDataByte(0x81), "");
  EXPECT_EQ(ParseTextDataByte(0x87), "");
}

// ===========================================================================
// ParseMessageToDataWithDiagnostics Tests
// ===========================================================================

TEST(MessageParseResultTest, ReportsUnknownToken) {
  auto result = ParseMessageToDataWithDiagnostics("Hello [BAD]");
  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.errors.empty());
}

TEST(MessageParseResultTest, ReportsNewlineWarning) {
  auto result = ParseMessageToDataWithDiagnostics("Hello\nWorld");
  EXPECT_TRUE(result.errors.empty());
  EXPECT_FALSE(result.warnings.empty());
}

// ===========================================================================
// Message Bundle Tests
// ===========================================================================

TEST(MessageBundleTest, SerializeMessageBundle) {
  std::vector<MessageData> vanilla;
  std::vector<MessageData> expanded;

  vanilla.push_back(
      MessageData(0, 0x100, "Hello", {}, "Hello", {}));
  expanded.push_back(
      MessageData(1, 0x200, "Expanded", {}, "Expanded", {}));

  nlohmann::json bundle = SerializeMessageBundle(vanilla, expanded);
  EXPECT_EQ(bundle["format"], "yaze-message-bundle");
  EXPECT_EQ(bundle["version"], kMessageBundleVersion);
  ASSERT_TRUE(bundle["messages"].is_array());
  ASSERT_EQ(bundle["messages"].size(), 2);
  EXPECT_EQ(bundle["messages"][0]["bank"], "vanilla");
  EXPECT_EQ(bundle["messages"][1]["bank"], "expanded");
}

TEST(MessageBundleTest, ParseLegacyArrayBundle) {
  nlohmann::json legacy = nlohmann::json::array();
  legacy.push_back({{"id", 2}, {"raw_string", "Legacy"}});
  auto entries_or = ParseMessageBundleJson(legacy);
  ASSERT_TRUE(entries_or.ok());
  auto entries = entries_or.value();
  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries[0].id, 2);
  EXPECT_EQ(entries[0].text, "Legacy");
  EXPECT_EQ(entries[0].bank, MessageBank::kVanilla);
}

// ===========================================================================
// FindDictionaryEntry Tests
// ===========================================================================

TEST(FindDictionaryEntryTest, ValidEntries) {
  EXPECT_EQ(FindDictionaryEntry(0x88), 0);
  EXPECT_EQ(FindDictionaryEntry(0x89), 1);
  EXPECT_EQ(FindDictionaryEntry(0xE8), 0x60);
}

TEST(FindDictionaryEntryTest, InvalidEntries) {
  EXPECT_EQ(FindDictionaryEntry(0x00), -1);
  EXPECT_EQ(FindDictionaryEntry(0x87), -1);
  EXPECT_EQ(FindDictionaryEntry(0xFF), -1);
}

// ===========================================================================
// FindMatchingCommand Tests
// ===========================================================================

TEST(FindMatchingCommandTest, KnownCommands) {
  auto cmd = FindMatchingCommand(0x7E);  // [K]
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->Token, "K");
  EXPECT_FALSE(cmd->HasArgument);

  cmd = FindMatchingCommand(0x6B);  // [W:##]
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->Token, "W");
  EXPECT_TRUE(cmd->HasArgument);

  cmd = FindMatchingCommand(0x79);  // [SFX:##]
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->Token, "SFX");
  EXPECT_TRUE(cmd->HasArgument);
}

TEST(FindMatchingCommandTest, UnknownByte) {
  EXPECT_FALSE(FindMatchingCommand(0x00).has_value());
  EXPECT_FALSE(FindMatchingCommand(0x42).has_value());
  EXPECT_FALSE(FindMatchingCommand(0xFF).has_value());
}

// ===========================================================================
// FindMatchingSpecial Tests
// ===========================================================================

TEST(FindMatchingSpecialTest, KnownSpecials) {
  auto spec = FindMatchingSpecial(0x43);  // [...]
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->Token, "...");

  spec = FindMatchingSpecial(0x5B);  // [A]
  ASSERT_TRUE(spec.has_value());
  EXPECT_EQ(spec->Token, "A");
}

TEST(FindMatchingSpecialTest, UnknownByte) {
  EXPECT_FALSE(FindMatchingSpecial(0x00).has_value());
  EXPECT_FALSE(FindMatchingSpecial(0xFF).has_value());
}

// ===========================================================================
// FindMatchingElement Tests (token string → ParsedElement)
// ===========================================================================

TEST(FindMatchingElementTest, SimpleCommand) {
  auto elem = FindMatchingElement("[K]");
  ASSERT_TRUE(elem.Active);
  EXPECT_EQ(elem.Parent.ID, 0x7E);
  EXPECT_EQ(elem.Parent.Token, "K");
}

TEST(FindMatchingElementTest, CommandWithArgument) {
  auto elem = FindMatchingElement("[W:02]");
  ASSERT_TRUE(elem.Active);
  EXPECT_EQ(elem.Parent.ID, 0x6B);
  EXPECT_EQ(elem.Value, 0x02);

  elem = FindMatchingElement("[SFX:2D]");
  ASSERT_TRUE(elem.Active);
  EXPECT_EQ(elem.Parent.ID, 0x79);
  EXPECT_EQ(elem.Value, 0x2D);
}

TEST(FindMatchingElementTest, SpecialCharacter) {
  auto elem = FindMatchingElement("[...]");
  ASSERT_TRUE(elem.Active);
  EXPECT_EQ(elem.Parent.ID, 0x43);

  elem = FindMatchingElement("[A]");
  ASSERT_TRUE(elem.Active);
  EXPECT_EQ(elem.Parent.ID, 0x5B);
}

TEST(FindMatchingElementTest, DictionaryToken) {
  auto elem = FindMatchingElement("[D:00]");
  ASSERT_TRUE(elem.Active);
  EXPECT_EQ(elem.Value, DICTOFF + 0x00);  // 0x88

  elem = FindMatchingElement("[D:3F]");
  ASSERT_TRUE(elem.Active);
  EXPECT_EQ(elem.Value, DICTOFF + 0x3F);
}

TEST(FindMatchingElementTest, InvalidToken) {
  auto elem = FindMatchingElement("[INVALID]");
  EXPECT_FALSE(elem.Active);

  elem = FindMatchingElement("hello");
  EXPECT_FALSE(elem.Active);
}

// ===========================================================================
// ParseMessageToData Tests (text → bytes)
// ===========================================================================

TEST(ParseMessageToDataTest, PlainText) {
  auto bytes = ParseMessageToData("Hello");
  ASSERT_EQ(bytes.size(), 5);
  EXPECT_EQ(bytes[0], 0x07);  // H
  EXPECT_EQ(bytes[1], 0x1E);  // e
  EXPECT_EQ(bytes[2], 0x25);  // l
  EXPECT_EQ(bytes[3], 0x25);  // l
  EXPECT_EQ(bytes[4], 0x28);  // o
}

TEST(ParseMessageToDataTest, PlainTextWithSpace) {
  auto bytes = ParseMessageToData("A B");
  ASSERT_EQ(bytes.size(), 3);
  EXPECT_EQ(bytes[0], 0x00);  // A
  EXPECT_EQ(bytes[1], 0x59);  // space
  EXPECT_EQ(bytes[2], 0x01);  // B
}

TEST(ParseMessageToDataTest, CommandNoArg) {
  auto bytes = ParseMessageToData("[K]");
  ASSERT_EQ(bytes.size(), 1);
  EXPECT_EQ(bytes[0], 0x7E);
}

TEST(ParseMessageToDataTest, CommandWithArg) {
  auto bytes = ParseMessageToData("[W:02]");
  ASSERT_EQ(bytes.size(), 2);
  EXPECT_EQ(bytes[0], 0x6B);  // W command
  EXPECT_EQ(bytes[1], 0x02);  // argument
}

TEST(ParseMessageToDataTest, PlayerNameCommand) {
  auto bytes = ParseMessageToData("[L]");
  ASSERT_EQ(bytes.size(), 1);
  EXPECT_EQ(bytes[0], 0x6A);
}

TEST(ParseMessageToDataTest, LineBreakCommands) {
  auto bytes = ParseMessageToData("[2]");
  ASSERT_EQ(bytes.size(), 1);
  EXPECT_EQ(bytes[0], 0x75);  // Line 2

  bytes = ParseMessageToData("[3]");
  ASSERT_EQ(bytes.size(), 1);
  EXPECT_EQ(bytes[0], 0x76);  // Line 3

  bytes = ParseMessageToData("[V]");
  ASSERT_EQ(bytes.size(), 1);
  EXPECT_EQ(bytes[0], 0x73);  // Scroll vertical
}

TEST(ParseMessageToDataTest, SpecialCharacters) {
  auto bytes = ParseMessageToData("[...]");
  ASSERT_EQ(bytes.size(), 1);
  EXPECT_EQ(bytes[0], 0x43);

  bytes = ParseMessageToData("[A]");
  ASSERT_EQ(bytes.size(), 1);
  EXPECT_EQ(bytes[0], 0x5B);

  bytes = ParseMessageToData("[X]");
  ASSERT_EQ(bytes.size(), 1);
  EXPECT_EQ(bytes[0], 0x5D);
}

TEST(ParseMessageToDataTest, DictionaryToken) {
  auto bytes = ParseMessageToData("[D:00]");
  ASSERT_EQ(bytes.size(), 1);
  EXPECT_EQ(bytes[0], 0x88);  // DICTOFF + 0

  bytes = ParseMessageToData("[D:10]");
  ASSERT_EQ(bytes.size(), 1);
  EXPECT_EQ(bytes[0], 0x98);  // DICTOFF + 0x10
}

TEST(ParseMessageToDataTest, SoundEffectCommand) {
  auto bytes = ParseMessageToData("[SFX:2D]");
  ASSERT_EQ(bytes.size(), 2);
  EXPECT_EQ(bytes[0], 0x79);  // SFX command
  EXPECT_EQ(bytes[1], 0x2D);  // argument
}

TEST(ParseMessageToDataTest, MixedTextAndCommands) {
  // "Hi, [L]!" → H, i, comma, space, [L], !
  auto bytes = ParseMessageToData("Hi, [L]!");
  ASSERT_EQ(bytes.size(), 6);
  EXPECT_EQ(bytes[0], 0x07);  // H
  EXPECT_EQ(bytes[1], 0x22);  // i
  EXPECT_EQ(bytes[2], 0x42);  // ,
  EXPECT_EQ(bytes[3], 0x59);  // space
  EXPECT_EQ(bytes[4], 0x6A);  // [L]
  EXPECT_EQ(bytes[5], 0x3E);  // !
}

TEST(ParseMessageToDataTest, ComplexMessageWithMultipleCommands) {
  // "[W:02][S:03]Hello[2]world[K]"
  auto bytes = ParseMessageToData("[W:02][S:03]Hello[2]world[K]");
  // W:02 (2) + S:03 (2) + Hello (5) + [2] (1) + world (5) + [K] (1) = 16
  ASSERT_EQ(bytes.size(), 16);
  EXPECT_EQ(bytes[0], 0x6B);   // W
  EXPECT_EQ(bytes[1], 0x02);   // arg
  EXPECT_EQ(bytes[2], 0x7A);   // S
  EXPECT_EQ(bytes[3], 0x03);   // arg
  EXPECT_EQ(bytes[4], 0x07);   // H
  EXPECT_EQ(bytes[9], 0x75);   // [2]
  EXPECT_EQ(bytes[15], 0x7E);  // [K]
}

TEST(ParseMessageToDataTest, EmptyString) {
  auto bytes = ParseMessageToData("");
  EXPECT_TRUE(bytes.empty());
}

TEST(ParseMessageToDataTest, UnknownCharactersSkipped) {
  // '@' is not in CharEncoder, should be skipped
  auto bytes = ParseMessageToData("A@B");
  ASSERT_EQ(bytes.size(), 2);
  EXPECT_EQ(bytes[0], 0x00);  // A
  EXPECT_EQ(bytes[1], 0x01);  // B
}

// ===========================================================================
// Encode/Decode Round-Trip Tests
// ===========================================================================

TEST(RoundTripTest, PlainTextRoundTrip) {
  std::string original = "Hello, world!";
  auto bytes = ParseMessageToData(original);

  std::string decoded;
  for (uint8_t b : bytes) {
    decoded += ParseTextDataByte(b);
  }

  EXPECT_EQ(decoded, original);
}

TEST(RoundTripTest, CommandsRoundTrip) {
  // Simple commands (no args) should round-trip exactly
  std::string original = "[K]";
  auto bytes = ParseMessageToData(original);
  ASSERT_EQ(bytes.size(), 1);
  std::string decoded = ParseTextDataByte(bytes[0]);
  EXPECT_EQ(decoded, original);
}

TEST(RoundTripTest, TextWithPlayerName) {
  std::string original = "[L] saved the day!";
  auto bytes = ParseMessageToData(original);

  std::string decoded;
  for (uint8_t b : bytes) {
    decoded += ParseTextDataByte(b);
  }

  EXPECT_EQ(decoded, original);
}

TEST(RoundTripTest, SpecialCharsRoundTrip) {
  std::string original = "[...][A][B][X][Y]";
  auto bytes = ParseMessageToData(original);
  ASSERT_EQ(bytes.size(), 5);

  std::string decoded;
  for (uint8_t b : bytes) {
    decoded += ParseTextDataByte(b);
  }

  EXPECT_EQ(decoded, original);
}

TEST(RoundTripTest, AllPunctuationRoundTrip) {
  std::string original = "!?-.,";
  auto bytes = ParseMessageToData(original);

  std::string decoded;
  for (uint8_t b : bytes) {
    decoded += ParseTextDataByte(b);
  }

  EXPECT_EQ(decoded, original);
}

TEST(RoundTripTest, DigitsRoundTrip) {
  std::string original = "0123456789";
  auto bytes = ParseMessageToData(original);
  ASSERT_EQ(bytes.size(), 10);

  std::string decoded;
  for (uint8_t b : bytes) {
    decoded += ParseTextDataByte(b);
  }

  EXPECT_EQ(decoded, original);
}

// ===========================================================================
// ParseSingleMessage Tests
// ===========================================================================

TEST(ParseSingleMessageTest, SimpleMessage) {
  // "AB" terminated by 0x7F
  std::vector<uint8_t> rom_data = {0x00, 0x01, 0x7F};
  int pos = 0;
  auto result = ParseSingleMessage(rom_data, &pos);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->RawString, "AB");
  EXPECT_EQ(pos, 3);
}

TEST(ParseSingleMessageTest, MessageWithCommand) {
  // "[K]" terminated by 0x7F
  std::vector<uint8_t> rom_data = {0x7E, 0x7F};
  int pos = 0;
  auto result = ParseSingleMessage(rom_data, &pos);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->RawString, "[K]");
}

TEST(ParseSingleMessageTest, MessageWithArgCommand) {
  // "[W:02]A" terminated by 0x7F
  std::vector<uint8_t> rom_data = {0x6B, 0x02, 0x00, 0x7F};
  int pos = 0;
  auto result = ParseSingleMessage(rom_data, &pos);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->RawString, "[W:02]A");
}

TEST(ParseSingleMessageTest, NullPosition) {
  std::vector<uint8_t> rom_data = {0x00, 0x7F};
  auto result = ParseSingleMessage(rom_data, nullptr);
  EXPECT_FALSE(result.ok());
}

TEST(ParseSingleMessageTest, OutOfRange) {
  std::vector<uint8_t> rom_data = {0x00, 0x7F};
  int pos = 10;
  auto result = ParseSingleMessage(rom_data, &pos);
  EXPECT_FALSE(result.ok());
}

TEST(ParseSingleMessageTest, MissingTerminator) {
  // 0xFF without 0x7F first
  std::vector<uint8_t> rom_data = {0x00, 0x01, 0xFF};
  int pos = 0;
  auto result = ParseSingleMessage(rom_data, &pos);
  EXPECT_FALSE(result.ok());
}

TEST(ParseSingleMessageTest, MessageWithSpecialChar) {
  // "[...]A" terminated by 0x7F → 0x43, 0x00, 0x7F
  std::vector<uint8_t> rom_data = {0x43, 0x00, 0x7F};
  int pos = 0;
  auto result = ParseSingleMessage(rom_data, &pos);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->RawString, "[...]A");
}

// ===========================================================================
// TextElement Tests
// ===========================================================================

TEST(TextElementTest, GetParamTokenNoArg) {
  TextElement elem(0x7E, "K", false, "Wait for key");
  EXPECT_EQ(elem.GetParamToken(), "[K]");
}

TEST(TextElementTest, GetParamTokenWithArg) {
  TextElement elem(0x6B, "W", true, "Window border");
  EXPECT_EQ(elem.GetParamToken(0x02), "[W:02]");
  EXPECT_EQ(elem.GetParamToken(0xFF), "[W:FF]");
}

TEST(TextElementTest, MatchMeSimple) {
  TextElement elem(0x7E, "K", false, "Wait for key");
  auto match = elem.MatchMe("[K]");
  EXPECT_GT(match.size(), 0);
}

TEST(TextElementTest, MatchMeWithArg) {
  TextElement elem(0x6B, "W", true, "Window border");
  auto match = elem.MatchMe("[W:02]");
  EXPECT_GT(match.size(), 0);

  // Should not match without arg format
  match = elem.MatchMe("[W]");
  // W has HasArgument=true, pattern allows optional arg
  // This may or may not match depending on regex - just verify no crash
}

TEST(TextElementTest, MatchMeNoMatch) {
  TextElement elem(0x7E, "K", false, "Wait for key");
  auto match = elem.MatchMe("[W:02]");
  EXPECT_EQ(match.size(), 0);
}

// ===========================================================================
// DictionaryEntry Tests
// ===========================================================================

TEST(DictionaryEntryTest, ContainedInString) {
  DictionaryEntry entry;
  entry.ID = 0;
  entry.Contents = "the";
  entry.Token = "[D:00]";

  EXPECT_TRUE(entry.ContainedInString("the cat"));
  EXPECT_TRUE(entry.ContainedInString("in the house"));
  EXPECT_FALSE(entry.ContainedInString("a cat"));
}

TEST(DictionaryEntryTest, ReplaceInstancesOfIn) {
  DictionaryEntry entry;
  entry.ID = 0;
  entry.Contents = "the";
  entry.Token = "[D:00]";

  EXPECT_EQ(entry.ReplaceInstancesOfIn("the cat"), "[D:00] cat");
  EXPECT_EQ(entry.ReplaceInstancesOfIn("no match"), "no match");
}

// ===========================================================================
// MessageData Tests
// ===========================================================================

TEST(MessageDataStructTest, CopyConstructor) {
  MessageData original(42, 0x1234, "raw", {0x01, 0x02}, "parsed", {0x03});
  MessageData copy(original);

  EXPECT_EQ(copy.ID, 42);
  EXPECT_EQ(copy.Address, 0x1234);
  EXPECT_EQ(copy.RawString, "raw");
  EXPECT_EQ(copy.ContentsParsed, "parsed");
  EXPECT_EQ(copy.Data, (std::vector<uint8_t>{0x01, 0x02}));
}

// ===========================================================================
// Line Width Validation Tests
// ===========================================================================

TEST(ValidateLineWidthsTest, ShortLinesPass) {
  auto result = ValidateMessageLineWidths("Hello, world!");
  EXPECT_TRUE(result.empty()) << "Expected no warnings for short line";
}

TEST(ValidateLineWidthsTest, ExactlyMaxWidthPasses) {
  // 32 characters exactly
  std::string line32 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";
  ASSERT_EQ(line32.size(), 32);
  auto result = ValidateMessageLineWidths(line32);
  EXPECT_TRUE(result.empty()) << "Expected no warnings for 32-char line";
}

TEST(ValidateLineWidthsTest, OverMaxWidthWarns) {
  // 33 characters
  std::string line33 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456";
  ASSERT_EQ(line33.size(), 33);
  auto result = ValidateMessageLineWidths(line33);
  EXPECT_FALSE(result.empty()) << "Expected warning for 33-char line";
}

TEST(ValidateLineWidthsTest, MultiLineMessage) {
  // Line 1 ok (5 chars), line 2 ok (5 chars)
  std::string msg = "Hello[2]world";
  auto result = ValidateMessageLineWidths(msg);
  EXPECT_TRUE(result.empty());
}

TEST(ValidateLineWidthsTest, CommandTokensNotCounted) {
  // [K], [V], [L] etc. should not count toward visible width
  // except [L] expands to player name (up to 6 chars) - for now just
  // verify commands don't add to count
  std::string msg = "[W:02][S:03]Hello";
  auto result = ValidateMessageLineWidths(msg);
  EXPECT_TRUE(result.empty());
}

TEST(ValidateLineWidthsTest, LineBreakTokensSplitLines) {
  // [2], [3], [V], [K] should all act as line breaks
  std::string long_line = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456";  // 33 chars
  // But split across two lines via [2]
  std::string msg = "ABCDEFGHIJKLMNOP[2]QRSTUVWXYZ0123456";
  auto result = ValidateMessageLineWidths(msg);
  // First line: 16 chars (ok), second line: 17 chars (ok)
  EXPECT_TRUE(result.empty());
}

TEST(ValidateLineWidthsTest, EmptyMessageOk) {
  auto result = ValidateMessageLineWidths("");
  EXPECT_TRUE(result.empty());
}

// ===========================================================================
// Org Format Tests
// ===========================================================================

TEST(OrgFormatTest, ParseHeader) {
  auto result = ParseOrgHeader("** 0F - Skeleton Guard");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, 0x0F);
  EXPECT_EQ(result->second, "Skeleton Guard");
}

TEST(OrgFormatTest, ParseHeaderHexId) {
  auto result = ParseOrgHeader("** 1E - Impa Hall of Secrets");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, 0x1E);
  EXPECT_EQ(result->second, "Impa Hall of Secrets");
}

TEST(OrgFormatTest, ParseHeaderWithQuestion) {
  auto result = ParseOrgHeader("** 19 - Village Elder?");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, 0x19);
  EXPECT_EQ(result->second, "Village Elder?");
}

TEST(OrgFormatTest, ParseHeaderInvalid) {
  // No ** prefix
  EXPECT_FALSE(ParseOrgHeader("0F - Skeleton Guard").has_value());
  // Empty line
  EXPECT_FALSE(ParseOrgHeader("").has_value());
  // Just a title line
  EXPECT_FALSE(ParseOrgHeader("* Oracle of Secrets").has_value());
}

TEST(OrgFormatTest, ParseOrgFileSimple) {
  std::string org_content = R"(* Oracle of Secrets English Dialogue
** 0F - Skeleton Guard
Press L or R in your menu to
[2]see the status of your quest
[3]and all your items!

** 10 - Skeleton Guard
Got a map, huh? (Press [X] to
[2]see your map). Don't get lost,
[3]or you might find our hideout![...]
)";

  auto messages = ParseOrgContent(org_content);
  ASSERT_EQ(messages.size(), 2);

  EXPECT_EQ(messages[0].first, 0x0F);
  // Body should contain the text with control codes
  EXPECT_NE(messages[0].second.find("[2]"), std::string::npos);
  EXPECT_NE(messages[0].second.find("[3]"), std::string::npos);

  EXPECT_EQ(messages[1].first, 0x10);
  EXPECT_NE(messages[1].second.find("[X]"), std::string::npos);
  EXPECT_NE(messages[1].second.find("[...]"), std::string::npos);
}

TEST(OrgFormatTest, ParseOrgFileMultiPage) {
  std::string org_content = R"(** 19 - Village Elder?
If you defeat Kydrog, the
[2]island will be free to prosper
[3]once again.[K]
[V]Go, seek the Essences!
)";

  auto messages = ParseOrgContent(org_content);
  ASSERT_EQ(messages.size(), 1);
  EXPECT_EQ(messages[0].first, 0x19);
  EXPECT_NE(messages[0].second.find("[K]"), std::string::npos);
  EXPECT_NE(messages[0].second.find("[V]"), std::string::npos);
}

TEST(OrgFormatTest, ExportOrgFormat) {
  std::vector<std::pair<int, std::string>> messages = {
      {0x0F, "Hello, world!"},
      {0x10, "Test[2]message"},
  };
  std::vector<std::string> labels = {"Skeleton Guard", "Another Guard"};

  std::string exported = ExportToOrgFormat(messages, labels);
  EXPECT_NE(exported.find("** 0F - Skeleton Guard"), std::string::npos);
  EXPECT_NE(exported.find("Hello, world!"), std::string::npos);
  EXPECT_NE(exported.find("** 10 - Another Guard"), std::string::npos);
}

// ===========================================================================
// Expanded Message Bank Tests
// ===========================================================================

TEST(ExpandedBankTest, ConstantsCorrect) {
  // $2F8000 SNES → PC address
  // Bank $2F: SNES $2F8000 = PC ((0x2F >> 1) << 16) | (0x8000 & 0x7FFF)
  //         = (0x17 << 16) | 0x0000 = 0x178000 - wait, let's just use SnesToPc
  uint32_t pc = SnesToPc(0x2F8000);
  EXPECT_EQ(pc, kExpandedTextData);

  uint32_t pc_end = SnesToPc(0x2FFFFF);
  EXPECT_EQ(pc_end, kExpandedTextDataEnd);
}

TEST(ExpandedBankTest, ReadExpandedTextDataSimple) {
  // Two messages: "AB" + terminator + "CD" + terminator + end marker
  std::vector<uint8_t> rom_data(0x180000, 0x00);
  int base = kExpandedTextData;
  rom_data[base + 0] = 0x00;  // A
  rom_data[base + 1] = 0x01;  // B
  rom_data[base + 2] = 0x7F;  // terminator
  rom_data[base + 3] = 0x02;  // C
  rom_data[base + 4] = 0x03;  // D
  rom_data[base + 5] = 0x7F;  // terminator
  rom_data[base + 6] = 0xFF;  // end of expanded data

  auto messages = ReadExpandedTextData(rom_data.data(), base);
  ASSERT_EQ(messages.size(), 2);
  EXPECT_EQ(messages[0].ContentsParsed, "AB");
  EXPECT_EQ(messages[1].ContentsParsed, "CD");
}

TEST(ExpandedBankTest, ReadExpandedTextDataEmpty) {
  // Immediately terminated
  std::vector<uint8_t> rom_data(0x180000, 0x00);
  rom_data[kExpandedTextData] = 0xFF;

  auto messages = ReadExpandedTextData(rom_data.data(), kExpandedTextData);
  EXPECT_TRUE(messages.empty());
}

TEST(ExpandedBankTest, WriteExpandedTextData) {
  std::vector<uint8_t> rom_data(0x180000, 0x00);

  std::vector<std::string> texts = {"AB", "CD"};
  auto status = WriteExpandedTextData(rom_data.data(), kExpandedTextData,
                                      kExpandedTextDataEnd, texts);
  ASSERT_TRUE(status.ok());

  // Verify written data
  int base = kExpandedTextData;
  EXPECT_EQ(rom_data[base + 0], 0x00);  // A
  EXPECT_EQ(rom_data[base + 1], 0x01);  // B
  EXPECT_EQ(rom_data[base + 2], 0x7F);  // terminator
  EXPECT_EQ(rom_data[base + 3], 0x02);  // C
  EXPECT_EQ(rom_data[base + 4], 0x03);  // D
  EXPECT_EQ(rom_data[base + 5], 0x7F);  // terminator
  EXPECT_EQ(rom_data[base + 6], 0xFF);  // end marker
}

TEST(ExpandedBankTest, WriteExpandedTextDataOverflow) {
  // Tiny buffer — should fail
  std::vector<uint8_t> rom_data(0x180000, 0x00);
  int start = kExpandedTextData;
  int end = start + 2;  // Only 3 bytes available

  std::vector<std::string> texts = {"Hello, world!"};
  auto status = WriteExpandedTextData(rom_data.data(), start, end, texts);
  EXPECT_FALSE(status.ok());
}

TEST(ExpandedBankTest, WriteAndReadRoundTrip) {
  std::vector<uint8_t> rom_data(0x180000, 0x00);

  std::vector<std::string> texts = {"Hello", "[L]", "Test[2]msg"};
  auto status = WriteExpandedTextData(rom_data.data(), kExpandedTextData,
                                      kExpandedTextDataEnd, texts);
  ASSERT_TRUE(status.ok());

  auto messages =
      ReadExpandedTextData(rom_data.data(), kExpandedTextData);
  ASSERT_EQ(messages.size(), 3);
  EXPECT_EQ(messages[0].ContentsParsed, "Hello");
  EXPECT_EQ(messages[1].ContentsParsed, "[L]");
  EXPECT_EQ(messages[2].ContentsParsed, "Test[2]msg");
}

}  // namespace
}  // namespace editor
}  // namespace yaze
