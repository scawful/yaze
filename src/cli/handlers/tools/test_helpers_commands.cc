/**
 * @file test_helpers_commands.cc
 * @brief Implementation of CLI command handlers for test helper tools
 */

#include "cli/handlers/tools/test_helpers_commands.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "app/emu/snes.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze {
namespace cli {
namespace handlers {

using namespace zelda3;

// =============================================================================
// ToolsHarnessStateCommandHandler
// =============================================================================

absl::Status ToolsHarnessStateCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  std::string rom_path = parser.GetString("rom").value_or("");
  std::string output_path = parser.GetString("output").value_or("");

  auto status = GenerateHarnessState(rom_path, output_path);
  if (!status.ok()) {
    return status;
  }

  formatter.AddField("status", "success");
  formatter.AddField("output_file", output_path);
  formatter.AddField("message",
                     "Successfully generated harness state from " + rom_path);
  return absl::OkStatus();
}

absl::Status ToolsHarnessStateCommandHandler::GenerateHarnessState(
    const std::string& rom_path, const std::string& output_path) {
  // Load ROM
  Rom rom;
  RETURN_IF_ERROR(rom.LoadFromFile(rom_path));
  auto rom_data = rom.vector();

  // Initialize SNES
  emu::Snes snes;
  snes.Init(rom_data);
  snes.Reset(false);

  auto& cpu = snes.cpu();
  auto& ppu = snes.ppu();

  // Run emulator until the main game loop is reached
  int max_cycles = 15000000;  // 15 million cycles
  int cycles = 0;
  while (cycles < max_cycles) {
    snes.RunCycle();
    cycles++;
    if (cpu.PB == 0x00 && cpu.PC == 0x8034) {
      break;  // Reached MainGameLoop
    }
  }

  if (cycles >= max_cycles) {
    return absl::InternalError(
        "Emulator timed out; did not reach main game loop.");
  }

  std::ofstream out_file(output_path);
  if (!out_file.is_open()) {
    return absl::InternalError("Failed to open output file: " + output_path);
  }

  // Write header
  out_file << "// ================================================"
              "=========================\n";
  out_file << "// YAZE Dungeon Test Harness State - Generated from: "
           << rom_path << "\n";
  out_file << "// ================================================"
              "=========================\n\n";
  out_file << "#pragma once\n\n";
  out_file << "#include <cstdint>\n";
  out_file << "#include <array>\n\n";
  out_file << "namespace yaze {\n";
  out_file << "namespace emu {\n\n";

  // Write WRAM state
  out_file << "constexpr std::array<uint8_t, 0x20000> kInitialWRAMState = {{\n";
  for (int i = 0; i < 0x20000; ++i) {
    if (i % 16 == 0) out_file << "    ";
    out_file << "0x" << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>(snes.Read(0x7E0000 + i));
    if (i < 0x1FFFF) out_file << ", ";
    if (i % 16 == 15) out_file << "\n";
  }
  out_file << "}};\n\n";

  // Write CPU/PPU register state
  out_file << "// ================================================"
              "=========================\n";
  out_file << "// Initial Register States\n";
  out_file << "// ================================================"
              "=========================\n\n";

  out_file << "struct InitialPpuState {\n";
  out_file << "    uint8_t inidisp = 0x" << std::hex << ppu.Read(0x2100, false)
           << ";\n";
  out_file << "    uint8_t objsel = 0x" << std::hex << ppu.Read(0x2101, false)
           << ";\n";
  out_file << "    uint8_t bgmode = 0x" << std::hex << ppu.Read(0x2105, false)
           << ";\n";
  out_file << "    uint8_t mosaic = 0x" << std::hex << ppu.Read(0x2106, false)
           << ";\n";
  out_file << "    uint8_t tm = 0x" << std::hex << ppu.Read(0x212C, false)
           << ";\n";
  out_file << "    uint8_t ts = 0x" << std::hex << ppu.Read(0x212D, false)
           << ";\n";
  out_file << "    uint8_t cgwsel = 0x" << std::hex << ppu.Read(0x2130, false)
           << ";\n";
  out_file << "    uint8_t cgadsub = 0x" << std::hex << ppu.Read(0x2131, false)
           << ";\n";
  out_file << "    uint8_t setini = 0x" << std::hex << ppu.Read(0x2133, false)
           << ";\n";
  out_file << "};\n\n";

  out_file << "}  // namespace emu\n";
  out_file << "}  // namespace yaze\n";

  return absl::OkStatus();
}

// =============================================================================
// ToolsExtractValuesCommandHandler
// =============================================================================

absl::Status ToolsExtractValuesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  std::string rom_path = parser.GetString("rom").value_or("");
  std::string format = parser.GetString("format").value_or("cpp");

