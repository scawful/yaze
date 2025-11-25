/**
 * @file yaze_debug_inspector.cc
 * @brief WASM debug inspector for yaze - palette, overworld, and emulator access
 *
 * This file provides JavaScript bindings for debugging infrastructure used by
 * Gemini/Antigravity AI integration to analyze rendering issues and game state.
 */

#include <emscripten/bind.h>

#include <iomanip>
#include <sstream>

#include "yaze.h"  // For YAZE_VERSION_STRING
#include "app/emu/emulator.h"
#include "app/emu/snes.h"
#include "app/emu/video/ppu.h"
#include "app/gfx/resource/arena.h"
#include "app/rom.h"
#include "zelda3/dungeon/palette_debug.h"

#include "app/editor/editor_manager.h"
#include "app/editor/editor.h"

using namespace emscripten;

// External function to get the global ROM (defined in wasm_terminal_bridge.cc)
namespace yaze::cli {
extern Rom* GetGlobalRom();
}

// External function to get the global emulator (defined in main.cc)
namespace yaze::app {
extern emu::Emulator* GetGlobalEmulator();
extern editor::EditorManager* GetGlobalEditorManager();
}

extern "C" {
// Forward declaration of Z3edProcessCommand from wasm_terminal_bridge.cc
const char* Z3edProcessCommand(const char* command);
}

// Helper function to get the emulator for this file
namespace {
yaze::emu::Emulator* GetGlobalEmulator() {
  return yaze::app::GetGlobalEmulator();
}
}  // namespace

// =============================================================================
// Editor State Functions
// =============================================================================

std::string getEditorState() {
  std::ostringstream json;
  auto* manager = yaze::app::GetGlobalEditorManager();
  
  if (!manager) {
    return "{\"error\":\"EditorManager not available\"}";
  }
  
  auto* editor = manager->GetCurrentEditor();
  
  json << "{";
  json << "\"active_editor\":\"" << (editor ? yaze::editor::kEditorNames[(int)editor->type()] : "None") << "\",";
  json << "\"session_id\":" << manager->GetCurrentSessionId() << ",";
  json << "\"rom_loaded\":" << (manager->GetCurrentRom() && manager->GetCurrentRom()->is_loaded() ? "true" : "false");
  
  if (editor && editor->type() == yaze::editor::EditorType::kDungeon) {
     // We can't easily cast to DungeonEditorV2 here without circular deps or massive includes
     // But we can check if it exposes state via base class if we added virtuals? No.
     // For now, just knowing it's the Dungeon Editor is a big help.
  }
  
  json << "}";
  return json.str();
}

std::string executeCommand(std::string command) {
  // Wrapper around Z3edProcessCommand for easier JS usage
  const char* result = Z3edProcessCommand(command.c_str());
  return result ? std::string(result) : "";
}

std::string switchToEditor(std::string editor_name) {
  auto* manager = yaze::app::GetGlobalEditorManager();
  if (!manager) {
    return "{\"error\":\"EditorManager not available\"}";
  }
  
  if (editor_name == "Dungeon") {
    manager->SwitchToEditor(yaze::editor::EditorType::kDungeon);
    return "{\"success\":true,\"editor\":\"Dungeon\"}";
  } else if (editor_name == "Overworld") {
    manager->SwitchToEditor(yaze::editor::EditorType::kOverworld);
    return "{\"success\":true,\"editor\":\"Overworld\"}";
  } else if (editor_name == "Sprite") {
    manager->SwitchToEditor(yaze::editor::EditorType::kSprite);
    return "{\"success\":true,\"editor\":\"Sprite\"}";
  } else if (editor_name == "Text") {
    manager->SwitchToEditor(yaze::editor::EditorType::kMessage);
    return "{\"success\":true,\"editor\":\"Text\"}";
  }
  
  return "{\"error\":\"Unknown editor name\"}";
}

// =============================================================================
// Palette Debug Functions
// =============================================================================

std::string getDungeonPaletteEvents() {
  return yaze::zelda3::PaletteDebugger::Get().ExportToJSON();
}

std::string getColorComparisons() {
  return yaze::zelda3::PaletteDebugger::Get().ExportColorComparisonsJSON();
}

std::string samplePixelAt(int x, int y) {
  return yaze::zelda3::PaletteDebugger::Get().SamplePixelJSON(x, y);
}

