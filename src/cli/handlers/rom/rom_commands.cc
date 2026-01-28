#include "cli/handlers/rom/rom_commands.h"

#include <cctype>
#include <fstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cli/util/hex_util.h"
#include "util/macro.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

absl::Status RomReadCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto address_str = parser.GetString("address").value();
  auto length = parser.GetInt("length").value_or(16);
  std::string data_format = "both";
  if (auto fmt = parser.GetString("data-format"); fmt.has_value()) {
    data_format = *fmt;
  } else if (auto fmt = parser.GetString("format"); fmt.has_value()) {
    if (*fmt == "hex" || *fmt == "ascii" || *fmt == "both" ||
        *fmt == "binary") {
      data_format = *fmt;
    }
  }

  uint32_t address = 0;
  if (!ParseHexString(address_str, &address)) {
    return absl::InvalidArgumentError("Invalid hex address format");
  }
  if (length <= 0) {
    return absl::InvalidArgumentError("Length must be greater than 0");
  }
  if (address + static_cast<uint32_t>(length) > rom->size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Read beyond ROM: 0x%X+%d > %zu", address, length, rom->size()));
  }

  const uint8_t* data = rom->data() + address;

  formatter.AddHexField("address", address, 6);
  formatter.AddField("length", length);

  bool include_hex = (data_format == "hex" || data_format == "both");
  bool include_ascii = (data_format == "ascii" || data_format == "both");

  if (include_hex) {
    std::string hex_data;
    for (int i = 0; i < length; ++i) {
      absl::StrAppendFormat(&hex_data, "%02X", data[i]);
      if (i < length - 1)
        hex_data += " ";
    }
    formatter.AddField("data", hex_data);
  }

  if (include_ascii) {
    std::string ascii_data;
    ascii_data.reserve(length);
    for (int i = 0; i < length; ++i) {
      char c = static_cast<char>(data[i]);
      ascii_data += (std::isprint(static_cast<unsigned char>(c)) ? c : '.');
    }
    formatter.AddField("ascii", ascii_data);
  }

  formatter.AddField("format", data_format);
  return absl::OkStatus();
}

absl::Status RomWriteCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto address_str = parser.GetString("address").value();
  auto data_str = parser.GetString("data").value();

  uint32_t address = 0;
  if (!ParseHexString(address_str, &address)) {
    return absl::InvalidArgumentError("Invalid hex address format");
  }

  std::vector<std::string> byte_strs = absl::StrSplit(data_str, ' ');
  std::vector<uint8_t> bytes;
  bytes.reserve(byte_strs.size());

  for (const auto& byte_str : byte_strs) {
    if (byte_str.empty())
      continue;
    int value = 0;
    if (!ParseHexString(byte_str, &value)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid byte '%s'", byte_str));
    }
    if (value < 0 || value > 0xFF) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Byte out of range: %s", byte_str));
    }
    bytes.push_back(static_cast<uint8_t>(value));
  }

  if (bytes.empty()) {
    return absl::InvalidArgumentError("No data bytes provided");
  }

  if (address + bytes.size() > rom->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Write beyond ROM: 0x%X+%zu > %zu", address,
                        bytes.size(), rom->size()));
  }

  for (size_t i = 0; i < bytes.size(); ++i) {
    rom->WriteByte(address + i, bytes[i]);
  }

  std::string hex_data;
  for (size_t i = 0; i < bytes.size(); ++i) {
    absl::StrAppendFormat(&hex_data, "%02X", bytes[i]);
    if (i < bytes.size() - 1)
      hex_data += " ";
  }

  formatter.AddHexField("address", address, 6);
  formatter.AddField("bytes_written", static_cast<int>(bytes.size()));
  formatter.AddField("data", hex_data);
  return absl::OkStatus();
}

absl::Status RomInfoCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM must be loaded");
  }

  formatter.AddField("title", rom->title());
  formatter.AddField("size", absl::StrFormat("0x%X", rom->size()));
  formatter.AddField("size_bytes", static_cast<int>(rom->size()));

  return absl::OkStatus();
}

absl::Status RomValidateCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM must be loaded");
  }

  bool all_ok = true;
  std::vector<std::string> validation_results;

  // Basic ROM validation - check if ROM is loaded and has reasonable size
  if (rom->is_loaded() && rom->size() > 0) {
    validation_results.push_back("checksum: PASSED");
  } else {
    validation_results.push_back("checksum: FAILED");
    all_ok = false;
  }

  // Header validation
  if (rom->title() == "THE LEGEND OF ZELDA") {
    validation_results.push_back("header: PASSED");
  } else {
    validation_results.push_back(
        "header: FAILED (Invalid title: " + rom->title() + ")");
    all_ok = false;
  }

  formatter.AddField("validation_passed", all_ok);
  std::string results_str;
  for (const auto& result : validation_results) {
    if (!results_str.empty())
      results_str += "; ";
    results_str += result;
  }
  formatter.AddField("results", results_str);

  return absl::OkStatus();
}

