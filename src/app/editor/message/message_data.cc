#include "message_data.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "core/rom_settings.h"
#include "rom/snes.h"
#include "rom/write_fence.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

int GetExpandedTextDataStart() {
  return static_cast<int>(core::RomSettings::Get().GetAddressOr(
      core::RomAddressKey::kExpandedMessageStart, kExpandedTextDataDefault));
}

int GetExpandedTextDataEnd() {
  return static_cast<int>(core::RomSettings::Get().GetAddressOr(
      core::RomAddressKey::kExpandedMessageEnd, kExpandedTextDataEndDefault));
}

uint8_t FindMatchingCharacter(char value) {
  for (const auto [key, char_value] : CharEncoder) {
    if (value == char_value) {
      return key;
    }
  }
  return 0xFF;
}

int8_t FindDictionaryEntry(uint8_t value) {
  if (value < DICTOFF || value == 0xFF) {
    return -1;
  }
  return value - DICTOFF;
}

std::optional<TextElement> FindMatchingCommand(uint8_t b) {
  for (const auto& text_element : TextCommands) {
    if (text_element.ID == b) {
      return text_element;
    }
  }
  return std::nullopt;
}

std::optional<TextElement> FindMatchingSpecial(uint8_t value) {
  auto it = std::ranges::find_if(SpecialChars,
                                 [value](const TextElement& text_element) {
                                   return text_element.ID == value;
                                 });
  if (it != SpecialChars.end()) {
    return *it;
  }
  return std::nullopt;
}

ParsedElement FindMatchingElement(const std::string& str) {
  std::smatch match;
  std::vector<TextElement> commands_and_chars = TextCommands;
  commands_and_chars.insert(commands_and_chars.end(), SpecialChars.begin(),
                            SpecialChars.end());
  for (auto& text_element : commands_and_chars) {
    match = text_element.MatchMe(str);
    if (match.size() > 0) {
      if (text_element.HasArgument) {
        std::string arg = match[1].str().substr(1);
        try {
          return ParsedElement(text_element, std::stoi(arg, nullptr, 16));
        } catch (const std::invalid_argument& e) {
          util::logf("Error parsing argument for %s: %s",
                     text_element.GenericToken.c_str(), arg.c_str());
          return ParsedElement(text_element, 0);
        } catch (const std::out_of_range& e) {
          util::logf("Argument out of range for %s: %s",
                     text_element.GenericToken.c_str(), arg.c_str());
          return ParsedElement(text_element, 0);
        }
      } else {
        return ParsedElement(text_element, 0);
      }
    }
  }

  const auto dictionary_element =
      TextElement(0x80, DICTIONARYTOKEN, true, "Dictionary");

  match = dictionary_element.MatchMe(str);
  if (match.size() > 0) {
    try {
      // match[1] captures ":XX" — strip the leading colon
      std::string dict_arg = match[1].str().substr(1);
      return ParsedElement(dictionary_element,
                           DICTOFF + std::stoi(dict_arg, nullptr, 16));
    } catch (const std::exception& e) {
      util::logf("Error parsing dictionary token: %s", match[1].str().c_str());
      return ParsedElement();
    }
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
  if (auto text_element = FindMatchingCommand(value);
      text_element != std::nullopt) {
    return text_element->GenericToken;
  }

  // Check for special characters.
  if (auto special_element = FindMatchingSpecial(value);
      special_element != std::nullopt) {
    return special_element->GenericToken;
  }

  // Check for dictionary.
  int8_t dictionary = FindDictionaryEntry(value);
  if (dictionary >= 0) {
    return absl::StrFormat("[%s:%02X]", DICTIONARYTOKEN,
                           static_cast<unsigned char>(dictionary));
  }

  return "";
}

std::vector<uint8_t> ParseMessageToData(std::string str) {
  std::vector<uint8_t> bytes;
  std::string temp_string = std::move(str);
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
        util::logf("Error parsing message: %s", temp_string);
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
        bytes.push_back(bb);
      }
    }
  }

  return bytes;
}

