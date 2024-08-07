#ifndef YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H
#define YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"

namespace yaze {
namespace app {
namespace editor {

const uint8_t MESSAGETERMINATOR = 0x7F;

static std::string AddNewLinesToCommands(std::string str);
static std::string ReplaceAllDictionaryWords(std::string str);
static std::vector<uint8_t> ParseMessageToData(std::string str);

const std::string CHEESE = "\uBEBE";  // Inserted into commands to protect
                                      // them from dictionary replacements.

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

  void SetMessage(std::string messageString) {
    ContentsParsed = messageString;
    RawString = OptimizeMessageForDictionary(messageString);
    RecalculateData();
  }

  std::string ToString() {
    return absl::StrFormat("%0X - %s", ID, ContentsParsed);
  }

  std::string GetReadableDumpedContents() {
    std::stringstream stringBuilder;
    for (const auto& b : Data) {
      stringBuilder << absl::StrFormat("%0X ", b);
    }
    stringBuilder << absl::StrFormat("%00X", MESSAGETERMINATOR);

    return absl::StrFormat(
        "[[[[\r\nMessage "
        "%000X]]]]\r\n[Contents]\r\n%s\r\n\r\n[Data]\r\n%s"
        "\r\n\r\n\r\n\r\n",
        ID, AddNewLinesToCommands(ContentsParsed), stringBuilder.str());
  }

  std::string GetDumpedContents() {
    return absl::StrFormat("%000X : %s\r\n\r\n", ID, ContentsParsed);
  }

  std::string OptimizeMessageForDictionary(std::string messageString) {
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
    std::string replacedString = ReplaceAllDictionaryWords(protonsString);
    std::string finalString =
        absl::StrReplaceAll(replacedString, {{CHEESE, ""}});

    return finalString;
  }

  void RecalculateData() {
    Data = ParseMessageToData(RawString);
    DataParsed = ParseMessageToData(ContentsParsed);
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
};

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

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H