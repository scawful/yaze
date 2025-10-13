#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <filesystem>

#include "app/rom.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

using namespace yaze::zelda3;
using namespace yaze;

/**
 * @brief Comprehensive ROM value extraction tool for golden data testing
 * 
 * This tool extracts all overworld-related values from a ROM to create
 * "golden" reference data for before/after edit validation and comprehensive
 * E2E testing. It supports both vanilla and ZSCustomOverworld ROMs.
 */
class OverworldGoldenDataExtractor {
 public:
  explicit OverworldGoldenDataExtractor(const std::string& rom_path) 
      : rom_path_(rom_path) {}
  
  absl::Status ExtractAllData(const std::string& output_path) {
    // Load ROM
    Rom rom;
    RETURN_IF_ERROR(rom.LoadFromFile(rom_path_));
    
    // Load overworld data
    Overworld overworld(&rom);
    RETURN_IF_ERROR(overworld.Load(&rom));
    
    std::ofstream out_file(output_path);
    if (!out_file.is_open()) {
      return absl::InternalError("Failed to open output file: " + output_path);
    }
    
    // Write header
    WriteHeader(out_file);
    
    // Extract basic ROM info
    WriteBasicROMInfo(out_file, rom);
    
    // Extract ASM version info
    WriteASMVersionInfo(out_file, rom);
    
    // Extract overworld maps data
    WriteOverworldMapsData(out_file, overworld);
    
    // Extract tile data
    WriteTileData(out_file, overworld);
    
    // Extract entrance/hole/exit data
    WriteEntranceData(out_file, overworld);
    WriteHoleData(out_file, overworld);
    WriteExitData(out_file, overworld);
    
    // Extract item data
    WriteItemData(out_file, overworld);
    
    // Extract sprite data
    WriteSpriteData(out_file, overworld);
    
    // Extract map tiles (compressed data)
    WriteMapTilesData(out_file, overworld);
    
    // Extract palette data
    WritePaletteData(out_file, rom);
    
    // Extract music data
    WriteMusicData(out_file, rom);
    
    // Extract overlay data
    WriteOverlayData(out_file, rom);
    
    // Write footer
    WriteFooter(out_file);
    
    return absl::OkStatus();
  }
  
 private:
  void WriteHeader(std::ofstream& out) {
    out << "// =============================================================================" << std::endl;
    out << "// YAZE Overworld Golden Data - Generated from: " << rom_path_ << std::endl;
    out << "// Generated on: " << __DATE__ << " " << __TIME__ << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    out << "#pragma once" << std::endl;
    out << std::endl;
    out << "#include <cstdint>" << std::endl;
    out << "#include <array>" << std::endl;
    out << "#include <vector>" << std::endl;
    out << "#include \"zelda3/overworld/overworld_map.h\"" << std::endl;
    out << std::endl;
    out << "namespace yaze {" << std::endl;
    out << "namespace test {" << std::endl;
    out << std::endl;
  }
  
  void WriteFooter(std::ofstream& out) {
    out << std::endl;
    out << "}  // namespace test" << std::endl;
    out << "}  // namespace yaze" << std::endl;
  }
  
  void WriteBasicROMInfo(std::ofstream& out, Rom& rom) {
    out << "// =============================================================================" << std::endl;
    out << "// Basic ROM Information" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    out << "constexpr std::string_view kGoldenROMTitle = \"" << rom.title() << "\";" << std::endl;
    out << "constexpr size_t kGoldenROMSize = " << rom.size() << ";" << std::endl;
    out << std::endl;
    
    // ROM header validation
    auto header_checksum = rom.ReadWord(0x7FDC);
    auto header_checksum_complement = rom.ReadWord(0x7FDE);
    if (header_checksum.ok() && header_checksum_complement.ok()) {
      out << "constexpr uint16_t kGoldenHeaderChecksum = 0x" 
          << std::hex << std::setw(4) << std::setfill('0') 
          << *header_checksum << ";" << std::endl;
      out << "constexpr uint16_t kGoldenHeaderChecksumComplement = 0x" 
          << std::hex << std::setw(4) << std::setfill('0') 
          << *header_checksum_complement << ";" << std::endl;
      out << std::endl;
    }
  }
  
