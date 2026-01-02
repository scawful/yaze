#include "cli/handlers/tools/hex_inspector_commands.h"

#include <iomanip>
#include <iostream>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cli/service/resources/command_context.h"
#include "rom/rom.h"

namespace yaze {
namespace cli {

namespace {

enum class AddressMode { kPc, kSnes };

std::string PcToSnesLoRom(int pc_addr) {
  int bank = pc_addr / 0x8000;
  int addr = (pc_addr % 0x8000) + 0x8000;
  return absl::StrFormat("%02X:%04X", bank, addr);
}

int SnesToPcLoRom(int bank, int addr) {
  if (addr < 0x8000)
    return -1;  // Invalid for ROM data in LoROM
  int pc_addr = ((bank & 0x7F) * 0x8000) + (addr - 0x8000);
  return pc_addr;
}

void PrintHexDump(const std::vector<uint8_t>& data, int offset, int size,
                  AddressMode mode,
                  [[maybe_unused]] resources::OutputFormatter& formatter) {
  std::string output;
  for (int i = 0; i < size; i += 16) {
    // Print address
    if (mode == AddressMode::kSnes) {
      output += PcToSnesLoRom(offset + i) + " ";
    }
    absl::StrAppend(&output, absl::StrFormat("%06X: ", offset + i));

    // Print hex values
    for (int j = 0; j < 16; ++j) {
      if (i + j < size) {
        absl::StrAppend(&output, absl::StrFormat("%02X ", data[i + j]));
      } else {
        absl::StrAppend(&output, "   ");
      }
    }

    absl::StrAppend(&output, " |");

    // Print ASCII values
    for (int j = 0; j < 16; ++j) {
      if (i + j < size) {
        unsigned char c = data[i + j];
        if (c >= 32 && c <= 126) {
          absl::StrAppend(&output, absl::StrFormat("%c", c));
        } else {
          absl::StrAppend(&output, ".");
        }
      }
    }
    absl::StrAppend(&output, "|\n");
  }
  // For visual hex dump, we print directly to stdout.
  // OutputFormatter is used for structured data (JSON/Text KV), which isn't suitable for a visual block.
  std::cout << output;
}

}  // namespace

absl::Status HexDumpCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (parser.GetPositional().empty()) {
    return absl::InvalidArgumentError("Missing ROM path.");
  }
  if (parser.GetPositional().size() < 2) {
    return absl::InvalidArgumentError("Missing offset.");
  }
  return absl::OkStatus();
}

absl::Status HexDumpCommandHandler::Execute(
    [[maybe_unused]] Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  std::string rom_path = parser.GetPositional()[0];
  std::string offset_str = parser.GetPositional()[1];
  int size = 256;  // Default size

  if (parser.GetPositional().size() >= 3) {
    if (!absl::SimpleAtoi(parser.GetPositional()[2], &size)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid size: %s", parser.GetPositional()[2]));
    }
  }

  AddressMode mode = AddressMode::kPc;
  if (auto mode_arg = parser.GetString("mode")) {
    if (*mode_arg == "snes") {
      mode = AddressMode::kSnes;
    } else if (*mode_arg != "pc") {
      return absl::InvalidArgumentError("Invalid mode: " + *mode_arg);
    }
  }

  int offset = 0;
  // Handle SNES address input (e.g., 00:8000)
  if (absl::StrContains(offset_str, ':')) {
    std::vector<std::string> parts = absl::StrSplit(offset_str, ':');
    if (parts.size() == 2) {
      int bank = 0;
      int addr = 0;
      if (absl::SimpleAtoi(parts[0], &bank) &&
          absl::SimpleAtoi(
              parts[1],
              &addr)) {  // This assumes decimal if no 0x, but usually hex for addresses
        // Let's try hex parsing for bank/addr
        try {
          bank = std::stoi(parts[0], nullptr, 16);
          addr = std::stoi(parts[1], nullptr, 16);
          offset = SnesToPcLoRom(bank, addr);
          if (offset == -1) {
            return absl::InvalidArgumentError(
                "Invalid LoROM SNES address (addr < 0x8000)");
          }
          // Auto-enable SNES mode if user provided SNES address
          if (!parser.GetString("mode")) {
            mode = AddressMode::kSnes;
          }
        } catch (...) {
          return absl::InvalidArgumentError(
              "Invalid SNES address format (expected HEX:HEX)");
        }
      }
    }
  } else if (offset_str.size() > 2 && offset_str.substr(0, 2) == "0x") {
    try {
      offset = std::stoi(offset_str, nullptr, 16);
    } catch (...) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid hex offset: %s", offset_str));
    }
  } else {
    if (!absl::SimpleAtoi(offset_str, &offset)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid offset: %s", offset_str));
    }
  }