  // Load the ROM if not already loaded
  if (!rom || rom->size() == 0) {
    Rom loaded_rom;
    RETURN_IF_ERROR(loaded_rom.LoadFromFile(rom_path));
    rom = &loaded_rom;
  }

  std::ostringstream output;

  if (format == "json") {
    output << "{\n";
    output << "  \"asm_version\": \"0x" << std::hex << std::setw(2)
           << std::setfill('0')
           << static_cast<int>((*rom)[OverworldCustomASMHasBeenApplied])
           << "\",\n";
    output << "  \"area_graphics\": [";
    for (int i = 0; i < 10; i++) {
      output << "\"0x" << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>((*rom)[kAreaGfxIdPtr + i]) << "\"";
      if (i < 9) output << ", ";
    }
    output << "],\n";
    output << "  \"area_palettes\": [";
    for (int i = 0; i < 10; i++) {
      output << "\"0x" << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>((*rom)[kOverworldMapPaletteIds + i]) << "\"";
      if (i < 9) output << ", ";
    }
    output << "],\n";
    output << "  \"screen_sizes\": [";
    for (int i = 0; i < 10; i++) {
      output << "\"0x" << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>((*rom)[kOverworldScreenSize + i]) << "\"";
      if (i < 9) output << ", ";
    }
    output << "]\n";
    output << "}\n";
  } else {
    // C++ format
    output << "// Vanilla ROM values extracted\n\n";

    output << "constexpr uint8_t kVanillaASMVersion = 0x" << std::hex
           << std::setw(2) << std::setfill('0')
           << static_cast<int>((*rom)[OverworldCustomASMHasBeenApplied])
           << ";\n\n";

    output << "// Area graphics for first 10 maps\n";
    for (int i = 0; i < 10; i++) {
      output << "constexpr uint8_t kVanillaAreaGraphics" << std::dec << i
             << " = 0x" << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>((*rom)[kAreaGfxIdPtr + i]) << ";\n";
    }
    output << "\n";

    output << "// Area palettes for first 10 maps\n";
    for (int i = 0; i < 10; i++) {
      output << "constexpr uint8_t kVanillaAreaPalette" << std::dec << i
             << " = 0x" << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>((*rom)[kOverworldMapPaletteIds + i]) << ";\n";
    }
    output << "\n";

    output << "// Screen sizes for first 10 maps\n";
    for (int i = 0; i < 10; i++) {
      output << "constexpr uint8_t kVanillaScreenSize" << std::dec << i
             << " = 0x" << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>((*rom)[kOverworldScreenSize + i]) << ";\n";
    }
    output << "\n";

    output << "// Sprite sets for first 10 maps\n";
    for (int i = 0; i < 10; i++) {
      output << "constexpr uint8_t kVanillaSpriteSet" << std::dec << i
             << " = 0x" << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>((*rom)[kOverworldSpriteset + i]) << ";\n";
    }
  }

  std::cout << output.str();
  formatter.AddField("status", "success");
  return absl::OkStatus();
}

// =============================================================================
// ToolsExtractGoldenCommandHandler
// =============================================================================

absl::Status ToolsExtractGoldenCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  std::string rom_path = parser.GetString("rom").value_or("");
  std::string output_path = parser.GetString("output").value_or("");

  // Load ROM
  Rom loaded_rom;
  RETURN_IF_ERROR(loaded_rom.LoadFromFile(rom_path));

  // Load overworld data
  Overworld overworld(&loaded_rom);
  RETURN_IF_ERROR(overworld.Load(&loaded_rom));

  std::ofstream out_file(output_path);
  if (!out_file.is_open()) {
    return absl::InternalError("Failed to open output file: " + output_path);
  }

  WriteHeader(out_file, rom_path);
  WriteBasicROMInfo(out_file, loaded_rom);
  WriteASMVersionInfo(out_file, loaded_rom);
  WriteOverworldMapsData(out_file, overworld);
  WriteTileData(out_file, overworld);
  WriteEntranceData(out_file, overworld);
  WriteFooter(out_file);

  formatter.AddField("status", "success");
  formatter.AddField("output_file", output_path);
  formatter.AddField("message",
                     "Successfully extracted golden data from " + rom_path);
  return absl::OkStatus();
}