MessageParseResult ParseMessageToDataWithDiagnostics(std::string_view str) {
  MessageParseResult result;
  std::string temp_string(str);
  size_t pos = 0;
  bool warned_newline = false;

  while (pos < temp_string.size()) {
    char current = temp_string[pos];
    if (current == '\r' || current == '\n') {
      if (!warned_newline) {
        result.warnings.push_back(
            "Literal newlines are ignored; use [1], [2], [3], [V], or [K] "
            "tokens for line breaks.");
        warned_newline = true;
      }
      pos++;
      continue;
    }

    if (current == '[') {
      size_t close = temp_string.find(']', pos);
      if (close == std::string::npos) {
        result.errors.push_back(
            absl::StrFormat("Unclosed token starting at position %zu", pos));
        break;
      }

      std::string token = temp_string.substr(pos, close - pos + 1);
      ParsedElement parsed_element = FindMatchingElement(token);
      const auto dictionary_element =
          TextElement(0x80, DICTIONARYTOKEN, true, "Dictionary");

      if (!parsed_element.Active) {
        result.errors.push_back(absl::StrFormat("Unknown token: %s", token));
        pos = close + 1;
        continue;
      }

      if (!parsed_element.Parent.HasArgument) {
        if (token != parsed_element.Parent.GetParamToken()) {
          result.errors.push_back(absl::StrFormat("Unknown token: %s", token));
          pos = close + 1;
          continue;
        }
      }

      if (parsed_element.Parent == dictionary_element) {
        result.bytes.push_back(parsed_element.Value);
      } else {
        result.bytes.push_back(parsed_element.Parent.ID);
        if (parsed_element.Parent.HasArgument) {
          result.bytes.push_back(parsed_element.Value);
        }
      }

      pos = close + 1;
      continue;
    }

    uint8_t bb = FindMatchingCharacter(current);
    if (bb == 0xFF) {
      result.errors.push_back(absl::StrFormat(
          "Unsupported character '%c' at position %zu", current, pos));
      pos++;
      continue;
    }

    result.bytes.push_back(bb);
    pos++;
  }

  return result;
}

std::string MessageBankToString(MessageBank bank) {
  switch (bank) {
    case MessageBank::kVanilla:
      return "vanilla";
    case MessageBank::kExpanded:
      return "expanded";
  }
  return "vanilla";
}

absl::StatusOr<MessageBank> MessageBankFromString(std::string_view value) {
  const std::string lowered = absl::AsciiStrToLower(value);
  if (lowered == "vanilla") {
    return MessageBank::kVanilla;
  }
  if (lowered == "expanded") {
    return MessageBank::kExpanded;
  }
  return absl::InvalidArgumentError(
      absl::StrFormat("Unknown message bank: %s", value));
}

std::vector<DictionaryEntry> BuildDictionaryEntries(Rom* rom) {
  std::vector<DictionaryEntry> AllDictionaries;
  for (int i = 0; i < kNumDictionaryEntries; i++) {
    std::vector<uint8_t> bytes;
    std::stringstream stringBuilder;

    int address = SnesToPc(
        kTextData + (rom->data()[kPointersDictionaries + (i * 2) + 1] << 8) +
        rom->data()[kPointersDictionaries + (i * 2)]);

    int temppush_backress =
        SnesToPc(kTextData +
                 (rom->data()[kPointersDictionaries + ((i + 1) * 2) + 1] << 8) +
                 rom->data()[kPointersDictionaries + ((i + 1) * 2)]);

    while (address < temppush_backress) {
      uint8_t uint8_tDictionary = rom->data()[address++];
      bytes.push_back(uint8_tDictionary);
      stringBuilder << ParseTextDataByte(uint8_tDictionary);
    }

    AllDictionaries.push_back(DictionaryEntry{(uint8_t)i, stringBuilder.str()});
  }

  std::ranges::sort(AllDictionaries,
                    [](const DictionaryEntry& a, const DictionaryEntry& b) {
                      return a.Contents.size() > b.Contents.size();
                    });

  return AllDictionaries;
}

std::string ReplaceAllDictionaryWords(
    std::string str, const std::vector<DictionaryEntry>& dictionary) {
  std::string temp = std::move(str);
  for (const auto& entry : dictionary) {
    if (entry.ContainedInString(temp)) {
      temp = entry.ReplaceInstancesOfIn(temp);
    }
  }
  return temp;
}

DictionaryEntry FindRealDictionaryEntry(
    uint8_t value, const std::vector<DictionaryEntry>& dictionary) {
  for (const auto& entry : dictionary) {
    if (entry.ID + DICTOFF == value) {
      return entry;
    }
  }
  return DictionaryEntry();
}

