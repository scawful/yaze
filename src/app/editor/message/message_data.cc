#include "message_data.h"

#include "app/core/common.h"

namespace yaze {
namespace app {
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
  for (const auto& text_element : TextCommands) {
    if (text_element.ID == b) {
      return text_element;
    }
  }
  return empty_element;
}

TextElement FindMatchingSpecial(uint8_t value) {
  auto it = std::find_if(SpecialChars.begin(), SpecialChars.end(),
                         [value](const TextElement& text_element) {
                           return text_element.ID == value;
                         });
  if (it != SpecialChars.end()) {
    return *it;
  }

  return TextElement();
}

ParsedElement FindMatchingElement(const std::string& str) {
  std::smatch match;
  for (auto& textElement : TextCommands) {
    match = textElement.MatchMe(str);
    if (match.size() > 0) {
      if (textElement.HasArgument) {
        return ParsedElement(textElement,
                             std::stoi(match[1].str(), nullptr, 16));
      } else {
        return ParsedElement(textElement, 0);
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
  TextElement textElement = FindMatchingCommand(value);
  if (!textElement.Empty()) {
    return textElement.GenericToken;
  }

  // Check for special characters.
  textElement = FindMatchingSpecial(value);
  if (!textElement.Empty()) {
    return textElement.GenericToken;
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

}  // namespace editor
}  // namespace app
}  // namespace yaze