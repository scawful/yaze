#ifndef YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H
#define YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H

#include <regex>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

const uint8_t kMessageTerminator = 0x7F;
const std::string BANKToken = "BANK";
const std::string DICTIONARYTOKEN = "D";
constexpr uint8_t DICTOFF = 0x88;

static const std::unordered_map<uint8_t, wchar_t> CharEncoder = {
    {0x00, 'A'},  {0x01, 'B'},  {0x02, 'C'},  {0x03, 'D'},  {0x04, 'E'},
    {0x05, 'F'},  {0x06, 'G'},  {0x07, 'H'},  {0x08, 'I'},  {0x09, 'J'},
    {0x0A, 'K'},  {0x0B, 'L'},  {0x0C, 'M'},  {0x0D, 'N'},  {0x0E, 'O'},
    {0x0F, 'P'},  {0x10, 'Q'},  {0x11, 'R'},  {0x12, 'S'},  {0x13, 'T'},
    {0x14, 'U'},  {0x15, 'V'},  {0x16, 'W'},  {0x17, 'X'},  {0x18, 'Y'},
    {0x19, 'Z'},  {0x1A, 'a'},  {0x1B, 'b'},  {0x1C, 'c'},  {0x1D, 'd'},
    {0x1E, 'e'},  {0x1F, 'f'},  {0x20, 'g'},  {0x21, 'h'},  {0x22, 'i'},
    {0x23, 'j'},  {0x24, 'k'},  {0x25, 'l'},  {0x26, 'm'},  {0x27, 'n'},
    {0x28, 'o'},  {0x29, 'p'},  {0x2A, 'q'},  {0x2B, 'r'},  {0x2C, 's'},
    {0x2D, 't'},  {0x2E, 'u'},  {0x2F, 'v'},  {0x30, 'w'},  {0x31, 'x'},
    {0x32, 'y'},  {0x33, 'z'},  {0x34, '0'},  {0x35, '1'},  {0x36, '2'},
    {0x37, '3'},  {0x38, '4'},  {0x39, '5'},  {0x3A, '6'},  {0x3B, '7'},
    {0x3C, '8'},  {0x3D, '9'},  {0x3E, '!'},  {0x3F, '?'},  {0x40, '-'},
    {0x41, '.'},  {0x42, ','},  {0x44, '>'},  {0x45, '('},  {0x46, ')'},
    {0x4C, '"'},  {0x51, '\''}, {0x59, ' '},  {0x5A, '<'},  {0x5F, L'¡'},
    {0x60, L'¡'}, {0x61, L'¡'}, {0x62, L' '}, {0x63, L' '}, {0x64, L' '},
    {0x65, ' '},  {0x66, '_'},
};

uint8_t FindMatchingCharacter(char value);
uint8_t FindDictionaryEntry(uint8_t value);

std::vector<uint8_t> ParseMessageToData(std::string str);

struct DictionaryEntry {
  uint8_t ID;
  std::string Contents;
  std::vector<uint8_t> Data;
  int Length;
  std::string Token;

  DictionaryEntry() = default;
  DictionaryEntry(uint8_t i, std::string s)
      : Contents(s), ID(i), Length(s.length()) {
    Token = absl::StrFormat("[%s:%00X]", DICTIONARYTOKEN, ID);
    Data = ParseMessageToData(Contents);
  }

  bool ContainedInString(std::string s) {
    return s.find(Contents) != std::string::npos;
  }

  std::string ReplaceInstancesOfIn(std::string s) {
    std::string replacedString = s;
    size_t pos = replacedString.find(Contents);
    while (pos != std::string::npos) {
      replacedString.replace(pos, Contents.length(), Token);
      pos = replacedString.find(Contents, pos + Token.length());
    }
    return replacedString;
  }
};

constexpr int kTextData = 0xE0000;
constexpr int kTextDataEnd = 0xE7FFF;
constexpr int kNumDictionaryEntries = 97;
constexpr int kPointersDictionaries = 0x74703;

std::vector<DictionaryEntry> BuildDictionaryEntries(app::Rom* rom);

std::string ReplaceAllDictionaryWords(std::string str,
                                      std::vector<DictionaryEntry> dictionary);

// Inserted into commands to protect them from dictionary replacements.
const std::string CHEESE = "\uBEBE";

struct MessageData {
  int ID;
  int Address;
  std::string RawString;
  std::string ContentsParsed;
  std::vector<uint8_t> Data;
  std::vector<uint8_t> DataParsed;

  MessageData() = default;
  MessageData(int id, int address, const std::string& rawString,
              const std::vector<uint8_t>& rawData,
              const std::string& parsedString,
              const std::vector<uint8_t>& parsedData)
      : ID(id),
        Address(address),
        RawString(rawString),
        Data(rawData),
        DataParsed(parsedData),
        ContentsParsed(parsedString) {}

  // Copy constructor
  MessageData(const MessageData& other) {
    ID = other.ID;
    Address = other.Address;
    RawString = other.RawString;
    Data = other.Data;
    DataParsed = other.DataParsed;
    ContentsParsed = other.ContentsParsed;
  }

  std::string ToString() {
    return absl::StrFormat("%0X - %s", ID, ContentsParsed);
  }

  std::string OptimizeMessageForDictionary(
      std::string messageString,
      const std::vector<DictionaryEntry>& dictionary) {
    std::stringstream protons;
    bool command = false;
    for (const auto& c : messageString) {
      if (c == '[') {
        command = true;
      } else if (c == ']') {
        command = false;
      }

      protons << c;
      if (command) {
        protons << CHEESE;
      }
    }

    std::string protonsString = protons.str();
    std::string replacedString =
        ReplaceAllDictionaryWords(protonsString, dictionary);
    std::string finalString =
        absl::StrReplaceAll(replacedString, {{CHEESE, ""}});

    return finalString;
  }