absl::StatusOr<MessageData> ParseSingleMessage(
    const std::vector<uint8_t>& rom_data, int* current_pos) {
  if (current_pos == nullptr) {
    return absl::InvalidArgumentError("current_pos is null");
  }
  if (*current_pos < 0 ||
      static_cast<size_t>(*current_pos) >= rom_data.size()) {
    return absl::OutOfRangeError("current_pos is out of range");
  }

  MessageData message_data;
  int pos = *current_pos;
  uint8_t current_byte;
  std::vector<uint8_t> temp_bytes_raw;
  std::vector<uint8_t> temp_bytes_parsed;
  std::string current_message_raw;
  std::string current_message_parsed;

  // Read the message data
  while (pos < static_cast<int>(rom_data.size())) {
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

      *current_pos = pos;
      return message_data;
    } else if (current_byte == 0xFF) {
      return absl::InvalidArgumentError("message terminator not found");
    }

    temp_bytes_raw.push_back(current_byte);

    // Check for command.
    auto text_element = FindMatchingCommand(current_byte);
    if (text_element != std::nullopt) {
      temp_bytes_parsed.push_back(current_byte);
      if (text_element->HasArgument) {
        if (pos >= static_cast<int>(rom_data.size())) {
          return absl::OutOfRangeError("message command argument out of range");
        }
        uint8_t arg_byte = rom_data[pos++];
        temp_bytes_raw.push_back(arg_byte);
        temp_bytes_parsed.push_back(arg_byte);
        current_message_raw.append(text_element->GetParamToken(arg_byte));
        current_message_parsed.append(text_element->GetParamToken(arg_byte));
      } else {
        current_message_raw.append(text_element->GetParamToken());
        current_message_parsed.append(text_element->GetParamToken());
      }
      continue;
    }

    // Check for special characters.
    if (auto special_element = FindMatchingSpecial(current_byte);
        special_element != std::nullopt) {
      current_message_raw.append(special_element->GetParamToken());
      current_message_parsed.append(special_element->GetParamToken());
      temp_bytes_parsed.push_back(current_byte);
      continue;
    }

    // Check for dictionary.
    int8_t dictionary = FindDictionaryEntry(current_byte);
    if (dictionary >= 0) {
      std::string token = absl::StrFormat(
          "[%s:%02X]", DICTIONARYTOKEN, static_cast<unsigned char>(dictionary));
      current_message_raw.append(token);
      current_message_parsed.append(token);
      temp_bytes_parsed.push_back(current_byte);
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
  return absl::InvalidArgumentError("message terminator not found");
}

std::vector<std::string> ParseMessageData(
    std::vector<MessageData>& message_data,
    const std::vector<DictionaryEntry>& dictionary_entries) {
  std::vector<std::string> parsed_messages;

  for (auto& message : message_data) {
    std::string parsed_message = "";
    // Use index-based loop to properly skip argument bytes
    for (size_t pos = 0; pos < message.Data.size(); ++pos) {
      uint8_t byte = message.Data[pos];

      // Check for text commands first (they may have arguments to skip)
      auto text_element = FindMatchingCommand(byte);
      if (text_element != std::nullopt) {
        // Add newline for certain commands
        if (text_element->ID == kScrollVertical || text_element->ID == kLine2 ||
            text_element->ID == kLine3) {
          parsed_message.append("\n");
        }
        // If command has an argument, get it from next byte and skip it
        if (text_element->HasArgument && pos + 1 < message.Data.size()) {
          uint8_t arg_byte = message.Data[pos + 1];
          parsed_message.append(text_element->GetParamToken(arg_byte));
          pos++;  // Skip the argument byte
        } else {
          parsed_message.append(text_element->GetParamToken());
        }
        continue;  // Move to next byte
      }

      // Check for special characters
      auto special_element = FindMatchingSpecial(byte);
      if (special_element != std::nullopt) {
        parsed_message.append(special_element->GetParamToken());
        continue;
      }

      // Check for dictionary entries
      if (byte >= DICTOFF && byte < (DICTOFF + 97)) {
        DictionaryEntry dic_entry;
        for (const auto& entry : dictionary_entries) {
          if (entry.ID == byte - DICTOFF) {
            dic_entry = entry;
            break;
          }
        }
        parsed_message.append(dic_entry.Contents);
        continue;
      }

      // Finally check for regular characters
      if (CharEncoder.contains(byte)) {
        parsed_message.push_back(CharEncoder.at(byte));
      }
    }
    parsed_messages.push_back(parsed_message);
  }

  return parsed_messages;
}

std::vector<MessageData> ReadAllTextData(uint8_t* rom, int pos, int max_pos) {
  std::vector<MessageData> list_of_texts;
  int message_id = 0;

  if (!rom) {
    return list_of_texts;
  }
  if (max_pos > 0 && (pos < 0 || pos >= max_pos)) {
    return list_of_texts;
  }

  std::vector<uint8_t> raw_message;
  std::vector<uint8_t> parsed_message;
  std::string current_raw_message;
  std::string current_parsed_message;

  bool did_bank_switch = false;
  uint8_t current_byte = 0;
  while (current_byte != 0xFF) {
    if (max_pos > 0 && (pos < 0 || pos >= max_pos)) break;
    current_byte = rom[pos++];
    if (current_byte == kMessageTerminator) {
      list_of_texts.push_back(
          MessageData(message_id++, pos, current_raw_message, raw_message,
                      current_parsed_message, parsed_message));
      raw_message.clear();
      parsed_message.clear();
      current_raw_message.clear();
      current_parsed_message.clear();
      continue;
    } else if (current_byte == 0xFF) {
      break;
    }

    raw_message.push_back(current_byte);

    auto text_element = FindMatchingCommand(current_byte);
    if (text_element != std::nullopt) {
      parsed_message.push_back(current_byte);
      if (text_element->HasArgument) {
        if (max_pos > 0 && (pos < 0 || pos >= max_pos)) break;
        current_byte = rom[pos++];
        raw_message.push_back(current_byte);
        parsed_message.push_back(current_byte);
      }

      current_raw_message.append(text_element->GetParamToken(current_byte));
      current_parsed_message.append(text_element->GetParamToken(current_byte));

      if (text_element->Token == kBankToken && !did_bank_switch) {
        did_bank_switch = true;
        pos = kTextData2;
      }

      continue;
    }

    // Check for special characters.
    auto special_element = FindMatchingSpecial(current_byte);
    if (special_element != std::nullopt) {
      current_raw_message.append(special_element->GetParamToken());
      current_parsed_message.append(special_element->GetParamToken());
      parsed_message.push_back(current_byte);
      continue;
    }

    // Check for dictionary.
    int8_t dictionary = FindDictionaryEntry(current_byte);
    if (dictionary >= 0) {
      current_raw_message.append(absl::StrFormat(
          "[%s:%s]", DICTIONARYTOKEN,
          util::HexByte(static_cast<unsigned char>(dictionary))));

      // Safety: bounds-check dictionary pointer reads and dictionary expansion.
      // This parser is used by tooling (RomDoctor) that may run on dummy or
      // partially-initialized ROM buffers.
      const int ptr_a = kPointersDictionaries + (dictionary * 2);
      const int ptr_b = kPointersDictionaries + ((dictionary + 1) * 2);
      if (max_pos > 0) {
        if (ptr_a < 0 || ptr_a + 1 >= max_pos || ptr_b < 0 || ptr_b + 1 >= max_pos) {
          continue;
        }
      }

      uint32_t address =
          Get24LocalFromPC(rom, kPointersDictionaries + (dictionary * 2));
      uint32_t address_end =
          Get24LocalFromPC(rom, kPointersDictionaries + ((dictionary + 1) * 2));

      if (max_pos > 0) {
        const uint32_t max_u = static_cast<uint32_t>(max_pos);
        if (address >= max_u || address_end > max_u || address_end < address) {
          continue;
        }
      }

      for (uint32_t i = address; i < address_end; i++) {
        if (max_pos > 0 && i >= static_cast<uint32_t>(max_pos)) break;
        parsed_message.push_back(rom[i]);
        current_parsed_message.append(ParseTextDataByte(rom[i]));
      }

      continue;
    }

    // Everything else.
    if (CharEncoder.contains(current_byte)) {
      std::string str = "";
      str.push_back(CharEncoder.at(current_byte));
      current_raw_message.append(str);
      current_parsed_message.append(str);
      parsed_message.push_back(current_byte);
    }
  }

  return list_of_texts;
}

absl::Status LoadExpandedMessages(std::string& expanded_message_path,
                                  std::vector<std::string>& parsed_messages,
                                  std::vector<MessageData>& expanded_messages,
                                  std::vector<DictionaryEntry>& dictionary) {
  static Rom expanded_message_rom;
  if (!expanded_message_rom.LoadFromFile(expanded_message_path).ok()) {
    return absl::InternalError("Failed to load expanded message ROM");
  }
  expanded_messages = ReadAllTextData(expanded_message_rom.mutable_data(), 0);
  auto parsed_expanded_messages =
      ParseMessageData(expanded_messages, dictionary);
  // Insert into parsed_messages
  for (const auto& expanded_message : expanded_messages) {
    parsed_messages.push_back(parsed_expanded_messages[expanded_message.ID]);
  }
  return absl::OkStatus();
}

nlohmann::json SerializeMessagesToJson(
    const std::vector<MessageData>& messages) {
  nlohmann::json j = nlohmann::json::array();
  for (const auto& msg : messages) {
    j.push_back({{"id", msg.ID},
                 {"address", msg.Address},
                 {"raw_string", msg.RawString},
                 {"parsed_string", msg.ContentsParsed}});
  }
  return j;
}

absl::Status ExportMessagesToJson(const std::string& path,
                                  const std::vector<MessageData>& messages) {
  try {
    nlohmann::json j = SerializeMessagesToJson(messages);
    std::ofstream file(path);
    if (!file.is_open()) {
      return absl::InternalError(
          absl::StrFormat("Failed to open file for writing: %s", path));
    }
    file << j.dump(2);  // Pretty print with 2-space indent
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("JSON export failed: %s", e.what()));
  }
}

