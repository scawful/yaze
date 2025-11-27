#ifndef YAZE_APP_EMU_RENDER_RENDER_CONTEXT_H_
#define YAZE_APP_EMU_RENDER_RENDER_CONTEXT_H_

#include <cstdint>
#include <string>
#include <vector>

namespace yaze {
namespace emu {
namespace render {

// Type of entity to render
enum class RenderTargetType {
  kDungeonObject,  // Single dungeon object (Type 1/2/3)
  kSprite,         // Single sprite entity
  kFullRoom,       // Complete room with objects and sprites
};

// Request structure for rendering operations
struct RenderRequest {
  RenderTargetType type = RenderTargetType::kDungeonObject;

  // Entity identification
  int entity_id = 0;  // Object ID (0x000-0xFFF) or Sprite ID (0x00-0xFF)

  // Position and size (for objects)
  int x = 0;     // X position in tile coordinates (0-63)
  int y = 0;     // Y position in tile coordinates (0-63)
  int size = 0;  // Size parameter for scalable objects

  // Room context
  int room_id = 0;        // Dungeon room ID for graphics/palette context
  uint8_t blockset = 0;   // Graphics blockset override (0 = use room default)
  uint8_t palette = 0;    // Palette override (0 = use room default)
  uint8_t spriteset = 0;  // Spriteset override (0 = use room default)

  // Output dimensions
  int output_width = 256;
  int output_height = 256;

  // Use default room context if true (ignores blockset/palette/spriteset)
  bool use_room_defaults = true;
};

// Result of a render operation
struct RenderResult {
  bool success = false;
  std::string error;

  // Output pixel data (RGBA8888 format)
  std::vector<uint8_t> rgba_pixels;
  int width = 0;
  int height = 0;

  // Debug/diagnostic info
  int cycles_executed = 0;
  int handler_address = 0;
  bool used_static_fallback = false;
};

// Save state metadata for ROM compatibility checking
struct StateMetadata {
  uint32_t rom_checksum = 0;  // CRC32 of ROM
  uint8_t rom_region = 0;     // 0=US, 1=JP, 2=EU
  int room_id = 0;            // Current room when state was saved
  uint8_t game_module = 0;    // Game state module ($7E:0010)
  std::string description;
  uint32_t version = 1;       // State format version
};

// Type of baseline state
enum class StateType {
  kRoomLoaded,       // Game booted, dungeon room fully loaded
  kOverworldLoaded,  // Game booted, overworld area loaded
  kBlankCanvas,      // Minimal state for custom rendering
};

// LoROM address conversion helper
// ALTTP uses LoROM mapping where:
// - Banks $00-$3F: Address $8000-$FFFF maps to ROM
// - Each bank contributes 32KB ($8000 bytes)
// - PC = (bank & 0x7F) * 0x8000 + (addr - 0x8000)
inline uint32_t SnesToPc(uint32_t snes_addr) {
  uint8_t bank = (snes_addr >> 16) & 0xFF;
  uint16_t addr = snes_addr & 0xFFFF;
  if (addr >= 0x8000) {
    return (bank & 0x7F) * 0x8000 + (addr - 0x8000);
  }
  return snes_addr;
}

// Convert 8BPP linear tile data to 4BPP SNES planar format
// Input: 64 bytes per tile (1 byte per pixel, linear row-major order)
// Output: 32 bytes per tile (4 bitplanes interleaved per SNES 4BPP format)
inline std::vector<uint8_t> ConvertLinear8bppToPlanar4bpp(
    const std::vector<uint8_t>& linear_data) {
  size_t num_tiles = linear_data.size() / 64;  // 64 bytes per 8x8 tile
  std::vector<uint8_t> planar_data(num_tiles * 32);  // 32 bytes per tile

  for (size_t tile = 0; tile < num_tiles; ++tile) {
    const uint8_t* src = linear_data.data() + tile * 64;
    uint8_t* dst = planar_data.data() + tile * 32;

    for (int row = 0; row < 8; ++row) {
      uint8_t bp0 = 0, bp1 = 0, bp2 = 0, bp3 = 0;

      for (int col = 0; col < 8; ++col) {
        uint8_t pixel = src[row * 8 + col] & 0x0F;  // Low 4 bits only
        int bit = 7 - col;  // MSB first

        bp0 |= ((pixel >> 0) & 1) << bit;
        bp1 |= ((pixel >> 1) & 1) << bit;
        bp2 |= ((pixel >> 2) & 1) << bit;
        bp3 |= ((pixel >> 3) & 1) << bit;
      }

      // SNES 4BPP interleaving: bp0,bp1 for rows 0-7 first, then bp2,bp3
      dst[row * 2] = bp0;
      dst[row * 2 + 1] = bp1;
      dst[16 + row * 2] = bp2;
      dst[16 + row * 2 + 1] = bp3;
    }
  }

  return planar_data;
}

// Key ALTTP ROM addresses (SNES format)
namespace rom_addresses {

// Object handler tables (bank $01)
constexpr uint32_t kType1DataTable = 0x018000;
constexpr uint32_t kType1HandlerTable = 0x018200;
constexpr uint32_t kType2DataTable = 0x018370;
constexpr uint32_t kType2HandlerTable = 0x018470;
constexpr uint32_t kType3DataTable = 0x0184F0;
constexpr uint32_t kType3HandlerTable = 0x0185F0;

// Tile/graphics data
constexpr uint32_t kTileData = 0x009B52;

// Palette addresses
constexpr uint32_t kDungeonMainPalettes = 0x0DD734;
constexpr uint32_t kSpriteAuxPalettes = 0x0DD308;
constexpr uint32_t kSpritePalettesLW = 0x0DD218;
constexpr uint32_t kSpritePalettesDW = 0x0DD290;

// Sprite data
constexpr uint32_t kSpritePointerTable = 0x04C298;
constexpr uint32_t kSpriteBlocksetPointer = 0x005B57;

// Room data
constexpr uint32_t kRoomObjectPointer = 0x00874C;
constexpr uint32_t kRoomLayoutPointer = 0x00882D;

}  // namespace rom_addresses

// Key WRAM addresses
namespace wram_addresses {

// Tilemap buffers
constexpr uint32_t kBG1TilemapBuffer = 0x7E2000;
constexpr uint32_t kBG2TilemapBuffer = 0x7E4000;
constexpr uint32_t kTilemapBufferSize = 0x2000;

// Game state
constexpr uint32_t kRoomId = 0x7E00A0;
constexpr uint32_t kGameModule = 0x7E0010;

// Tilemap indirect pointers (zero page)
constexpr uint8_t kTilemapPointers[] = {0xBF, 0xC2, 0xC5, 0xC8, 0xCB,
                                        0xCE, 0xD1, 0xD4, 0xD7, 0xDA, 0xDD};
constexpr uint32_t kTilemapRowStride = 0x80;  // 64 tiles * 2 bytes

}  // namespace wram_addresses

// CRC32 calculation for ROM checksum verification
// Uses standard CRC32 polynomial 0xEDB88320
uint32_t CalculateCRC32(const uint8_t* data, size_t size);

}  // namespace render
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_RENDER_RENDER_CONTEXT_H_
