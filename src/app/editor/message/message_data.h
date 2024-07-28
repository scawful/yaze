#ifndef YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H
#define YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H

#include <string>
#include <vector>

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
}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H