nlohmann::json SerializeMessageBundle(const std::vector<MessageData>& vanilla,
                                      const std::vector<MessageData>& expanded) {
  nlohmann::json j;
  j["format"] = "yaze-message-bundle";
  j["version"] = kMessageBundleVersion;
  j["counts"] = {{"vanilla", vanilla.size()}, {"expanded", expanded.size()}};
  j["messages"] = nlohmann::json::array();

  auto append_messages = [&j](const std::vector<MessageData>& messages,
                              MessageBank bank) {
    for (const auto& msg : messages) {
      nlohmann::json entry;
      entry["id"] = msg.ID;
      entry["bank"] = MessageBankToString(bank);
      entry["address"] = msg.Address;
      entry["raw"] = msg.RawString;
      entry["parsed"] = msg.ContentsParsed;
      entry["text"] =
          !msg.RawString.empty() ? msg.RawString : msg.ContentsParsed;
      entry["length"] = msg.Data.size();
      const std::string validation_text =
          !msg.RawString.empty() ? msg.RawString : msg.ContentsParsed;
      auto warnings = ValidateMessageLineWidths(validation_text);
      if (!warnings.empty()) {
        entry["line_width_warnings"] = warnings;
      }
      j["messages"].push_back(entry);
    }
  };

  append_messages(vanilla, MessageBank::kVanilla);
  append_messages(expanded, MessageBank::kExpanded);

  return j;
}