void ToolsExtractGoldenCommandHandler::WriteHeader(std::ofstream& out,
                                                   const std::string& rom_path) {
  out << "// =============================================="
         "===========================\n";
  out << "// YAZE Overworld Golden Data - Generated from: " << rom_path << "\n";
  out << "// =============================================="
         "===========================\n\n";
  out << "#pragma once\n\n";
  out << "#include <cstdint>\n";
  out << "#include <array>\n";
  out << "#include <vector>\n";
  out << "#include \"zelda3/overworld/overworld_map.h\"\n\n";
  out << "namespace yaze {\n";
  out << "namespace test {\n\n";
}

void ToolsExtractGoldenCommandHandler::WriteBasicROMInfo(std::ofstream& out,
                                                         Rom& rom) {
  out << "// =============================================="
         "===========================\n";
  out << "// Basic ROM Information\n";
  out << "// =============================================="
         "===========================\n\n";

  out << "constexpr std::string_view kGoldenROMTitle = \"" << rom.title()
      << "\";\n";
  out << "constexpr size_t kGoldenROMSize = " << std::dec << rom.size()
      << ";\n\n";
}

void ToolsExtractGoldenCommandHandler::WriteASMVersionInfo(std::ofstream& out,
                                                           Rom& rom) {
  out << "// =============================================="
         "===========================\n";
  out << "// ASM Version Information\n";
  out << "// =============================================="
         "===========================\n\n";

  auto asm_version = rom.ReadByte(0x140145);
  if (asm_version.ok()) {
    out << "constexpr uint8_t kGoldenASMVersion = 0x" << std::hex << std::setw(2)
        << std::setfill('0') << static_cast<int>(*asm_version) << ";\n";

    if (*asm_version == 0xFF) {
      out << "constexpr bool kGoldenIsVanillaROM = true;\n";
      out << "constexpr bool kGoldenHasZSCustomOverworld = false;\n";
    } else {
      out << "constexpr bool kGoldenIsVanillaROM = false;\n";
      out << "constexpr bool kGoldenHasZSCustomOverworld = true;\n";
      out << "constexpr uint8_t kGoldenZSCustomOverworldVersion = "
          << std::dec << static_cast<int>(*asm_version) << ";\n";
    }
    out << "\n";
  }
}

void ToolsExtractGoldenCommandHandler::WriteOverworldMapsData(
    std::ofstream& out, Overworld& overworld) {
  out << "// =============================================="
         "===========================\n";
  out << "// Overworld Maps Data\n";
  out << "// =============================================="
         "===========================\n\n";

  const auto& maps = overworld.overworld_maps();
  out << "constexpr size_t kGoldenNumOverworldMaps = " << std::dec
      << maps.size() << ";\n\n";

  out << "// Map properties for first 20 maps\n";
  out << "constexpr std::array<uint8_t, 20> kGoldenMapAreaGraphics = {{\n";
  for (int i = 0; i < std::min(20, static_cast<int>(maps.size())); i++) {
    out << "    0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(maps[i].area_graphics());
    if (i < 19) out << ",";
    out << "  // Map " << std::dec << i << "\n";
  }
  out << "}};\n\n";
}

void ToolsExtractGoldenCommandHandler::WriteTileData(std::ofstream& out,
                                                     Overworld& overworld) {
  out << "// =============================================="
         "===========================\n";
  out << "// Tile Data Information\n";
  out << "// =============================================="
         "===========================\n\n";

  out << "constexpr bool kGoldenExpandedTile16 = "
      << (overworld.expanded_tile16() ? "true" : "false") << ";\n";
  out << "constexpr bool kGoldenExpandedTile32 = "
      << (overworld.expanded_tile32() ? "true" : "false") << ";\n\n";

  const auto& tiles16 = overworld.tiles16();
  const auto& tiles32 = overworld.tiles32_unique();

  out << "constexpr size_t kGoldenNumTiles16 = " << std::dec << tiles16.size()
      << ";\n";
  out << "constexpr size_t kGoldenNumTiles32 = " << std::dec << tiles32.size()
      << ";\n\n";
}

void ToolsExtractGoldenCommandHandler::WriteEntranceData(
    std::ofstream& out, Overworld& overworld) {
  out << "// =============================================="
         "===========================\n";
  out << "// Entrance Data\n";
  out << "// =============================================="
         "===========================\n\n";

  const auto& entrances = overworld.entrances();
  out << "constexpr size_t kGoldenNumEntrances = " << std::dec
      << entrances.size() << ";\n\n";

  out << "// Sample entrance data (first 10 entrances)\n";
  out << "constexpr std::array<uint16_t, 10> kGoldenEntranceMapPos = {{\n";
  for (int i = 0; i < std::min(10, static_cast<int>(entrances.size())); i++) {
    out << "    0x" << std::hex << std::setw(4) << std::setfill('0')
        << entrances[i].map_pos_;
    if (i < 9) out << ",";
    out << "  // Entrance " << std::dec << i << "\n";
  }
  out << "}};\n\n";
}