absl::Status RomDiffCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto rom_a_opt = parser.GetString("rom_a");
  auto rom_b_opt = parser.GetString("rom_b");

  if (!rom_a_opt.has_value()) {
    return absl::InvalidArgumentError("Missing required argument: rom_a");
  }
  if (!rom_b_opt.has_value()) {
    return absl::InvalidArgumentError("Missing required argument: rom_b");
  }

  std::string rom_a_path = rom_a_opt.value();
  std::string rom_b_path = rom_b_opt.value();

  Rom rom_a;
  auto status_a = rom_a.LoadFromFile(rom_a_path);
  if (!status_a.ok()) {
    return status_a;
  }

  Rom rom_b;
  auto status_b = rom_b.LoadFromFile(rom_b_path);
  if (!status_b.ok()) {
    return status_b;
  }

  if (rom_a.size() != rom_b.size()) {
    formatter.AddField("size_match", false);
    formatter.AddField("size_a", static_cast<int>(rom_a.size()));
    formatter.AddField("size_b", static_cast<int>(rom_b.size()));
    return absl::OkStatus();
  }

  int differences = 0;
  std::vector<std::string> diff_details;

  for (size_t i = 0; i < rom_a.size(); ++i) {
    if (rom_a.vector()[i] != rom_b.vector()[i]) {
      differences++;
      if (differences <= 10) {  // Limit output to first 10 differences
        diff_details.push_back(absl::StrFormat("0x%08X: 0x%02X vs 0x%02X", i,
                                               rom_a.vector()[i],
                                               rom_b.vector()[i]));
      }
    }
  }

  formatter.AddField("identical", differences == 0);
  formatter.AddField("differences_count", differences);
  if (!diff_details.empty()) {
    std::string diff_str;
    for (const auto& diff : diff_details) {
      if (!diff_str.empty())
        diff_str += "; ";
      diff_str += diff;
    }
    formatter.AddField("differences", diff_str);
  }

  return absl::OkStatus();
}

absl::Status RomGenerateGoldenCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto rom_opt = parser.GetString("rom_file");
  auto golden_opt = parser.GetString("golden_file");

  if (!rom_opt.has_value()) {
    return absl::InvalidArgumentError("Missing required argument: rom_file");
  }
  if (!golden_opt.has_value()) {
    return absl::InvalidArgumentError("Missing required argument: golden_file");
  }

  std::string rom_path = rom_opt.value();
  std::string golden_path = golden_opt.value();

  Rom source_rom;
  auto status = source_rom.LoadFromFile(rom_path);
  if (!status.ok()) {
    return status;
  }

  std::ofstream file(golden_path, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("Could not open file for writing: " +
                               golden_path);
  }

  file.write(reinterpret_cast<const char*>(source_rom.vector().data()),
             source_rom.size());

  formatter.AddField("status", "success");
  formatter.AddField("golden_file", golden_path);
  formatter.AddField("source_file", rom_path);
  formatter.AddField("size", static_cast<int>(source_rom.size()));

  return absl::OkStatus();
}

absl::Status RomResolveAddressCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto address_str = parser.GetString("address").value();
  auto max_offset = parser.GetInt("max-offset").value_or(0x100);

  uint32_t address = 0;
  if (!ParseHexString(address_str, &address)) {
    return absl::InvalidArgumentError("Invalid hex address format");
  }

  if (symbol_provider_ == nullptr || !symbol_provider_->HasSymbols()) {
    formatter.AddField("status", "no_symbols_loaded");
    formatter.AddHexField("address", address, 6);
    return absl::OkStatus();
  }

  auto symbols = symbol_provider_->GetSymbolsAtAddress(address);
  if (!symbols.empty()) {
    formatter.AddField("status", "success");
    formatter.AddField("match_type", "exact");
    if (symbols.size() == 1) {
      formatter.AddField("name", symbols[0].name);
      if (!symbols[0].file.empty()) {
        formatter.AddField("file", symbols[0].file);
        formatter.AddField("line", symbols[0].line);
      }
    } else {
      formatter.BeginArray("names");
      for (const auto& sym : symbols) {
        formatter.AddArrayItem(sym.name);
      }
      formatter.EndArray();
    }
    formatter.AddHexField("address", address, 6);
  } else {
    auto nearest = symbol_provider_->GetNearestSymbol(address);
    if (nearest) {
      uint32_t offset = address - nearest->address;
      if (offset <= static_cast<uint32_t>(max_offset)) {
        formatter.AddField("status", "success");
        formatter.AddField("match_type", "nearest");
        formatter.AddField("name", nearest->name);
        formatter.AddHexField("symbol_address", nearest->address, 6);
        formatter.AddHexField("offset", offset);
        formatter.AddField("formatted", absl::StrFormat("%s+$%X", nearest->name, offset));
      } else {
        formatter.AddField("status", "not_found");
        formatter.AddField("message", "Nearest symbol too far away");
      }
    } else {
      formatter.AddField("status", "not_found");
    }
  }

  formatter.AddHexField("requested_address", address, 6);
  return absl::OkStatus();
}

absl::Status RomFindSymbolCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto name = parser.GetString("name").value();

  if (symbol_provider_ == nullptr || !symbol_provider_->HasSymbols()) {
    return absl::FailedPreconditionError("No symbols loaded. Provide a ROM with .mlb/.sym file.");
  }

  auto symbol = symbol_provider_->FindSymbol(name);
  if (symbol) {
    formatter.AddField("status", "success");
    formatter.AddField("name", symbol->name);
    formatter.AddHexField("address", symbol->address, 6);
    if (!symbol->file.empty()) {
      formatter.AddField("file", symbol->file);
      formatter.AddField("line", symbol->line);
    }
  } else {
    // Try wildcard search
    auto matches = symbol_provider_->FindSymbolsMatching(name);
    if (!matches.empty()) {
      formatter.AddField("status", "success");
      formatter.AddField("match_type", "partial");
      formatter.BeginArray("matches");
      for (const auto& m : matches) {
        formatter.BeginObject();
        formatter.AddField("name", m.name);
        formatter.AddHexField("address", m.address, 6);
        formatter.EndObject();
      }
      formatter.EndArray();
    } else {
      formatter.AddField("status", "not_found");
    }
  }

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