absl::Status ExportMessageBundleToJson(
    const std::string& path, const std::vector<MessageData>& vanilla,
    const std::vector<MessageData>& expanded) {
  try {
    nlohmann::json j = SerializeMessageBundle(vanilla, expanded);
    std::ofstream file(path);
    if (!file.is_open()) {
      return absl::InternalError(
          absl::StrFormat("Failed to open file for writing: %s", path));
    }
    file << j.dump(2);
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Message bundle export failed: %s", e.what()));
  }
}

namespace {
absl::StatusOr<MessageBundleEntry> ParseMessageBundleEntry(
    const nlohmann::json& entry, MessageBank default_bank) {
  if (!entry.is_object()) {
    return absl::InvalidArgumentError("Message entry must be an object");
  }

  MessageBundleEntry result;
  result.id = entry.value("id", -1);
  if (result.id < 0) {
    return absl::InvalidArgumentError("Message entry missing valid id");
  }

  if (entry.contains("bank")) {
    if (!entry["bank"].is_string()) {
      return absl::InvalidArgumentError("Message entry bank must be string");
    }
    auto bank_or = MessageBankFromString(entry["bank"].get<std::string>());
    if (!bank_or.ok()) {
      return bank_or.status();
    }
    result.bank = bank_or.value();
  } else {
    result.bank = default_bank;
  }

  if (entry.contains("raw") && entry["raw"].is_string()) {
    result.raw = entry["raw"].get<std::string>();
  } else if (entry.contains("raw_string") && entry["raw_string"].is_string()) {
    result.raw = entry["raw_string"].get<std::string>();
  }

  if (entry.contains("parsed") && entry["parsed"].is_string()) {
    result.parsed = entry["parsed"].get<std::string>();
  } else if (entry.contains("parsed_string") &&
             entry["parsed_string"].is_string()) {
    result.parsed = entry["parsed_string"].get<std::string>();
  }

  if (entry.contains("text") && entry["text"].is_string()) {
    result.text = entry["text"].get<std::string>();
  }

  if (result.text.empty()) {
    if (!result.raw.empty()) {
      result.text = result.raw;
    } else if (!result.parsed.empty()) {
      result.text = result.parsed;
    }
  }

  if (result.text.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Message entry %d missing text content", result.id));
  }

  return result;
}
}  // namespace

