#include "cli/handlers/graphics/hex_commands.h"

#include <cctype>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"

namespace yaze {
namespace cli {
namespace handlers {

absl::Status HexReadCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto address_str = parser.GetString("address").value();
  auto length = parser.GetInt("length").value_or(16);
  std::string format = "both";
  if (auto data_format = parser.GetString("data-format");
      data_format.has_value()) {
    format = *data_format;
  } else if (auto fmt = parser.GetString("format"); fmt.has_value()) {
    if (*fmt == "hex" || *fmt == "ascii" || *fmt == "both" ||
        *fmt == "binary") {
      format = *fmt;
    }
  }

  // Parse address
  uint32_t address;
  try {
    size_t pos;
    address = std::stoul(address_str, &pos, 16);
    if (pos != address_str.size()) {
      return absl::InvalidArgumentError("Invalid hex address format");
    }
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Failed to parse address '%s': %s", address_str, e.what()));
  }

  // Validate range
  if (address + length > rom->size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Read beyond ROM: 0x%X+%d > %zu", address, length, rom->size()));
  }

  // Read and format data
  const uint8_t* data = rom->data() + address;

  formatter.BeginObject("Hex Read");
  formatter.AddHexField("address", address, 6);
  formatter.AddField("length", length);
  formatter.AddField("format", format);

  bool include_hex = (format == "hex" || format == "both" || format == "binary");
  bool include_ascii = (format == "ascii" || format == "both");

  if (include_hex) {
    std::string hex_data;
    for (int i = 0; i < length; ++i) {
      absl::StrAppendFormat(&hex_data, "%02X", data[i]);
      if (i < length - 1)
        hex_data += " ";
    }
    formatter.AddField("hex_data", hex_data);
  }

  if (include_ascii) {
    std::string ascii_data;
    for (int i = 0; i < length; ++i) {
      char c = static_cast<char>(data[i]);
      ascii_data += (std::isprint(static_cast<unsigned char>(c)) ? c : '.');
    }
    formatter.AddField("ascii_data", ascii_data);
  }
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status HexWriteCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto address_str = parser.GetString("address").value();
  auto data_str = parser.GetString("data").value();

  // Parse address
  uint32_t address;
  try {
    size_t pos;
    address = std::stoul(address_str, &pos, 16);
    if (pos != address_str.size()) {
      return absl::InvalidArgumentError("Invalid hex address format");
    }
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Failed to parse address '%s': %s", address_str, e.what()));
  }

  // Parse data bytes
  std::vector<std::string> byte_strs = absl::StrSplit(data_str, ' ');
  std::vector<uint8_t> bytes;

  for (const auto& byte_str : byte_strs) {
    if (byte_str.empty())
      continue;
    try {
      uint8_t value = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
      bytes.push_back(value);
    } catch (const std::exception& e) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid byte '%s': %s", byte_str, e.what()));
    }
  }

  // Validate range
  if (address + bytes.size() > rom->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Write beyond ROM: 0x%X+%zu > %zu", address,
                        bytes.size(), rom->size()));
  }

  // Write data
  for (size_t i = 0; i < bytes.size(); ++i) {
    rom->WriteByte(address + i, bytes[i]);
  }

  // Format written data
  std::string hex_data;
  for (size_t i = 0; i < bytes.size(); ++i) {
    absl::StrAppendFormat(&hex_data, "%02X", bytes[i]);
    if (i < bytes.size() - 1)
      hex_data += " ";
  }

  formatter.BeginObject("Hex Write");
  formatter.AddHexField("address", address, 6);
  formatter.AddField("bytes_written", static_cast<int>(bytes.size()));
  formatter.AddField("hex_data", hex_data);
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status HexSearchCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto pattern_str = parser.GetString("pattern").value();
  auto start_str = parser.GetString("start").value_or("0x000000");
  auto end_str = parser.GetString("end").value_or(
      absl::StrFormat("0x%06X", static_cast<int>(rom->size())));

  // Parse addresses
  uint32_t start_address, end_address;
  try {
    start_address = std::stoul(start_str, nullptr, 16);
    end_address = std::stoul(end_str, nullptr, 16);
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid address format: %s", e.what()));
  }

  // Parse pattern
  std::vector<std::string> byte_strs = absl::StrSplit(pattern_str, ' ');
  std::vector<std::pair<uint8_t, bool>> pattern;  // (value, is_wildcard)

  for (const auto& byte_str : byte_strs) {
    if (byte_str.empty())
      continue;
    if (byte_str == "??" || byte_str == "**") {
      pattern.push_back({0, true});  // Wildcard
    } else {
      try {
        uint8_t value = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        pattern.push_back({value, false});
      } catch (const std::exception& e) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Invalid pattern byte '%s': %s", byte_str, e.what()));
      }
    }
  }

  if (pattern.empty()) {
    return absl::InvalidArgumentError("Empty pattern");
  }

  // Search for pattern
  std::vector<uint32_t> matches;
  const uint8_t* rom_data = rom->data();

  for (uint32_t i = start_address; i <= end_address - pattern.size(); ++i) {
    bool match = true;
    for (size_t j = 0; j < pattern.size(); ++j) {
      if (!pattern[j].second &&  // If not wildcard
          rom_data[i + j] != pattern[j].first) {
        match = false;
        break;
      }
    }
    if (match) {
      matches.push_back(i);
    }
  }

  formatter.BeginObject("Hex Search Results");
  formatter.AddField("pattern", pattern_str);
  formatter.AddHexField("start_address", start_address, 6);
  formatter.AddHexField("end_address", end_address, 6);
  formatter.AddField("matches_found", static_cast<int>(matches.size()));

  formatter.BeginArray("matches");
  for (uint32_t match : matches) {
    formatter.AddArrayItem(absl::StrFormat("0x%06X", match));
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