void clearPaletteDebugEvents() {
  yaze::zelda3::PaletteDebugger::Get().Clear();
}

// AI analysis functions for Gemini/Antigravity integration
std::string getFullPaletteState() {
  return yaze::zelda3::PaletteDebugger::Get().ExportFullStateJSON();
}

std::string getPaletteData() {
  return yaze::zelda3::PaletteDebugger::Get().ExportPaletteDataJSON();
}

std::string getEventTimeline() {
  return yaze::zelda3::PaletteDebugger::Get().ExportTimelineJSON();
}

std::string getDiagnosticSummary() {
  return yaze::zelda3::PaletteDebugger::Get().GetDiagnosticSummary();
}

std::string getHypothesisAnalysis() {
  return yaze::zelda3::PaletteDebugger::Get().GetHypothesisAnalysis();
}

// =============================================================================
// Graphics Arena Debug Functions
// =============================================================================

std::string getArenaStatus() {
  std::ostringstream json;
  auto& arena = yaze::gfx::Arena::Get();

  json << "{";
  json << "\"texture_queue_size\":" << arena.texture_command_queue_size()
       << ",";

  // Get info about graphics sheets
  json << "\"gfx_sheets\":[";
  bool first = true;
  for (int i = 0; i < 223; i++) {
    auto sheet = arena.gfx_sheet(i);  // Returns by value
    if (sheet.is_active()) {
      if (!first) json << ",";
      json << "{\"index\":" << i << ",\"width\":" << sheet.width()
           << ",\"height\":" << sheet.height()
           << ",\"has_texture\":" << (sheet.texture() != nullptr ? "true" : "false")
           << ",\"has_surface\":" << (sheet.surface() != nullptr ? "true" : "false")
           << "}";
      first = false;
    }
  }
  json << "]";
  json << "}";

  return json.str();
}

std::string getGfxSheetInfo(int index) {
  if (index < 0 || index >= 223) {
    return "{\"error\": \"Invalid sheet index\"}";
  }

  std::ostringstream json;
  auto& arena = yaze::gfx::Arena::Get();
  auto sheet = arena.gfx_sheet(index);  // Returns by value

  json << "{";
  json << "\"index\":" << index << ",";
  json << "\"active\":" << (sheet.is_active() ? "true" : "false") << ",";
  json << "\"width\":" << sheet.width() << ",";
  json << "\"height\":" << sheet.height() << ",";
  json << "\"has_texture\":" << (sheet.texture() != nullptr ? "true" : "false")
       << ",";
  json << "\"has_surface\":" << (sheet.surface() != nullptr ? "true" : "false");

  // If surface exists, get palette info
  if (sheet.surface() && sheet.surface()->format) {
    auto* fmt = sheet.surface()->format;
    json << ",\"surface_format\":" << fmt->format;
    if (fmt->palette) {
      json << ",\"palette_colors\":" << fmt->palette->ncolors;
    }
  }

  json << "}";
  return json.str();
}

// =============================================================================
// ROM Debug Functions
// =============================================================================

std::string getRomStatus() {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  json << "{";

  if (!rom) {
    json << "\"loaded\":false,\"error\":\"No ROM loaded\"";
  } else {
    json << "\"loaded\":" << (rom->is_loaded() ? "true" : "false") << ",";
    json << "\"size\":" << rom->size() << ",";
    json << "\"title\":\"" << rom->title() << "\"";
  }

  json << "}";
  return json.str();
}

std::string readRomBytes(int address, int count) {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  if (!rom || !rom->is_loaded()) {
    return "{\"error\":\"No ROM loaded\"}";
  }

  if (count > 256) count = 256;  // Limit to prevent huge responses
  if (count < 1) count = 1;

  json << "{\"address\":" << address << ",\"count\":" << count << ",\"bytes\":[";

  for (int i = 0; i < count; i++) {
    if (i > 0) json << ",";
    auto byte_result = rom->ReadByte(address + i);
    if (byte_result.ok()) {
      json << static_cast<int>(*byte_result);
    } else {
      json << "null";
    }
  }

  json << "]}";
  return json.str();
}