absl::StatusOr<std::vector<MessageBundleEntry>> ParseMessageBundleJson(
    const nlohmann::json& json) {
  std::vector<MessageBundleEntry> entries;

  if (json.is_array()) {
    for (const auto& entry : json) {
      auto parsed_or =
          ParseMessageBundleEntry(entry, MessageBank::kVanilla);
      if (!parsed_or.ok()) {
        return parsed_or.status();
      }
      entries.push_back(parsed_or.value());
    }
    return entries;
  }

  if (!json.is_object()) {
    return absl::InvalidArgumentError("Message bundle JSON must be object");
  }

  if (json.contains("version") && json["version"].is_number_integer()) {
    int version = json["version"].get<int>();
    if (version != kMessageBundleVersion) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Unsupported message bundle version: %d", version));
    }
  }

  if (!json.contains("messages") || !json["messages"].is_array()) {
    return absl::InvalidArgumentError("Message bundle missing messages array");
  }

  for (const auto& entry : json["messages"]) {
    auto parsed_or =
        ParseMessageBundleEntry(entry, MessageBank::kVanilla);
    if (!parsed_or.ok()) {
      return parsed_or.status();
    }
    entries.push_back(parsed_or.value());
  }

  return entries;
}

absl::StatusOr<std::vector<MessageBundleEntry>> LoadMessageBundleFromJson(
    const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open message bundle: %s", path));
  }

  nlohmann::json json;
  try {
    file >> json;
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse JSON: %s", e.what()));
  }

  return ParseMessageBundleJson(json);
}

// ===========================================================================
// Line Width Validation
// ===========================================================================

std::vector<std::string> ValidateMessageLineWidths(
    const std::string& message) {
  std::vector<std::string> warnings;

  // Split message into lines on line-break tokens: [1], [2], [3], [V], [K]
  // We walk through the string, counting visible characters per line.
  int line_num = 1;
  int visible_chars = 0;
  bool all_spaces_this_line = true;
  size_t pos = 0;

  while (pos < message.size()) {
    if (message[pos] == '[') {
      // Find the closing bracket
      size_t close = message.find(']', pos);
      if (close == std::string::npos) break;

      std::string token = message.substr(pos, close - pos + 1);
      pos = close + 1;

      // Check if this token is a line-breaking command
      // Line breaks: [1], [2], [3], [V], [K]
      if (token == "[1]" || token == "[2]" || token == "[3]" ||
          token == "[V]" || token == "[K]") {
        // Check current line width before breaking.
        // Exempt whitespace-only lines (used as screen clears in ALTTP).
        if (visible_chars > kMaxLineWidth && !all_spaces_this_line) {
          warnings.push_back(
              absl::StrFormat("Line %d: %d visible characters (max %d)",
                              line_num, visible_chars, kMaxLineWidth));
        }
        line_num++;
        visible_chars = 0;
        all_spaces_this_line = true;
      }
      // Other command tokens ([W:02], [S:03], [SFX:2D], [L], [...], etc.)
      // are not counted as visible characters - they're control codes or
      // expand to game-rendered content that we can't measure in chars.
      // Exception: [L] expands to player name but width varies (1-6 chars).
      // For simplicity, we don't count command tokens.
      continue;
    }

    // Regular visible character
    if (message[pos] != ' ') all_spaces_this_line = false;
    visible_chars++;
    pos++;
  }

  // Check the last line (exempt whitespace-only lines)
  if (visible_chars > kMaxLineWidth && !all_spaces_this_line) {
    warnings.push_back(
        absl::StrFormat("Line %d: %d visible characters (max %d)", line_num,
                        visible_chars, kMaxLineWidth));
  }

  return warnings;
}