  void WriteASMVersionInfo(std::ofstream& out, Rom& rom) {
    out << "// =============================================================================" << std::endl;
    out << "// ASM Version Information" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    auto asm_version = rom.ReadByte(0x140145);
    if (asm_version.ok()) {
      out << "constexpr uint8_t kGoldenASMVersion = 0x" 
          << std::hex << std::setw(2) << std::setfill('0') 
          << static_cast<int>(*asm_version) << ";" << std::endl;
      
      if (*asm_version == 0xFF) {
        out << "constexpr bool kGoldenIsVanillaROM = true;" << std::endl;
        out << "constexpr bool kGoldenHasZSCustomOverworld = false;" << std::endl;
      } else {
        out << "constexpr bool kGoldenIsVanillaROM = false;" << std::endl;
        out << "constexpr bool kGoldenHasZSCustomOverworld = true;" << std::endl;
        out << "constexpr uint8_t kGoldenZSCustomOverworldVersion = " 
            << static_cast<int>(*asm_version) << ";" << std::endl;
      }
      out << std::endl;
    }
    
    // Feature flags for v3
    if (asm_version.ok() && *asm_version >= 0x03) {
      out << "// v3 Feature Flags" << std::endl;
      
      auto main_palettes = rom.ReadByte(0x140146);
      auto area_bg = rom.ReadByte(0x140147);
      auto subscreen_overlay = rom.ReadByte(0x140148);
      auto animated_gfx = rom.ReadByte(0x140149);
      auto custom_tiles = rom.ReadByte(0x14014A);
      auto mosaic = rom.ReadByte(0x14014B);
      
      if (main_palettes.ok()) {
        out << "constexpr bool kGoldenEnableMainPalettes = " 
            << (*main_palettes != 0 ? "true" : "false") << ";" << std::endl;
      }
      if (area_bg.ok()) {
        out << "constexpr bool kGoldenEnableAreaSpecificBG = " 
            << (*area_bg != 0 ? "true" : "false") << ";" << std::endl;
      }
      if (subscreen_overlay.ok()) {
        out << "constexpr bool kGoldenEnableSubscreenOverlay = " 
            << (*subscreen_overlay != 0 ? "true" : "false") << ";" << std::endl;
      }
      if (animated_gfx.ok()) {
        out << "constexpr bool kGoldenEnableAnimatedGFX = " 
            << (*animated_gfx != 0 ? "true" : "false") << ";" << std::endl;
      }
      if (custom_tiles.ok()) {
        out << "constexpr bool kGoldenEnableCustomTiles = " 
            << (*custom_tiles != 0 ? "true" : "false") << ";" << std::endl;
      }
      if (mosaic.ok()) {
        out << "constexpr bool kGoldenEnableMosaic = " 
            << (*mosaic != 0 ? "true" : "false") << ";" << std::endl;
      }
      out << std::endl;
    }
  }
  
