#ifndef YAZE_ZELDA3_GAME_DATA_H
#define YAZE_ZELDA3_GAME_DATA_H

#include <array>
#include <cstdint>
#include <map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "rom/rom.h"
#include "rom/rom_diagnostics.h"
#include "zelda.h"

namespace yaze {
namespace zelda3 {

// ============================================================================
// Graphics Constants
// ============================================================================

// Sheet counts
constexpr uint32_t kNumGfxSheets = 223;
constexpr uint32_t kNumLinkSheets = 14;

// Blockset/Group counts
constexpr uint32_t kNumMainBlocksets = 37;
constexpr uint32_t kNumRoomBlocksets = 82;
constexpr uint32_t kNumSpritesets = 144;
constexpr uint32_t kNumPalettesets = 72;

// ROM pointer locations
constexpr uint32_t kEntranceGfxGroup = 0x5D97;
// Note: kGfxGroupsPointer defined in zelda3/dungeon/dungeon_rom_addresses.h
constexpr uint32_t kMaxGraphics = 0x0C3FFF;

// Dungeon palette lookup tables
// Room headers store a "palette set ID" (0-71), NOT a direct palette index!
// Two-level lookup: paletteset_ids[room.palette][0] gives a byte offset into
// kDungeonPalettePointerTable. The word there, divided by 180, is the palette.
constexpr uint32_t kPalettesetIdsAddress = 0x75460;
constexpr uint32_t kDungeonPalettePointerTable = 0xDEC4B;

// Link graphics location ($10:8000)
constexpr uint32_t kLinkGfxOffset = 0x80000;
constexpr uint16_t kLinkGfxLength = 0x800;

// Font graphics location
constexpr uint32_t kFontSpriteLocation = 0x70000;

// Sheet sizes
constexpr uint32_t kUncompressedSheetSize = 0x0800;  // 2048 bytes

// Tile16 pointer location
constexpr uint32_t kTile16Ptr = 0x78000;

// Version constants map
static const std::map<zelda3_version, zelda3_version_pointers>
    kVersionConstantsMap = {
        {zelda3_version::US, zelda3_us_pointers},
        {zelda3_version::JP, zelda3_jp_pointers},
        {zelda3_version::SD, {}},
        {zelda3_version::RANDO, {}},
};

struct GameData {
  // Constructors
  GameData() = default;
  explicit GameData(Rom* rom) : rom_(rom) {}

  // ROM reference (non-owning)
  Rom* rom() const { return rom_; }
  void set_rom(Rom* rom) { rom_ = rom; }

  // Version info
  zelda3_version version = zelda3_version::US;
  std::string title;

  // Graphics Resources
  std::vector<uint8_t> graphics_buffer; // Legacy contiguous buffer
  std::array<std::vector<uint8_t>, kNumGfxSheets> raw_gfx_sheets; // 8BPP indexed
  std::array<gfx::Bitmap, kNumGfxSheets> gfx_bitmaps; // Renderable bitmaps
  std::array<gfx::Bitmap, kNumLinkSheets> link_graphics;
  gfx::Bitmap font_graphics;

  // Game Data Structures
  gfx::PaletteGroupMap palette_groups;

  std::array<std::array<uint8_t, 8>, kNumMainBlocksets> main_blockset_ids;
  std::array<std::array<uint8_t, 4>, kNumRoomBlocksets> room_blockset_ids;
  std::array<std::array<uint8_t, 4>, kNumSpritesets> spriteset_ids;

  // Palette set lookup table (72 entries × 4 bytes each)
  // Entry format: [bg_palette_offset, aux1, aux2, aux3]
  // NOTE: paletteset_ids[n][0] is a BYTE OFFSET into kDungeonPalettePointerTable,
  // NOT a direct palette index! See room.cc for correct lookup algorithm.
  std::array<std::array<uint8_t, 4>, kNumPalettesets> paletteset_ids;

  // Diagnostics
  GraphicsLoadDiagnostics diagnostics;

  void Clear() {
    graphics_buffer.clear();
    for (auto& sheet : raw_gfx_sheets) sheet.clear();
    // gfx_bitmaps don't need explicit clearing if reloaded
    palette_groups.clear();
  }

 private:
  Rom* rom_ = nullptr;
};

struct LoadOptions {
  bool load_graphics = true;
  bool load_palettes = true;
  bool load_gfx_groups = true;
  bool expand_rom = true;
  bool populate_metadata = true;
};

/**
 * @brief Loads all Zelda3-specific game data from a generic ROM.
 * @param rom The source ROM
 * @param data The destination GameData structure
 * @param options Loading configuration
 */
absl::Status LoadGameData(Rom& rom, GameData& data, const LoadOptions& options = {});

/**
 * @brief Saves modified game data back to the ROM.
 * @param rom The target ROM
 * @param data The source GameData
 */
absl::Status SaveGameData(Rom& rom, GameData& data);

// Individual loaders (internal use or fine-grained control)
absl::Status LoadMetadata(const Rom& rom, GameData& data);
absl::Status LoadPalettes(const Rom& rom, GameData& data);
absl::Status LoadGfxGroups(Rom& rom, GameData& data);
absl::Status LoadGraphics(Rom& rom, GameData& data);
absl::Status SaveGfxGroups(Rom& rom, const GameData& data);

/**
 * @brief Loads Link's graphics sheets from ROM.
 * @param rom The source ROM
 * @return Array of 14 Link graphics bitmaps, or error status
 */
absl::StatusOr<std::array<gfx::Bitmap, kNumLinkSheets>> LoadLinkGraphics(
    const Rom& rom);

/**
 * @brief Loads 2BPP graphics sheets from ROM.
 * @param rom The source ROM
 * @return Vector of 8BPP converted graphics data, or error status
 */
absl::StatusOr<std::vector<uint8_t>> Load2BppGraphics(const Rom& rom);

/**
 * @brief Loads font graphics from ROM.
 * @param rom The source ROM
 * @return Font graphics bitmap, or error status
 */
absl::StatusOr<gfx::Bitmap> LoadFontGraphics(const Rom& rom);

/**
 * @brief Saves all graphics sheets back to ROM.
 * @param rom The target ROM
 * @param sheets The graphics sheets to save
 * @return Status of the operation
 */
absl::Status SaveAllGraphicsData(
    Rom& rom, const std::array<gfx::Bitmap, kNumGfxSheets>& sheets);

/**
 * @brief Gets the graphics address for a sheet index.
 * @param data ROM data pointer
 * @param addr Sheet index
 * @param ptr1 Bank byte pointer table offset
 * @param ptr2 High byte pointer table offset
 * @param ptr3 Low byte pointer table offset
 * @param rom_size ROM size for bounds checking
 * @return PC offset for the graphics data
 */
uint32_t GetGraphicsAddress(const uint8_t* data, uint8_t addr, uint32_t ptr1,
                            uint32_t ptr2, uint32_t ptr3, size_t rom_size);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_GAME_DATA_H