std::string getRomPaletteGroup(const std::string& group_name, int palette_index) {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  if (!rom || !rom->is_loaded()) {
    return "{\"error\":\"No ROM loaded\"}";
  }

  json << "{\"group_name\":\"" << group_name << "\",\"palette_index\":" << palette_index;

  // Get palette colors from the ROM's palette groups
  try {
    auto palette_group = rom->palette_group();
    auto* group = palette_group.get_group(group_name);
    if (group) {
      if (palette_index >= 0 && palette_index < static_cast<int>(group->size())) {
        auto palette = (*group)[palette_index];
        json << ",\"size\":" << palette.size();
        json << ",\"colors\":[";
        for (size_t i = 0; i < palette.size(); i++) {
          if (i > 0) json << ",";
          auto rgb = palette[i].rgb();
          json << "{\"r\":" << static_cast<int>(rgb.x)
               << ",\"g\":" << static_cast<int>(rgb.y)
               << ",\"b\":" << static_cast<int>(rgb.z) << "}";
        }
        json << "]";
      } else {
        json << ",\"error\":\"Invalid palette index\"";
      }
    } else {
      json << ",\"error\":\"Invalid group name. Valid names: ow_main, ow_aux, ow_animated, hud, global_sprites, armors, swords, shields, sprites_aux1, sprites_aux2, sprites_aux3, dungeon_main, grass, 3d_object, ow_mini_map\"";
    }
  } catch (...) {
    json << ",\"error\":\"Exception accessing palette\"";
  }

  json << "}";
  return json.str();
}

// =============================================================================
// Overworld Debug Functions
// =============================================================================

std::string getOverworldMapInfo(int map_id) {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  if (!rom || !rom->is_loaded()) {
    return "{\"error\":\"No ROM loaded\"}";
  }

  // Overworld map constants
  constexpr int kNumOverworldMaps = 160;
  if (map_id < 0 || map_id >= kNumOverworldMaps) {
    return "{\"error\":\"Invalid map ID (0-159)\"}";
  }

  json << "{\"map_id\":" << map_id;

  // Read map properties from known ROM addresses
  // Map size: 0x12844 + map_id
  auto size_byte = rom->ReadByte(0x12844 + map_id);
  if (size_byte.ok()) {
    json << ",\"size_flag\":" << static_cast<int>(*size_byte);
    json << ",\"is_large\":" << (*size_byte == 0x20 ? "true" : "false");
  }

  // Parent ID: 0x125EC + map_id
  auto parent_byte = rom->ReadByte(0x125EC + map_id);
  if (parent_byte.ok()) {
    json << ",\"parent_id\":" << static_cast<int>(*parent_byte);
  }

  // Determine world type
  if (map_id < 64) {
    json << ",\"world\":\"light\"";
  } else if (map_id < 128) {
    json << ",\"world\":\"dark\"";
  } else {
    json << ",\"world\":\"special\"";
  }

  json << "}";
  return json.str();
}

std::string getOverworldTileInfo(int map_id, int tile_x, int tile_y) {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  if (!rom || !rom->is_loaded()) {
    return "{\"error\":\"No ROM loaded\"}";
  }

  json << "{\"map_id\":" << map_id
       << ",\"tile_x\":" << tile_x
       << ",\"tile_y\":" << tile_y;

  // Note: Full tile data access would require loading the overworld
  // For now, provide basic info that can be accessed without full load
  json << ",\"note\":\"Full tile data requires overworld to be loaded in editor\"";

  json << "}";
  return json.str();
}

// =============================================================================
// Emulator Debug Functions
// =============================================================================

/**
 * @brief Get the current emulator status including CPU state
 *
 * Returns JSON with:
 * - initialized: whether the emulator is ready
 * - running: whether the emulator is currently executing
 * - cpu: register values (A, X, Y, SP, PC, D, DB, PB, P/status)
 * - cycles: total cycles executed
 * - fps: current frames per second
 *
 * CPU status flags (P register):
 * - N (0x80): Negative
 * - V (0x40): Overflow
 * - M (0x20): Accumulator size (0=16-bit, 1=8-bit)
 * - X (0x10): Index size (0=16-bit, 1=8-bit)
 * - D (0x08): Decimal mode
 * - I (0x04): IRQ disable
 * - Z (0x02): Zero
 * - C (0x01): Carry
 * - E: Emulation mode (6502 compatibility)
 */