  // Load ROM locally since RequiresRom() is false (to allow inspecting any file)
  Rom local_rom;
  auto status = local_rom.LoadFromFile(rom_path);
  if (!status.ok()) {
    return status;
  }

  if (offset < 0 || static_cast<size_t>(offset) >= local_rom.size()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Offset out of bounds. ROM size: %lu", local_rom.size()));
  }

  if (static_cast<size_t>(offset) + static_cast<size_t>(size) >
      local_rom.size()) {
    size = static_cast<int>(local_rom.size() - static_cast<size_t>(offset));
  }

  std::vector<uint8_t> buffer(size);
  const auto& rom_data = local_rom.vector();
  for (int i = 0; i < size; ++i) {
    buffer[i] = rom_data[offset + i];
  }

  PrintHexDump(buffer, offset, size, mode, formatter);
  return absl::OkStatus();
}

// =============================================================================
// HexCompareCommandHandler
// =============================================================================

absl::Status HexCompareCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (parser.GetPositional().size() < 2) {
    return absl::InvalidArgumentError(
        "Missing ROM paths. Usage: hex-compare <rom1> <rom2>");
  }
  return absl::OkStatus();
}

absl::Status HexCompareCommandHandler::Execute(
    [[maybe_unused]] Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  std::string rom1_path = parser.GetPositional()[0];
  std::string rom2_path = parser.GetPositional()[1];

  // Parse optional start/end offsets
  int start_offset = 0;
  int end_offset = -1;  // -1 means compare to end

  if (auto start_str = parser.GetString("start")) {
    if (start_str->size() > 2 && start_str->substr(0, 2) == "0x") {
      start_offset = std::stoi(*start_str, nullptr, 16);
    } else {
      if (!absl::SimpleAtoi(*start_str, &start_offset)) {
        return absl::InvalidArgumentError("Invalid start offset: " +
                                          *start_str);
      }
    }
  }

  if (auto end_str = parser.GetString("end")) {
    if (end_str->size() > 2 && end_str->substr(0, 2) == "0x") {
      end_offset = std::stoi(*end_str, nullptr, 16);
    } else {
      if (!absl::SimpleAtoi(*end_str, &end_offset)) {
        return absl::InvalidArgumentError("Invalid end offset: " + *end_str);
      }
    }
  }

  AddressMode mode = AddressMode::kPc;
  if (auto mode_arg = parser.GetString("mode")) {
    if (*mode_arg == "snes") {
      mode = AddressMode::kSnes;
    }
  }

  // Load both ROMs
  Rom rom1, rom2;
  auto status1 = rom1.LoadFromFile(rom1_path);
  if (!status1.ok()) {
    return absl::InvalidArgumentError("Failed to load ROM 1: " +
                                      std::string(status1.message()));
  }

  auto status2 = rom2.LoadFromFile(rom2_path);
  if (!status2.ok()) {
    return absl::InvalidArgumentError("Failed to load ROM 2: " +
                                      std::string(status2.message()));
  }

  // Determine comparison range
  size_t compare_size = std::min(rom1.size(), rom2.size());
  if (end_offset >= 0 && static_cast<size_t>(end_offset) < compare_size) {
    compare_size = end_offset;
  }

  if (static_cast<size_t>(start_offset) >= compare_size) {
    return absl::InvalidArgumentError("Start offset beyond ROM size");
  }

  // Compare bytes
  std::vector<uint32_t> diff_offsets;
  int total_diffs = 0;
  const int max_diff_display = 20;

  const auto& data1 = rom1.vector();
  const auto& data2 = rom2.vector();

  for (size_t i = start_offset; i < compare_size; ++i) {
    if (data1[i] != data2[i]) {
      total_diffs++;
      if (diff_offsets.size() < max_diff_display) {
        diff_offsets.push_back(static_cast<uint32_t>(i));
      }
    }
  }

  // Output results
  formatter.AddField("rom1_path", rom1_path);
  formatter.AddField("rom1_size", static_cast<int>(rom1.size()));
  formatter.AddField("rom2_path", rom2_path);
  formatter.AddField("rom2_size", static_cast<int>(rom2.size()));
  formatter.AddField("compare_start", start_offset);
  formatter.AddField("compare_end", static_cast<int>(compare_size));
  formatter.AddField("total_differences", total_diffs);
  formatter.AddField("sizes_match", rom1.size() == rom2.size());

  // Output first N differences
  if (!diff_offsets.empty() && !formatter.IsJson()) {
    std::cout << "\n=== Hex Compare Results ===\n";
    std::cout << absl::StrFormat("ROM 1: %s (%zu bytes)\n", rom1_path,
                                 rom1.size());
    std::cout << absl::StrFormat("ROM 2: %s (%zu bytes)\n", rom2_path,
                                 rom2.size());
    std::cout << absl::StrFormat("Range: 0x%06X - 0x%06zX\n", start_offset,
                                 compare_size);
    std::cout << absl::StrFormat("Total differences: %d\n\n", total_diffs);

    std::cout << "First differences:\n";
    std::cout << "  Offset     ROM1  ROM2\n";
    std::cout << "  ------     ----  ----\n";
    for (uint32_t offset : diff_offsets) {
      std::string addr_str;
      if (mode == AddressMode::kSnes) {
        addr_str = PcToSnesLoRom(offset) + " ";
      }
      std::cout << absl::StrFormat("  %s0x%06X:  0x%02X  0x%02X\n", addr_str,
                                   offset, data1[offset], data2[offset]);
    }
    if (total_diffs > max_diff_display) {
      std::cout << absl::StrFormat("  ... and %d more differences\n",
                                   total_diffs - max_diff_display);
    }
    std::cout << "\n";
  }

  return absl::OkStatus();
}