  void SetMessage(const std::string& message,
                  const std::vector<DictionaryEntry>& dictionary) {
    RawString = message;
    ContentsParsed = OptimizeMessageForDictionary(message, dictionary);
  }
};

struct TextElement {
  uint8_t ID;
  std::string Token;
  std::string GenericToken;
  std::string Pattern;
  std::string StrictPattern;
  std::string Description;
  bool HasArgument;

  TextElement() = default;
  TextElement(uint8_t id, std::string token, bool arg,
              std::string description) {
    ID = id;
    Token = token;
    if (arg) {
      GenericToken = absl::StrFormat("[%s:##]", Token);
    } else {
      GenericToken = absl::StrFormat("[%s]", Token);
    }
    HasArgument = arg;
    Description = description;
    Pattern =
        arg ? "\\[" + Token + ":?([0-9A-F]{1,2})\\]" : "\\[" + Token + "\\]";
    Pattern = absl::StrReplaceAll(Pattern, {{"[", "\\["}, {"]", "\\]"}});
    StrictPattern = absl::StrCat("^", Pattern, "$");
    StrictPattern = "^" + Pattern + "$";
  }

  std::string GetParameterizedToken(uint8_t value = 0) {
    if (HasArgument) {
      return absl::StrFormat("[%s:%02X]", Token, value);
    } else {
      return absl::StrFormat("[%s]", Token);
    }
  }

  std::string ToString() {
    return absl::StrFormat("%s %s", GenericToken, Description);
  }

  std::smatch MatchMe(std::string dfrag) const {
    std::regex pattern(StrictPattern);
    std::smatch match;
    std::regex_match(dfrag, match, pattern);
    return match;
  }

  bool Empty() { return ID == 0; }

  // Comparison operator
  bool operator==(const TextElement& other) const { return ID == other.ID; }
};

static const std::vector<TextElement> TextCommands = {
    TextElement(0x6B, "W", true, "Window border"),
    TextElement(0x6D, "P", true, "Window position"),
    TextElement(0x6E, "SPD", true, "Scroll speed"),
    TextElement(0x7A, "S", true, "Text draw speed"),
    TextElement(0x77, "C", true, "Text color"),
    TextElement(0x6A, "L", false, "Player name"),
    TextElement(0x74, "1", false, "Line 1"),
    TextElement(0x75, "2", false, "Line 2"),
    TextElement(0x76, "3", false, "Line 3"),
    TextElement(0x7E, "K", false, "Wait for key"),
    TextElement(0x73, "V", false, "Scroll text"),
    TextElement(0x78, "WT", true, "Delay X"),
    TextElement(0x6C, "N", true, "BCD number"),
    TextElement(0x79, "SFX", true, "Sound effect"),
    TextElement(0x71, "CH3", false, "Choose 3"),
    TextElement(0x72, "CH2", false, "Choose 2 high"),
    TextElement(0x6F, "CH2L", false, "Choose 2 low"),
    TextElement(0x68, "CH2I", false, "Choose 2 indented"),
    TextElement(0x69, "CHI", false, "Choose item"),
    TextElement(0x67, "IMG", false, "Next attract image"),
    TextElement(0x80, BANKToken, false, "Bank marker (automatic)"),
    TextElement(0x70, "NONO", false, "Crash"),
};

TextElement FindMatchingCommand(uint8_t b);

static const std::vector<TextElement> SpecialChars = {
    TextElement(0x43, "...", false, "Ellipsis …"),
    TextElement(0x4D, "UP", false, "Arrow ↑"),
    TextElement(0x4E, "DOWN", false, "Arrow ↓"),
    TextElement(0x4F, "LEFT", false, "Arrow ←"),
    TextElement(0x50, "RIGHT", false, "Arrow →"),
    TextElement(0x5B, "A", false, "Button Ⓐ"),
    TextElement(0x5C, "B", false, "Button Ⓑ"),
    TextElement(0x5D, "X", false, "Button ⓧ"),
    TextElement(0x5E, "Y", false, "Button ⓨ"),
    TextElement(0x52, "HP1L", false, "1 HP left"),
    TextElement(0x53, "HP1R", false, "1 HP right"),
    TextElement(0x54, "HP2L", false, "2 HP left"),
    TextElement(0x55, "HP3L", false, "3 HP left"),
    TextElement(0x56, "HP3R", false, "3 HP right"),
    TextElement(0x57, "HP4L", false, "4 HP left"),
    TextElement(0x58, "HP4R", false, "4 HP right"),
    TextElement(0x47, "HY0", false, "Hieroglyph ☥"),
    TextElement(0x48, "HY1", false, "Hieroglyph 𓈗"),
    TextElement(0x49, "HY2", false, "Hieroglyph Ƨ"),
    TextElement(0x4A, "LFL", false, "Link face left"),
    TextElement(0x4B, "LFR", false, "Link face right"),
};

TextElement FindMatchingSpecial(uint8_t b);

struct ParsedElement {
  TextElement Parent;
  uint8_t Value;
  bool Active = false;

  ParsedElement() = default;
  ParsedElement(TextElement textElement, uint8_t value) {
    Parent = textElement;
    Value = value;
    Active = true;
  }
};

ParsedElement FindMatchingElement(const std::string& str);

std::string ParseTextDataByte(uint8_t value);

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H