void ToolsExtractGoldenCommandHandler::WriteFooter(std::ofstream& out) {
  out << "\n";
  out << "}  // namespace test\n";
  out << "}  // namespace yaze\n";
}

// =============================================================================
// ToolsPatchV3CommandHandler
// =============================================================================

absl::Status ToolsPatchV3CommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  std::string input_path = parser.GetString("rom").value_or("");
  std::string output_path = parser.GetString("output").value_or("");

  // Load the vanilla ROM
  Rom rom;
  RETURN_IF_ERROR(rom.LoadFromFile(input_path));

  // Apply v3 patch
  RETURN_IF_ERROR(ApplyV3Patch(rom));

  // Save the patched ROM
  RETURN_IF_ERROR(rom.SaveToFile(Rom::SaveSettings{.filename = output_path}));

  formatter.AddField("status", "success");
  formatter.AddField("output_file", output_path);
  formatter.AddField("message",
                     "Successfully created v3 patched ROM: " + output_path);
  return absl::OkStatus();
}

absl::Status ToolsPatchV3CommandHandler::ApplyV3Patch(Rom& rom) {
  // Set ASM version to v3
  rom.WriteByte(OverworldCustomASMHasBeenApplied, 0x03);

  // Enable v3 features
  rom.WriteByte(OverworldCustomAreaSpecificBGEnabled, 0x01);
  rom.WriteByte(OverworldCustomSubscreenOverlayEnabled, 0x01);
  rom.WriteByte(OverworldCustomAnimatedGFXEnabled, 0x01);
  rom.WriteByte(OverworldCustomTileGFXGroupEnabled, 0x01);
  rom.WriteByte(OverworldCustomMosaicEnabled, 0x01);
  rom.WriteByte(OverworldCustomMainPaletteEnabled, 0x01);

  // Apply v3 settings to first 10 maps for testing
  for (int i = 0; i < 10; i++) {
    // Set area sizes (mix of different sizes)
    auto area_size = static_cast<AreaSizeEnum>(i % 4);
    rom.WriteByte(kOverworldScreenSize + i, static_cast<uint8_t>(area_size));

    // Set main palettes
    rom.WriteByte(OverworldCustomMainPaletteArray + i, i % 8);

    // Set area-specific background colors
    uint16_t bg_color = 0x0000 + (i * 0x1000);
    rom.WriteByte(OverworldCustomAreaSpecificBGPalette + (i * 2),
                  bg_color & 0xFF);
    rom.WriteByte(OverworldCustomAreaSpecificBGPalette + (i * 2) + 1,
                  (bg_color >> 8) & 0xFF);

    // Set subscreen overlays
    uint16_t overlay = 0x0090 + i;
    rom.WriteByte(OverworldCustomSubscreenOverlayArray + (i * 2),
                  overlay & 0xFF);
    rom.WriteByte(OverworldCustomSubscreenOverlayArray + (i * 2) + 1,
                  (overlay >> 8) & 0xFF);

    // Set animated GFX
    rom.WriteByte(OverworldCustomAnimatedGFXArray + i, 0x50 + i);

    // Set custom tile GFX groups (8 bytes per map)
    for (int j = 0; j < 8; j++) {
      rom.WriteByte(OverworldCustomTileGFXGroupArray + (i * 8) + j,
                    0x20 + j + i);
    }

    // Set mosaic settings
    rom.WriteByte(OverworldCustomMosaicArray + i, i % 16);

    // Set expanded message IDs
    uint16_t message_id = 0x1000 + i;
    rom.WriteByte(kOverworldMessagesExpanded + (i * 2), message_id & 0xFF);
    rom.WriteByte(kOverworldMessagesExpanded + (i * 2) + 1,
                  (message_id >> 8) & 0xFF);
  }

  return absl::OkStatus();
}

// =============================================================================
// ToolsListCommandHandler
// =============================================================================

absl::Status ToolsListCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& /*parser*/,
    resources::OutputFormatter& formatter) {
  std::cout << "Available Test Helper Tools:\n\n";
  std::cout << "  tools-harness-state   Generate WRAM state for test harnesses\n";
  std::cout << "  tools-extract-values  Extract vanilla ROM values\n";
  std::cout << "  tools-extract-golden  Extract comprehensive golden data\n";
  std::cout << "  tools-patch-v3        Create v3 ZSCustomOverworld patched ROM\n";
  std::cout << "\nUsage: z3ed <tool-name> --help\n";

  formatter.AddField("status", "success");
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