// =============================================================================
// HexAnnotateCommandHandler
// =============================================================================

namespace {

struct AnnotatedField {
  std::string name;
  int offset;
  int size;
  std::string format;  // "hex", "decimal", "flags", "enum"
};

struct DataStructure {
  std::string name;
  int total_size;
  std::vector<AnnotatedField> fields;
};

// SNES ROM Header structure at 0x7FC0
const DataStructure kSnesHeaderStructure = {
    "SNES ROM Header",
    32,
    {
        {"Title", 0, 21, "ascii"},
        {"Map Mode", 21, 1, "hex"},
        {"ROM Type", 22, 1, "hex"},
        {"ROM Size", 23, 1, "decimal"},
        {"SRAM Size", 24, 1, "decimal"},
        {"Country Code", 25, 1, "hex"},
        {"License Code", 26, 1, "hex"},
        {"Version", 27, 1, "decimal"},
        {"Checksum Complement", 28, 2, "hex"},
        {"Checksum", 30, 2, "hex"},
    }};

// Dungeon room header structure
const DataStructure kRoomHeaderStructure = {
    "Room Header",
    14,
    {
        {"BG2 Property", 0, 1, "flags"},
        {"Collision/Effect", 1, 1, "hex"},
        {"Light/Dark", 2, 1, "flags"},
        {"Palette", 3, 1, "decimal"},
        {"Blockset", 4, 1, "decimal"},
        {"Enemy Blockset", 5, 1, "decimal"},
        {"Effect", 6, 1, "hex"},
        {"Tag1", 7, 1, "hex"},
        {"Tag2", 8, 1, "hex"},
        {"Floor1", 9, 1, "hex"},
        {"Floor2", 10, 1, "hex"},
        {"Planes1", 11, 1, "hex"},
        {"Planes2", 12, 1, "hex"},
        {"Message ID", 13, 1, "decimal"},
    }};

// Sprite entry structure (3 bytes)
const DataStructure kSpriteStructure = {"Sprite Entry",
                                        3,
                                        {
                                            {"Y Position", 0, 1, "decimal"},
                                            {"X/Subtype", 1, 1, "hex"},
                                            {"Sprite ID", 2, 1, "hex"},
                                        }};

// Tile16 entry structure (8 bytes - 4 tiles)
const DataStructure kTile16Structure = {"Tile16 Entry",
                                        8,
                                        {
                                            {"Tile TL", 0, 2, "hex"},
                                            {"Tile TR", 2, 2, "hex"},
                                            {"Tile BL", 4, 2, "hex"},
                                            {"Tile BR", 6, 2, "hex"},
                                        }};

void PrintAnnotatedStructure(const DataStructure& structure,
                             const std::vector<uint8_t>& data, int offset) {
  std::cout << "\n=== " << structure.name << " ===\n";
  std::cout << absl::StrFormat("Offset: 0x%06X, Size: %d bytes\n\n", offset,
                               structure.total_size);

  for (const auto& field : structure.fields) {
    std::cout << absl::StrFormat("  +%02X %-20s: ", field.offset, field.name);

    if (field.format == "ascii") {
      std::string ascii_val;
      for (int i = 0; i < field.size && field.offset + i < (int)data.size();
           ++i) {
        char c = static_cast<char>(data[field.offset + i]);
        if (c >= 32 && c < 127) {
          ascii_val += c;
        }
      }
      std::cout << "\"" << ascii_val << "\"\n";
    } else if (field.format == "hex") {
      if (field.size == 1 && field.offset < (int)data.size()) {
        std::cout << absl::StrFormat("0x%02X\n", data[field.offset]);
      } else if (field.size == 2 && field.offset + 1 < (int)data.size()) {
        uint16_t val = data[field.offset] | (data[field.offset + 1] << 8);
        std::cout << absl::StrFormat("0x%04X\n", val);
      } else {
        std::cout << "?\n";
      }
    } else if (field.format == "decimal") {
      if (field.offset < (int)data.size()) {
        std::cout << static_cast<int>(data[field.offset]) << "\n";
      } else {
        std::cout << "?\n";
      }
    } else if (field.format == "flags") {
      if (field.offset < (int)data.size()) {
        uint8_t val = data[field.offset];
        std::cout << absl::StrFormat("0x%02X (", val);
        for (int b = 7; b >= 0; --b) {
          std::cout << ((val & (1 << b)) ? "1" : "0");
        }
        std::cout << ")\n";
      } else {
        std::cout << "?\n";
      }
    } else {
      std::cout << "?\n";
    }
  }
  std::cout << "\n";
}

}  // namespace