  void WriteOverworldMapsData(std::ofstream& out, Overworld& overworld) {
    out << "// =============================================================================" << std::endl;
    out << "// Overworld Maps Data" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    const auto& maps = overworld.overworld_maps();
    out << "constexpr size_t kGoldenNumOverworldMaps = " << maps.size() << ";" << std::endl;
    out << std::endl;
    
    // Extract map properties for first 20 maps (to keep file size manageable)
    out << "// Map properties for first 20 maps" << std::endl;
    out << "constexpr std::array<uint8_t, 20> kGoldenMapAreaGraphics = {{" << std::endl;
    for (int i = 0; i < std::min(20, static_cast<int>(maps.size())); i++) {
      out << "    0x" << std::hex << std::setw(2) << std::setfill('0') 
          << static_cast<int>(maps[i].area_graphics());
      if (i < 19) out << ",";
      out << "  // Map " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
    
    out << "constexpr std::array<uint8_t, 20> kGoldenMapMainPalettes = {{" << std::endl;
    for (int i = 0; i < std::min(20, static_cast<int>(maps.size())); i++) {
      out << "    0x" << std::hex << std::setw(2) << std::setfill('0') 
          << static_cast<int>(maps[i].main_palette());
      if (i < 19) out << ",";
      out << "  // Map " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
    
    out << "constexpr std::array<AreaSizeEnum, 20> kGoldenMapAreaSizes = {{" << std::endl;
    for (int i = 0; i < std::min(20, static_cast<int>(maps.size())); i++) {
      out << "    AreaSizeEnum::";
      switch (maps[i].area_size()) {
        case AreaSizeEnum::SmallArea: out << "SmallArea"; break;
        case AreaSizeEnum::LargeArea: out << "LargeArea"; break;
        case AreaSizeEnum::WideArea: out << "WideArea"; break;
        case AreaSizeEnum::TallArea: out << "TallArea"; break;
      }
      if (i < 19) out << ",";
      out << "  // Map " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
  }
  
  void WriteTileData(std::ofstream& out, Overworld& overworld) {
    out << "// =============================================================================" << std::endl;
    out << "// Tile Data Information" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    out << "constexpr bool kGoldenExpandedTile16 = " 
        << (overworld.expanded_tile16() ? "true" : "false") << ";" << std::endl;
    out << "constexpr bool kGoldenExpandedTile32 = " 
        << (overworld.expanded_tile32() ? "true" : "false") << ";" << std::endl;
    out << std::endl;
    
    const auto& tiles16 = overworld.tiles16();
    const auto& tiles32 = overworld.tiles32_unique();
    
    out << "constexpr size_t kGoldenNumTiles16 = " << tiles16.size() << ";" << std::endl;
    out << "constexpr size_t kGoldenNumTiles32 = " << tiles32.size() << ";" << std::endl;
    out << std::endl;
    
    // Sample some tile data for validation
    out << "// Sample Tile16 data (first 10 tiles)" << std::endl;
    out << "constexpr std::array<uint32_t, 10> kGoldenTile16Sample = {{" << std::endl;
    for (int i = 0; i < std::min(10, static_cast<int>(tiles16.size())); i++) {
      // Extract tile data as uint32_t for sample using TileInfo values
      const auto& tile16 = tiles16[i];
      uint32_t sample = tile16.tile0_.id_ | (tile16.tile1_.id_ << 8) | 
                       (tile16.tile2_.id_ << 16) | (tile16.tile3_.id_ << 24);
      out << "    0x" << std::hex << std::setw(8) << std::setfill('0') << sample;
      if (i < 9) out << ",";
      out << "  // Tile16 " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
  }
  
  void WriteEntranceData(std::ofstream& out, Overworld& overworld) {
    out << "// =============================================================================" << std::endl;
    out << "// Entrance Data" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    const auto& entrances = overworld.entrances();
    out << "constexpr size_t kGoldenNumEntrances = " << entrances.size() << ";" << std::endl;
    out << std::endl;
    
    // Sample entrance data for validation
    out << "// Sample entrance data (first 10 entrances)" << std::endl;
    out << "constexpr std::array<uint16_t, 10> kGoldenEntranceMapPos = {{" << std::endl;
    for (int i = 0; i < std::min(10, static_cast<int>(entrances.size())); i++) {
      out << "    0x" << std::hex << std::setw(4) << std::setfill('0') 
          << entrances[i].map_pos_;
      if (i < 9) out << ",";
      out << "  // Entrance " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
    
    out << "constexpr std::array<uint16_t, 10> kGoldenEntranceMapId = {{" << std::endl;
    for (int i = 0; i < std::min(10, static_cast<int>(entrances.size())); i++) {
      out << "    0x" << std::hex << std::setw(4) << std::setfill('0') 
          << entrances[i].map_id_;
      if (i < 9) out << ",";
      out << "  // Entrance " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
    
    out << "constexpr std::array<int, 10> kGoldenEntranceX = {{" << std::endl;
    for (int i = 0; i < std::min(10, static_cast<int>(entrances.size())); i++) {
      out << "    " << std::dec << entrances[i].x_;
      if (i < 9) out << ",";
      out << "  // Entrance " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
    
    out << "constexpr std::array<int, 10> kGoldenEntranceY = {{" << std::endl;
    for (int i = 0; i < std::min(10, static_cast<int>(entrances.size())); i++) {
      out << "    " << std::dec << entrances[i].y_;
      if (i < 9) out << ",";
      out << "  // Entrance " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
  }
  
  void WriteHoleData(std::ofstream& out, Overworld& overworld) {
    out << "// =============================================================================" << std::endl;
    out << "// Hole Data" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    const auto& holes = overworld.holes();
    out << "constexpr size_t kGoldenNumHoles = " << holes.size() << ";" << std::endl;
    out << std::endl;
    
    // Sample hole data for validation
    out << "// Sample hole data (first 5 holes)" << std::endl;
    out << "constexpr std::array<uint16_t, 5> kGoldenHoleMapPos = {{" << std::endl;
    for (int i = 0; i < std::min(5, static_cast<int>(holes.size())); i++) {
      out << "    0x" << std::hex << std::setw(4) << std::setfill('0') 
          << holes[i].map_pos_;
      if (i < 4) out << ",";
      out << "  // Hole " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
  }
  
  void WriteExitData(std::ofstream& out, Overworld& overworld) {
    out << "// =============================================================================" << std::endl;
    out << "// Exit Data" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    const auto& exits = overworld.exits();
    out << "constexpr size_t kGoldenNumExits = " << exits->size() << ";" << std::endl;
    out << std::endl;
    
    // Sample exit data for validation
    out << "// Sample exit data (first 10 exits)" << std::endl;
    out << "constexpr std::array<uint16_t, 10> kGoldenExitRoomId = {{" << std::endl;
    for (int i = 0; i < std::min(10, static_cast<int>(exits->size())); i++) {
      out << "    0x" << std::hex << std::setw(4) << std::setfill('0') 
          << (*exits)[i].room_id_;
      if (i < 9) out << ",";
      out << "  // Exit " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
  }
  
  void WriteItemData(std::ofstream& out, Overworld& overworld) {
    out << "// =============================================================================" << std::endl;
    out << "// Item Data" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    const auto& items = overworld.all_items();
    out << "constexpr size_t kGoldenNumItems = " << items.size() << ";" << std::endl;
    out << std::endl;
    
    // Sample item data for validation
    if (!items.empty()) {
      out << "// Sample item data (first 10 items)" << std::endl;
      out << "constexpr std::array<uint8_t, 10> kGoldenItemIds = {{" << std::endl;
      for (int i = 0; i < std::min(10, static_cast<int>(items.size())); i++) {
        out << "    0x" << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(items[i].id_);
        if (i < 9) out << ",";
        out << "  // Item " << i << std::endl;
      }
      out << "}};" << std::endl;
      out << std::endl;
    }
  }
  
  void WriteSpriteData(std::ofstream& out, Overworld& overworld) {
    out << "// =============================================================================" << std::endl;
    out << "// Sprite Data" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    const auto& sprites = overworld.all_sprites();
    out << "constexpr size_t kGoldenNumSpriteStates = " << sprites.size() << ";" << std::endl;
    out << std::endl;
    
    // Sample sprite data for validation
    out << "// Sample sprite data (first 5 sprites from each state)" << std::endl;
    for (int state = 0; state < std::min(3, static_cast<int>(sprites.size())); state++) {
      out << "constexpr size_t kGoldenNumSpritesState" << state << " = " 
          << sprites[state].size() << ";" << std::endl;
    }
    out << std::endl;
  }
  
  void WriteMapTilesData(std::ofstream& out, Overworld& overworld) {
    out << "// =============================================================================" << std::endl;
    out << "// Map Tiles Data" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    const auto& map_tiles = overworld.map_tiles();
    out << "// Map tile dimensions" << std::endl;
    out << "constexpr size_t kGoldenMapTileWidth = " << map_tiles.light_world[0].size() << ";" << std::endl;
    out << "constexpr size_t kGoldenMapTileHeight = " << map_tiles.light_world.size() << ";" << std::endl;
    out << std::endl;
    
    // Sample map tile data for validation
    out << "// Sample map tile data (top-left 10x10 corner of Light World)" << std::endl;
    out << "constexpr std::array<std::array<uint16_t, 10>, 10> kGoldenMapTilesSample = {{" << std::endl;
    for (int row = 0; row < 10; row++) {
      out << "    {{";
      for (int col = 0; col < 10; col++) {
        out << "0x" << std::hex << std::setw(4) << std::setfill('0') 
            << map_tiles.light_world[row][col];
        if (col < 9) out << ", ";
      }
      out << "}}";
      if (row < 9) out << ",";
      out << "  // Row " << row << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
  }
  
  void WritePaletteData(std::ofstream& out, Rom& rom) {
    out << "// =============================================================================" << std::endl;
    out << "// Palette Data" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    // Sample palette data from ROM
    out << "// Sample palette data (first 10 bytes from overworld palette table)" << std::endl;
    out << "constexpr std::array<uint8_t, 10> kGoldenPaletteSample = {{" << std::endl;
    for (int i = 0; i < 10; i++) {
      auto palette_byte = rom.ReadByte(0x7D1C + i); // overworldMapPalette
      if (palette_byte.ok()) {
        out << "    0x" << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(*palette_byte);
      } else {
        out << "    0x00";
      }
      if (i < 9) out << ",";
      out << "  // Palette " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
  }
  
  void WriteMusicData(std::ofstream& out, Rom& rom) {
    out << "// =============================================================================" << std::endl;
    out << "// Music Data" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    // Sample music data from ROM
    out << "// Sample music data (first 10 bytes from overworld music table)" << std::endl;
    out << "constexpr std::array<uint8_t, 10> kGoldenMusicSample = {{" << std::endl;
    for (int i = 0; i < 10; i++) {
      auto music_byte = rom.ReadByte(0x14303 + i); // overworldMusicBegining
      if (music_byte.ok()) {
        out << "    0x" << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(*music_byte);
      } else {
        out << "    0x00";
      }
      if (i < 9) out << ",";
      out << "  // Music " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
  }
  
  void WriteOverlayData(std::ofstream& out, Rom& rom) {
    out << "// =============================================================================" << std::endl;
    out << "// Overlay Data" << std::endl;
    out << "// =============================================================================" << std::endl;
    out << std::endl;
    
    // Sample overlay data from ROM
    out << "// Sample overlay data (first 10 bytes from overlay pointers)" << std::endl;
    out << "constexpr std::array<uint8_t, 10> kGoldenOverlaySample = {{" << std::endl;
    for (int i = 0; i < 10; i++) {
      auto overlay_byte = rom.ReadByte(0x77664 + i); // overlayPointers
      if (overlay_byte.ok()) {
        out << "    0x" << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(*overlay_byte);
      } else {
        out << "    0x00";
      }
      if (i < 9) out << ",";
      out << "  // Overlay " << i << std::endl;
    }
    out << "}};" << std::endl;
    out << std::endl;
  }
  
  std::string rom_path_;
};

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <rom_path> <output_path>" << std::endl;
    std::cerr << "Example: " << argv[0] << " zelda3.sfc golden_data.h" << std::endl;
    return 1;
  }
  
  std::string rom_path = argv[1];
  std::string output_path = argv[2];
  
  if (!std::filesystem::exists(rom_path)) {
    std::cerr << "Error: ROM file not found: " << rom_path << std::endl;
    return 1;
  }
  
  OverworldGoldenDataExtractor extractor(rom_path);
  auto status = extractor.ExtractAllData(output_path);
  
  if (status.ok()) {
    std::cout << "Successfully extracted golden data from " << rom_path 
              << " to " << output_path << std::endl;
    return 0;
  } else {
    std::cerr << "Error extracting golden data: " << status.message() << std::endl;
    return 1;
  }
}
