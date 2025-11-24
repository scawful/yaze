/**
 * @file yaze_debug_inspector.cc
 * @brief WASM debug inspector for yaze - palette, overworld, and emulator access
 *
 * This file provides JavaScript bindings for debugging infrastructure used by
 * Gemini/Antigravity AI integration to analyze rendering issues and game state.
 */

#include <emscripten/bind.h>

#include <sstream>

#include "app/gfx/resource/arena.h"
#include "app/rom.h"
#include "zelda3/dungeon/palette_debug.h"

using namespace emscripten;

// External function to get the global ROM (defined in wasm_terminal_bridge.cc)
namespace yaze::cli {
extern Rom* GetGlobalRom();
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
    json << "\"title\":\"" << rom->title() << "\",";
    json << "\"version\":" << static_cast<int>(rom->version());
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

std::string getRomPaletteGroup(int group_id, int palette_index) {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  if (!rom || !rom->is_loaded()) {
    return "{\"error\":\"No ROM loaded\"}";
  }

  json << "{\"group_id\":" << group_id << ",\"palette_index\":" << palette_index;

  // Get palette colors from the ROM's palette groups
  try {
    auto& palette_group = rom->palette_group();
    if (group_id >= 0 && group_id < static_cast<int>(palette_group.size())) {
      auto& group = palette_group[group_id];
      if (palette_index >= 0 && palette_index < static_cast<int>(group.size())) {
        auto& palette = group[palette_index];
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
      json << ",\"error\":\"Invalid group ID\"";
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
// Combined Debug State for AI Analysis
// =============================================================================

std::string getFullDebugState() {
  std::ostringstream json;

  json << "{";

  // Palette debug state
  json << "\"palette\":" << getFullPaletteState() << ",";

  // Arena status
  json << "\"arena\":" << getArenaStatus() << ",";

  // ROM status
  json << "\"rom\":" << getRomStatus() << ",";

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

  // Combined state for AI
  function("getFullDebugState", &getFullDebugState);
}