std::string getEmulatorStatus() {
  std::ostringstream json;
  auto* emulator = GetGlobalEmulator();

  json << "{";

  if (!emulator) {
    json << "\"initialized\":false,\"error\":\"Emulator not available\"";
    json << "}";
    return json.str();
  }

  bool is_initialized = emulator->is_snes_initialized();
  bool is_running = emulator->running();

  json << "\"initialized\":" << (is_initialized ? "true" : "false") << ",";
  json << "\"running\":" << (is_running ? "true" : "false") << ",";

  if (is_initialized) {
    auto& snes = emulator->snes();
    auto& cpu = snes.cpu();

    // CPU registers
    json << "\"cpu\":{";
    json << "\"A\":" << cpu.A << ",";
    json << "\"X\":" << cpu.X << ",";
    json << "\"Y\":" << cpu.Y << ",";
    json << "\"SP\":" << cpu.SP() << ",";
    json << "\"PC\":" << cpu.PC << ",";
    json << "\"D\":" << cpu.D << ",";       // Direct page register
    json << "\"DB\":" << (int)cpu.DB << ","; // Data bank register
    json << "\"PB\":" << (int)cpu.PB << ","; // Program bank register
    json << "\"P\":" << (int)cpu.status << ","; // Processor status
    json << "\"E\":" << (int)cpu.E << ",";   // Emulation mode flag

    // Decode status flags for convenience
    json << "\"flags\":{";
    json << "\"N\":" << (cpu.GetNegativeFlag() ? "true" : "false") << ",";
    json << "\"V\":" << (cpu.GetOverflowFlag() ? "true" : "false") << ",";
    json << "\"M\":" << (cpu.GetAccumulatorSize() ? "true" : "false") << ",";
    json << "\"X\":" << (cpu.GetIndexSize() ? "true" : "false") << ",";
    json << "\"D\":" << (cpu.GetDecimalFlag() ? "true" : "false") << ",";
    json << "\"I\":" << (cpu.GetInterruptFlag() ? "true" : "false") << ",";
    json << "\"Z\":" << (cpu.GetZeroFlag() ? "true" : "false") << ",";
    json << "\"C\":" << (cpu.GetCarryFlag() ? "true" : "false");
    json << "}";
    json << "},";

    // Full 24-bit PC address
    uint32_t full_pc = ((uint32_t)cpu.PB << 16) | cpu.PC;
    json << "\"full_pc\":" << full_pc << ",";
    json << "\"full_pc_hex\":\"$" << std::hex << std::uppercase
         << std::setfill('0') << std::setw(6) << full_pc << std::dec << "\",";

    // Timing information
    json << "\"cycles\":" << snes.mutable_cycles() << ",";
    json << "\"fps\":" << emulator->GetCurrentFPS();
  }

  json << "}";
  return json.str();
}

/**
 * @brief Read memory from the emulator's WRAM
 *
 * @param address Starting address (SNES address space, e.g., 0x7E0000 for WRAM)
 * @param count Number of bytes to read (max 256)
 * @return JSON with address, count, and bytes array
 *
 * Memory map reference:
 * - $7E0000-$7E1FFF: Low RAM (first 8KB, mirrored in banks $00-$3F)
 * - $7E2000-$7FFFFF: High RAM (additional ~120KB)
 * - $7F0000-$7FFFFF: Extended RAM (second 64KB bank)
 */
std::string readEmulatorMemory(int address, int count) {
  std::ostringstream json;
  auto* emulator = GetGlobalEmulator();

  if (!emulator || !emulator->is_snes_initialized()) {
    return "{\"error\":\"Emulator not initialized\"}";
  }

  // Clamp count to prevent huge responses
  if (count > 256) count = 256;
  if (count < 1) count = 1;

  auto& snes = emulator->snes();

  json << "{\"address\":" << address << ",";
  json << "\"address_hex\":\"$" << std::hex << std::uppercase
       << std::setfill('0') << std::setw(6) << address << std::dec << "\",";
  json << "\"count\":" << count << ",";
  json << "\"bytes\":[";

  // Read bytes from emulator memory using Snes::Read
  // This respects SNES memory mapping
  for (int i = 0; i < count; i++) {
    if (i > 0) json << ",";
    uint8_t byte = snes.Read(address + i);
    json << static_cast<int>(byte);
  }

  json << "],\"hex\":\"";
  // Also provide hex string representation
  for (int i = 0; i < count; i++) {
    uint8_t byte = snes.Read(address + i);
    json << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
         << static_cast<int>(byte);
  }
  json << std::dec << "\"}";

  return json.str();
}