// ===========================================================================
// Org Format (.org) Import/Export
// ===========================================================================

std::optional<std::pair<int, std::string>> ParseOrgHeader(
    const std::string& line) {
  // Expected format: "** XX - Label Text"
  // where XX is a hex message ID
  if (line.size() < 6 || line[0] != '*' || line[1] != '*' || line[2] != ' ') {
    return std::nullopt;
  }

  // Find the " - " separator
  size_t sep = line.find(" - ", 3);
  if (sep == std::string::npos) {
    return std::nullopt;
  }

  // Parse hex ID between "** " and " - "
  std::string hex_id = line.substr(3, sep - 3);
  int message_id;
  try {
    message_id = std::stoi(hex_id, nullptr, 16);
  } catch (const std::exception&) {
    return std::nullopt;
  }

  // Extract label after " - "
  std::string label = line.substr(sep + 3);

  return std::make_pair(message_id, label);
}

std::vector<std::pair<int, std::string>> ParseOrgContent(
    const std::string& content) {
  std::vector<std::pair<int, std::string>> messages;
  std::istringstream stream(content);
  std::string line;

  int current_id = -1;
  std::string current_body;

  while (std::getline(stream, line)) {
    // Check if this is a header line
    auto header = ParseOrgHeader(line);
    if (header.has_value()) {
      // Save previous message if any
      if (current_id >= 0) {
        // Trim trailing newline from body
        while (!current_body.empty() && current_body.back() == '\n') {
          current_body.pop_back();
        }
        messages.push_back({current_id, current_body});
      }

      current_id = header->first;
      current_body.clear();
      continue;
    }

    // Skip top-level org headers (single *)
    if (!line.empty() && line[0] == '*' && (line.size() < 2 || line[1] != '*')) {
      continue;
    }

    // Accumulate body text
    if (current_id >= 0) {
      if (!current_body.empty()) {
        current_body += "\n";
      }
      current_body += line;
    }
  }

  // Save last message
  if (current_id >= 0) {
    while (!current_body.empty() && current_body.back() == '\n') {
      current_body.pop_back();
    }
    messages.push_back({current_id, current_body});
  }

  return messages;
}

std::string ExportToOrgFormat(
    const std::vector<std::pair<int, std::string>>& messages,
    const std::vector<std::string>& labels) {
  std::string output;
  output += "* Oracle of Secrets English Dialogue\n";

  for (size_t i = 0; i < messages.size(); ++i) {
    const auto& [msg_id, body] = messages[i];
    std::string label =
        (i < labels.size()) ? labels[i] : absl::StrFormat("Message %02X", msg_id);

    output += absl::StrFormat("** %02X - %s\n", msg_id, label);
    output += body;
    output += "\n\n";
  }

  return output;
}

// ===========================================================================
// Expanded Message Bank
// ===========================================================================

std::vector<MessageData> ReadExpandedTextData(uint8_t* rom, int pos) {
  // Reuse ReadAllTextData — it already handles 0x7F terminators and 0xFF end
  return ReadAllTextData(rom, pos);
}