absl::Status HexAnnotateCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (parser.GetPositional().empty()) {
    return absl::InvalidArgumentError("Missing ROM path.");
  }
  if (parser.GetPositional().size() < 2) {
    return absl::InvalidArgumentError("Missing offset.");
  }
  return absl::OkStatus();
}

absl::Status HexAnnotateCommandHandler::Execute(
    [[maybe_unused]] Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  std::string rom_path = parser.GetPositional()[0];
  std::string offset_str = parser.GetPositional()[1];

  // Parse offset
  int offset = 0;
  if (offset_str.size() > 2 && offset_str.substr(0, 2) == "0x") {
    try {
      offset = std::stoi(offset_str, nullptr, 16);
    } catch (...) {
      return absl::InvalidArgumentError("Invalid hex offset: " + offset_str);
    }
  } else if (absl::StrContains(offset_str, ':')) {
    // SNES address format BB:AAAA
    std::vector<std::string> parts = absl::StrSplit(offset_str, ':');
    if (parts.size() == 2) {
      try {
        int bank = std::stoi(parts[0], nullptr, 16);
        int addr = std::stoi(parts[1], nullptr, 16);
        offset = SnesToPcLoRom(bank, addr);
        if (offset == -1) {
          return absl::InvalidArgumentError(
              "Invalid LoROM SNES address (addr < 0x8000)");
        }
      } catch (...) {
        return absl::InvalidArgumentError(
            "Invalid SNES address format (expected HEX:HEX)");
      }
    }
  } else {
    if (!absl::SimpleAtoi(offset_str, &offset)) {
      return absl::InvalidArgumentError("Invalid offset: " + offset_str);
    }
  }

  // Get structure type
  std::string type = "auto";
  if (auto type_arg = parser.GetString("type")) {
    type = *type_arg;
  }

  // Load ROM
  Rom local_rom;
  auto status = local_rom.LoadFromFile(rom_path);
  if (!status.ok()) {
    return status;
  }

  if (offset < 0 || static_cast<size_t>(offset) >= local_rom.size()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Offset out of bounds. ROM size: %lu", local_rom.size()));
  }

  // Auto-detect structure type if needed
  if (type == "auto") {
    if (offset == 0x7FC0 || (offset >= 0x7FC0 && offset < 0x7FE0)) {
      type = "snes_header";
    } else if (offset >= 0x0 && offset < 0x10000) {
      // Likely code/data region - default to room header for dungeon ROM offsets
      type = "room_header";
    } else {
      type = "room_header";  // Default
    }
  }

  // Select structure
  const DataStructure* structure = nullptr;
  if (type == "snes_header") {
    structure = &kSnesHeaderStructure;
    if (offset != 0x7FC0) {
      offset = 0x7FC0;  // Force to header location
    }
  } else if (type == "room_header") {
    structure = &kRoomHeaderStructure;
  } else if (type == "sprite") {
    structure = &kSpriteStructure;
  } else if (type == "tile16") {
    structure = &kTile16Structure;
  } else {
    return absl::InvalidArgumentError("Unknown structure type: " + type);
  }

  // Read data
  int read_size = std::min(structure->total_size,
                           static_cast<int>(local_rom.size() - offset));
  std::vector<uint8_t> buffer(read_size);
  const auto& rom_data = local_rom.vector();
  for (int i = 0; i < read_size; ++i) {
    buffer[i] = rom_data[offset + i];
  }

  // Output
  formatter.AddField("rom_path", rom_path);
  formatter.AddHexField("offset", offset, 6);
  formatter.AddField("structure_type", type);
  formatter.AddField("structure_name", structure->name);
  formatter.AddField("structure_size", structure->total_size);

  if (!formatter.IsJson()) {
    PrintAnnotatedStructure(*structure, buffer, offset);
  } else {
    // JSON output with field values
    formatter.BeginArray("fields");
    for (const auto& field : structure->fields) {
      std::string value;
      if (field.format == "hex" && field.size == 1 &&
          field.offset < (int)buffer.size()) {
        value = absl::StrFormat("0x%02X", buffer[field.offset]);
      } else if (field.format == "hex" && field.size == 2 &&
                 field.offset + 1 < (int)buffer.size()) {
        uint16_t val = buffer[field.offset] | (buffer[field.offset + 1] << 8);
        value = absl::StrFormat("0x%04X", val);
      } else if (field.format == "decimal" &&
                 field.offset < (int)buffer.size()) {
        value = std::to_string(buffer[field.offset]);
      } else if (field.format == "ascii") {
        for (int i = 0; i < field.size && field.offset + i < (int)buffer.size();
             ++i) {
          char c = static_cast<char>(buffer[field.offset + i]);
          if (c >= 32 && c < 127)
            value += c;
        }
      } else if (field.format == "flags" && field.offset < (int)buffer.size()) {
        value = absl::StrFormat("0x%02X", buffer[field.offset]);
      }

      formatter.AddArrayItem(absl::StrFormat(
          R"({"name":"%s","offset":%d,"size":%d,"format":"%s","value":"%s"})",
          field.name, field.offset, field.size, field.format, value));
    }
    formatter.EndArray();
  }

  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