/**
 * @brief Get PPU (video) state from the emulator
 *
 * Returns JSON with:
 * - current_scanline: Current rendering scanline
 * - h_pos / v_pos: Horizontal/vertical position
 * - mode: Current BG mode (0-7)
 * - brightness: Screen brightness level
 * - forced_blank: Whether screen is blanked
 * - overscan: Whether overscan is enabled
 * - interlace: Whether interlacing is enabled
 */
std::string getEmulatorVideoState() {
  std::ostringstream json;
  auto* emulator = GetGlobalEmulator();

  if (!emulator || !emulator->is_snes_initialized()) {
    return "{\"error\":\"Emulator not initialized\"}";
  }

  auto& snes = emulator->snes();
  auto& ppu = snes.ppu();
  auto& memory = snes.memory();

  json << "{";

  // Scanline and position info from memory interface
  json << "\"h_pos\":" << memory.h_pos() << ",";
  json << "\"v_pos\":" << memory.v_pos() << ",";
  json << "\"current_scanline\":" << ppu.current_scanline_ << ",";

  // PPU mode and settings
  json << "\"mode\":" << (int)ppu.mode << ",";
  json << "\"brightness\":" << (int)ppu.brightness << ",";
  json << "\"forced_blank\":" << (ppu.forced_blank_ ? "true" : "false") << ",";
  json << "\"overscan\":" << (ppu.overscan_ ? "true" : "false") << ",";
  json << "\"frame_overscan\":" << (ppu.frame_overscan_ ? "true" : "false") << ",";
  json << "\"interlace\":" << (ppu.interlace ? "true" : "false") << ",";
  json << "\"frame_interlace\":" << (ppu.frame_interlace ? "true" : "false") << ",";
  json << "\"pseudo_hires\":" << (ppu.pseudo_hires_ ? "true" : "false") << ",";
  json << "\"direct_color\":" << (ppu.direct_color_ ? "true" : "false") << ",";
  json << "\"bg3_priority\":" << (ppu.bg3priority ? "true" : "false") << ",";
  json << "\"even_frame\":" << (ppu.even_frame ? "true" : "false") << ",";

  // VRAM pointer info
  json << "\"vram_pointer\":" << ppu.vram_pointer << ",";
  json << "\"vram_increment\":" << ppu.vram_increment_ << ",";
  json << "\"vram_increment_on_high\":" << (ppu.vram_increment_on_high_ ? "true" : "false");

  json << "}";
  return json.str();
}

// =============================================================================
// Version and Session Management
// =============================================================================

std::string getYazeVersion() {
  return YAZE_VERSION_STRING;
}

std::string getRomSessions() {
  std::ostringstream json;
  auto* manager = yaze::app::GetGlobalEditorManager();

  if (!manager) {
    return "{\"error\":\"EditorManager not available\",\"sessions\":[]}";
  }

  json << "{";
  json << "\"current_session\":" << manager->GetCurrentSessionId() << ",";

  // Get current ROM info
  auto* current_rom = manager->GetCurrentRom();
  if (current_rom && current_rom->is_loaded()) {
    json << "\"current_rom\":{";
    json << "\"loaded\":true,";
    json << "\"title\":\"" << current_rom->title() << "\",";
    json << "\"filename\":\"" << current_rom->filename() << "\",";
    json << "\"size\":" << current_rom->size();
    json << "},";
  } else {
    json << "\"current_rom\":{\"loaded\":false},";
  }

  // Note: Full session enumeration would require exposing session_coordinator
  // For now, provide current session info
  json << "\"sessions\":[]";

  json << "}";
  return json.str();
}

