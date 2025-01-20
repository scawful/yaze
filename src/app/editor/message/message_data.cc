#include "message_data.h"

#include <string>

#include "app/core/common.h"

namespace yaze {
namespace editor {

uint8_t FindMatchingCharacter(char value) {
  for (const auto [key, char_value] : CharEncoder) {
    if (value == char_value) {
      return key;
    }
  }
  return 0xFF;
}

uint8_t FindDictionaryEntry(uint8_t value) {
  if (value < DICTOFF || value == 0xFF) {
    return -1;
  }
  return value - DICTOFF;
}

TextElement FindMatchingCommand(uint8_t b) {
  TextElement empty_element;
  for (const auto &text_element : TextCommands) {
    if (text_element.ID == b) {
      return text_element;
    }
  }
  return empty_element;
}

TextElement FindMatchingSpecial(uint8_t value) {
  auto it = std::find_if(SpecialChars.begin(), SpecialChars.end(),
                         [value](const TextElement &text_element) {
                           return text_element.ID == value;
                         });
  if (it != SpecialChars.end()) {
    return *it;
  }

  return TextElement();
}

ParsedElement FindMatchingElement(const std::string &str) {
  std::smatch match;
  for (auto &text_element : TextCommands) {
    match = text_element.MatchMe(str);
    if (match.size() > 0) {
      if (text_element.HasArgument) {
        return ParsedElement(text_element,
                             std::stoi(match[1].str(), nullptr, 16));
      } else {
        return ParsedElement(text_element, 0);
      }
    }
  }

  const auto dictionary_element =
      TextElement(0x80, DICTIONARYTOKEN, true, "Dictionary");

  match = dictionary_element.MatchMe(str);
  if (match.size() > 0) {
    return ParsedElement(dictionary_element,
                         DICTOFF + std::stoi(match[1].str(), nullptr, 16));
  }
  return ParsedElement();
}

std::string ParseTextDataByte(uint8_t value) {
  if (CharEncoder.contains(value)) {
    char c = CharEncoder.at(value);
    std::string str = "";
    str.push_back(c);
    return str;
  }

  // Check for command.
  TextElement text_element = FindMatchingCommand(value);
  if (!text_element.Empty()) {
    return text_element.GenericToken;
  }

  // Check for special characters.
  text_element = FindMatchingSpecial(value);
  if (!text_element.Empty()) {
    return text_element.GenericToken;
  }

  // Check for dictionary.
  int dictionary = FindDictionaryEntry(value);
  if (dictionary >= 0) {
    return absl::StrFormat("[%s:%X]", DICTIONARYTOKEN, dictionary);
  }

  return "";
}

std::vector<uint8_t> ParseMessageToData(std::string str) {
  std::vector<uint8_t> bytes;
  std::string temp_string = str;
  int pos = 0;

  while (pos < temp_string.size()) {
    // Get next text fragment.
    if (temp_string[pos] == '[') {
      int next = temp_string.find(']', pos);
      if (next == -1) {
        break;
      }

      ParsedElement parsedElement =
          FindMatchingElement(temp_string.substr(pos, next - pos + 1));

      const auto dictionary_element =
          TextElement(0x80, DICTIONARYTOKEN, true, "Dictionary");

      if (!parsedElement.Active) {
        core::logf("Error parsing message: %s", temp_string);
        break;
      } else if (parsedElement.Parent == dictionary_element) {
        bytes.push_back(parsedElement.Value);
      } else {
        bytes.push_back(parsedElement.Parent.ID);

        if (parsedElement.Parent.HasArgument) {
          bytes.push_back(parsedElement.Value);
        }
      }

      pos = next + 1;
      continue;
    } else {
      uint8_t bb = FindMatchingCharacter(temp_string[pos++]);

      if (bb != 0xFF) {
        core::logf("Error parsing message: %s", temp_string);
        bytes.push_back(bb);
      }
    }
  }

  return bytes;
}

std::vector<DictionaryEntry> BuildDictionaryEntries(Rom *rom) {
  std::vector<DictionaryEntry> AllDictionaries;
  for (int i = 0; i < kNumDictionaryEntries; i++) {
    std::vector<uint8_t> bytes;
    std::stringstream stringBuilder;

    int address = core::SnesToPc(
        kTextData + (rom->data()[kPointersDictionaries + (i * 2) + 1] << 8) +
        rom->data()[kPointersDictionaries + (i * 2)]);

    int temppush_backress = core::SnesToPc(
        kTextData +
        (rom->data()[kPointersDictionaries + ((i + 1) * 2) + 1] << 8) +
        rom->data()[kPointersDictionaries + ((i + 1) * 2)]);

    while (address < temppush_backress) {
      uint8_t uint8_tDictionary = rom->data()[address++];
      bytes.push_back(uint8_tDictionary);
      stringBuilder << ParseTextDataByte(uint8_tDictionary);
    }

    AllDictionaries.push_back(DictionaryEntry{(uint8_t)i, stringBuilder.str()});
  }

  std::sort(AllDictionaries.begin(), AllDictionaries.end(),
            [](const DictionaryEntry &a, const DictionaryEntry &b) {
              return a.Contents.size() > b.Contents.size();
            });

  return AllDictionaries;
}

absl::StatusOr<MessageData> ParseSingleMessage(
    const std::vector<uint8_t> &rom_data, int *current_pos) {
  MessageData message_data;
  int pos = *current_pos;
  uint8_t current_byte;
  std::vector<uint8_t> temp_bytes_raw;
  std::vector<uint8_t> temp_bytes_parsed;
  std::string current_message_raw;
  std::string current_message_parsed;

  // Read the message data
  while (true) {
    current_byte = rom_data[pos++];

    if (current_byte == kMessageTerminator) {
      message_data.ID = message_data.ID + 1;
      message_data.Address = pos;
      message_data.RawString = current_message_raw;
      message_data.Data = temp_bytes_raw;
      message_data.DataParsed = temp_bytes_parsed;
      message_data.ContentsParsed = current_message_parsed;

      temp_bytes_raw.clear();
      temp_bytes_parsed.clear();
      current_message_raw.clear();
      current_message_parsed.clear();

      break;
    } else if (current_byte == 0xFF) {
      break;
    }

    temp_bytes_raw.push_back(current_byte);

    // Check for command.
    TextElement text_element = FindMatchingCommand(current_byte);
    if (!text_element.Empty()) {
      current_message_raw.append(text_element.GetParamToken());
      current_message_parsed.append(text_element.GetParamToken());
      temp_bytes_parsed.push_back(current_byte);
      continue;
    }

    // Check for dictionary.
    int dictionary = FindDictionaryEntry(current_byte);
    if (dictionary >= 0) {
      current_message_raw.append("[");
      current_message_raw.append(DICTIONARYTOKEN);
      current_message_raw.append(":");
      current_message_raw.append(core::HexWord(dictionary));
      current_message_raw.append("]");

      auto mutable_rom_data = const_cast<uint8_t *>(rom_data.data());
      uint32_t address = core::Get24LocalFromPC(
          mutable_rom_data, kPointersDictionaries + (dictionary * 2));
      uint32_t address_end = core::Get24LocalFromPC(
          mutable_rom_data, kPointersDictionaries + ((dictionary + 1) * 2));

      for (uint32_t i = address; i < address_end; i++) {
        temp_bytes_parsed.push_back(rom_data[i]);
        current_message_parsed.append(ParseTextDataByte(rom_data[i]));
      }

      continue;
    }

    // Everything else.
    if (CharEncoder.contains(current_byte)) {
      std::string str = "";
      str.push_back(CharEncoder.at(current_byte));
      current_message_raw.append(str);
      current_message_parsed.append(str);
      temp_bytes_parsed.push_back(current_byte);
    }
  }

  *current_pos = pos;
  return message_data;
}

std::vector<std::string> ParseMessageData(
    std::vector<MessageData> &message_data,
    const std::vector<DictionaryEntry> &dictionary_entries) {
  std::vector<std::string> parsed_messages;

  for (auto &message : message_data) {
    std::cout << "Message #" << message.ID << " at address "
              << core::HexLong(message.Address) << std::endl;
    std::cout << "  " << message.RawString << std::endl;

    std::string parsed_message = "";
    for (const uint8_t &byte : message.Data) {
      if (CharEncoder.contains(byte)) {
        parsed_message.push_back(CharEncoder.at(byte));
      } else {
        if (byte >= DICTOFF && byte < (DICTOFF + 97)) {
          if (byte > 0 && byte <= dictionary_entries.size()) {
            auto dic_entry = dictionary_entries[byte];
            parsed_message.append(dic_entry.Contents);
          } else {
            parsed_message.append(dictionary_entries[0].Contents);
          }
        } else {
          auto text_element = FindMatchingCommand(byte);
          if (!text_element.Empty()) {
            if (text_element.ID == kScrollVertical ||
                text_element.ID == kLine2 || text_element.ID == kLine3) {
              parsed_message.append("\n");
            }
            parsed_message.append(text_element.GenericToken);
          }
        }
      }
    }
    parsed_messages.push_back(parsed_message);
  }

  return parsed_messages;
}

std::vector<std::string> ImportMessageData(std::string_view filename) {
  std::vector<std::string> messages;
  std::ifstream file(filename.data());
  if (!file.is_open()) {
    core::logf("Error opening file: %s", filename);
    return messages;
  }

  // Parse a file with dialogue IDs and convert
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) {
      continue;
    }

    // Get the Dialogue ID and then read until the next header
  }

  return messages;
}

}  // namespace editor
}  // namespace yaze