absl::Status WriteExpandedTextData(Rom* rom, int start, int end,
                                   const std::vector<std::string>& messages) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  if (start < 0 || end < start) {
    return absl::InvalidArgumentError("Invalid expanded message region");
  }

  const int capacity = end - start + 1;
  if (capacity <= 0) {
    return absl::InvalidArgumentError("Expanded message region has no capacity");
  }

  const auto& data = rom->vector();
  if (end >= static_cast<int>(data.size())) {
    return absl::OutOfRangeError("Expanded message region out of ROM range");
  }

  // Serialize into a contiguous buffer, then do a single ROM write for safety
  // and determinism (and to honor write fences).
  std::vector<uint8_t> blob;
  blob.reserve(static_cast<size_t>(capacity));

  int used = 0;
  for (size_t i = 0; i < messages.size(); ++i) {
    auto bytes = ParseMessageToData(messages[i]);
    const int needed = static_cast<int>(bytes.size()) + 1;  // +0x7F

    // Always reserve space for the final 0xFF.
    if (used + needed + 1 > capacity) {
      return absl::ResourceExhaustedError(absl::StrFormat(
          "Expanded message data exceeds bank boundary "
          "(at message %d, used=%d, needed=%d, capacity=%d, end=0x%06X)",
          static_cast<int>(i), used, needed, capacity, end));
    }

    blob.insert(blob.end(), bytes.begin(), bytes.end());
    blob.push_back(kMessageTerminator);
    used += needed;
  }

  if (used + 1 > capacity) {
    return absl::ResourceExhaustedError(
        "No space for end-of-region marker (0xFF)");
  }
  blob.push_back(0xFF);

  // ROM safety: this writer must only touch the expanded message region.
  // NOTE: `end` is inclusive; convert to half-open for the fence.
  yaze::rom::WriteFence fence;
  const uint32_t fence_start = static_cast<uint32_t>(start);
  const uint32_t fence_end =
      static_cast<uint32_t>(static_cast<uint64_t>(end) + 1ULL);
  RETURN_IF_ERROR(
      fence.Allow(fence_start, fence_end, "ExpandedMessageBank"));
  yaze::rom::ScopedWriteFence scope(rom, &fence);

  return rom->WriteVector(start, std::move(blob));
}

absl::Status WriteExpandedTextData(uint8_t* rom, int start, int end,
                                   const std::vector<std::string>& messages) {
  int pos = start;
  int capacity = end - start + 1;

  for (size_t i = 0; i < messages.size(); ++i) {
    auto bytes = ParseMessageToData(messages[i]);

    // Check space: bytes + terminator (0x7F) + final end marker (0xFF)
    int needed = static_cast<int>(bytes.size()) + 1;  // +1 for 0x7F
    if (i == messages.size() - 1) {
      needed += 1;  // +1 for final 0xFF
    }

    if (pos + needed - start > capacity) {
      return absl::ResourceExhaustedError(absl::StrFormat(
          "Expanded message data exceeds bank boundary "
          "(at message %d, pos 0x%06X, end 0x%06X)",
          static_cast<int>(i), pos, end));
    }

    // Write encoded bytes
    for (uint8_t byte : bytes) {
      rom[pos++] = byte;
    }
    // Write message terminator
    rom[pos++] = kMessageTerminator;
  }

  // Write end-of-region marker
  if (pos - start >= capacity) {
    return absl::ResourceExhaustedError(
        "No space for end-of-region marker (0xFF)");
  }
  rom[pos++] = 0xFF;

  return absl::OkStatus();
}

absl::Status WriteAllTextData(Rom* rom,
                              const std::vector<MessageData>& messages) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  int pos = kTextData;
  bool in_second_bank = false;

  for (const auto& message : messages) {
    for (uint8_t value : message.Data) {
      RETURN_IF_ERROR(rom->WriteByte(pos, value));

      if (value == kBankSwitchCommand) {
        if (!in_second_bank && pos > kTextDataEnd) {
          return absl::ResourceExhaustedError(absl::StrFormat(
              "Text data exceeds first bank (pos 0x%06X)", pos));
        }
        pos = kTextData2 - 1;
        in_second_bank = true;
      }

      pos++;
    }

    RETURN_IF_ERROR(rom->WriteByte(pos++, kMessageTerminator));
  }

  if (!in_second_bank && pos > kTextDataEnd) {
    return absl::ResourceExhaustedError(
        absl::StrFormat("Text data exceeds first bank (pos 0x%06X)", pos));
  }

  if (in_second_bank && pos > kTextData2End) {
    return absl::ResourceExhaustedError(
        absl::StrFormat("Text data exceeds second bank (pos 0x%06X)", pos));
  }

  RETURN_IF_ERROR(rom->WriteByte(pos, 0xFF));
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