std::string getFileManagerDebugInfo() {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  json << "{";
  json << "\"global_rom_ptr\":" << (rom ? "true" : "false") << ",";

  if (rom) {
    json << "\"rom_loaded\":" << (rom->is_loaded() ? "true" : "false") << ",";
    json << "\"rom_size\":" << rom->size() << ",";
    json << "\"rom_filename\":\"" << rom->filename() << "\",";
    json << "\"rom_title\":\"" << rom->title() << "\",";

    // Add diagnostics if available
    if (rom->is_loaded()) {
      auto& diag = rom->GetDiagnostics();
      json << "\"diagnostics\":{";
      json << "\"header_stripped\":" << (diag.header_stripped ? "true" : "false") << ",";
      json << "\"checksum_valid\":" << (diag.checksum_valid ? "true" : "false") << ",";
      json << "\"sheets_loaded\":" << diag.sheets.size();
      json << "}";
    }
  }

  // EditorManager info
  auto* manager = yaze::app::GetGlobalEditorManager();
  if (manager) {
    json << ",\"editor_manager\":{";
    json << "\"session_count\":" << manager->GetActiveSessionCount() << ",";
    json << "\"current_session\":" << manager->GetCurrentSessionId() << ",";
    json << "\"has_current_rom\":" << (manager->GetCurrentRom() ? "true" : "false");
    json << "}";
  } else {
    json << ",\"editor_manager\":null";
  }

  json << "}";
  return json.str();
}

void resumeAudioContext() {
  auto* emulator = GetGlobalEmulator();
  if (emulator) {
    emulator->ResumeAudio();
  }
}

// =============================================================================
// Combined Debug State for AI Analysis
// =============================================================================

std::string getGraphicsDiagnostics() {
  auto* rom = yaze::cli::GetGlobalRom();
  if (!rom || !rom->is_loaded()) {
    return "{\"error\":\"No ROM loaded\"}";
  }
  return rom->GetDiagnostics().ToJson();
}

std::string getFullDebugState() {
  std::ostringstream json;

  json << "{";

  // Palette debug state
  json << "\"palette\":" << getFullPaletteState() << ",";

  // Arena status
  json << "\"arena\":" << getArenaStatus() << ",";

  // ROM status
  json << "\"rom\":" << getRomStatus() << ",";

  // Emulator status
  json << "\"emulator\":" << getEmulatorStatus() << ",";

  // Diagnostic summary
  json << "\"diagnostic\":\"" << getDiagnosticSummary() << "\",";

  // Hypothesis
  json << "\"hypothesis\":\"" << getHypothesisAnalysis() << "\"";

  json << "}";

  return json.str();
}

// =============================================================================
// Emscripten Bindings
// =============================================================================

EMSCRIPTEN_BINDINGS(yaze_debug_inspector) {
  // Palette debug functions
  function("getDungeonPaletteEvents", &getDungeonPaletteEvents);
  function("getColorComparisons", &getColorComparisons);
  function("samplePixelAt", &samplePixelAt);
  function("clearPaletteDebugEvents", &clearPaletteDebugEvents);

  // AI analysis functions
  function("getFullPaletteState", &getFullPaletteState);
  function("getPaletteData", &getPaletteData);
  function("getEventTimeline", &getEventTimeline);
  function("getDiagnosticSummary", &getDiagnosticSummary);
  function("getHypothesisAnalysis", &getHypothesisAnalysis);

  // Arena debug functions
  function("getArenaStatus", &getArenaStatus);
  function("getGfxSheetInfo", &getGfxSheetInfo);

  // ROM debug functions
  function("getRomStatus", &getRomStatus);
  function("readRomBytes", &readRomBytes);
  function("getRomPaletteGroup", &getRomPaletteGroup);

  // Overworld debug functions
  function("getOverworldMapInfo", &getOverworldMapInfo);
  function("getOverworldTileInfo", &getOverworldTileInfo);

  // Emulator debug functions
  function("getEmulatorStatus", &getEmulatorStatus);
  function("readEmulatorMemory", &readEmulatorMemory);
  function("getEmulatorVideoState", &getEmulatorVideoState);
  function("resumeAudioContext", &resumeAudioContext);

  // Editor state and command execution
  function("getEditorState", &getEditorState);
  function("executeCommand", &executeCommand);
  function("switchToEditor", &switchToEditor);

  // Combined state for AI
  function("getFullDebugState", &getFullDebugState);
  function("getGraphicsDiagnostics", &getGraphicsDiagnostics);

  // Version and session management
  function("getYazeVersion", &getYazeVersion);
  function("getRomSessions", &getRomSessions);
  function("getFileManagerDebugInfo", &getFileManagerDebugInfo);
}
