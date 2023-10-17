#include "app/emu/video/ppu.h"

#include <cstdint>
#include <iostream>
#include <vector>

#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {

using namespace PPURegisters;

void PPU::Update() {
  auto cycles_to_run = clock_.GetCycleCount();

  UpdateInternalState(cycles_to_run);

  // Render however many scanlines we're supposed to.
  if (current_scanline_ < visibleScanlines) {
    // Render the current scanline
    RenderScanline();

    // Increment the current scanline
    current_scanline_++;
  }
}

void PPU::UpdateInternalState(int cycles) {
  // Update the PPU's internal state based on the number of cycles
  cycle_count_ += cycles;

  // Check if it's time to move to the next scanline
  if (cycle_count_ >= cyclesPerScanline) {
    current_scanline_++;
    cycle_count_ -= cyclesPerScanline;

    // If we've reached the end of the frame, reset to the first scanline
    if (current_scanline_ >= totalScanlines) {
      current_scanline_ = 0;
    }
  }
}

void PPU::RenderScanline() {
  for (int y = 0; y < 240; ++y) {
    for (int x = 0; x < 256; ++x) {
      frame_buffer_[y * 256 + x] = (x + y) % 256;  // Simple gradient pattern
    }
  }

  // Fetch the tile data from VRAM, tile map data from memory, and palette data
  // from CGRAM
  UpdateTileData();     // Fetches the tile data from VRAM and stores it in an
                        // internal buffer
  UpdateTileMapData();  // Fetches the tile map data from memory and stores it
                        // in an internal buffer
  UpdatePaletteData();  // Fetches the palette data from CGRAM and stores it in
                        // an internal buffer

  // Render the background layers, taking into account the current mode and
  // layer priorities
  for (int layer = 1; layer <= 4; ++layer) {
    RenderBackground(layer);  // Renders the specified background layer into an
                              // internal layer buffer
  }

  // Render the sprite layer, taking into account sprite priorities and
  // transparency
  RenderSprites();  // Renders the sprite layer into an internal sprite buffer

  // Apply effects to the layers, such as scaling, rotation, and blending
  ApplyEffects();  // Applies effects to the layers based on the current mode
                   // and register settings

  // Combine the layers into a single image and store it in the frame buffer
  ComposeLayers();  // Combines the layers into a single image and stores it in
                    // the frame buffer

  // Display the frame buffer on the screen
  DisplayFrameBuffer();
}

void PPU::Notify(uint32_t address, uint8_t data) {
  // Handle communication in the PPU.
  if (address >= 0x2100 && address <= 0x213F) {
    // Handle register notification
    switch (address) {
      case PPURegisters::INIDISP:
        enable_forced_blanking_ = (data >> 7) & 0x01;
        break;
      case PPURegisters::BGMODE:
        // Update the PPU mode settings
        UpdateModeSettings();
        break;
    }
  }
}

void PPU::UpdateModeSettings() {
  // Read the PPU mode settings from the PPU registers
  uint8_t modeRegister = memory_.ReadByte(PPURegisters::INIDISP);

  // Mode is stored in the lower 3 bits
  auto mode = static_cast<BackgroundMode>(modeRegister & 0x07);

  // Update the tilemap, tile data, and palette settings
  switch (mode) {
    case BackgroundMode::Mode0:
      // Mode 0: 4 layers, each 2bpp (4 colors)
      break;

    case BackgroundMode::Mode1:
      // Mode 1: 2 layers, 4bpp (16 colors), 1 layer, 2bpp (4 colors)
      break;

    case BackgroundMode::Mode2:
      // Mode 2: 2 layers, 4bpp (16 colors), 1 layer for offset-per-tile
      break;

    case BackgroundMode::Mode3:
      // Mode 3: 1 layer, 8bpp (256 colors), 1 layer, 4bpp (16 colors)
      break;

    case BackgroundMode::Mode4:
      // Mode 4: 1 layer, 8bpp (256 colors), 1 layer, 2bpp (4 colors)
      break;

    case BackgroundMode::Mode5:
      // Mode 5: 1 layer, 4bpp (16 colors), 1 layer, 2bpp (4 colors) hi-res
      break;

    case BackgroundMode::Mode6:
      // Mode 6: 1 layer, 4bpp (16 colors), 1 layer for offset-per-tile, hi-res
      break;

    case BackgroundMode::Mode7:
      // Mode 7: 1 layer, 8bpp (256 colors), rotation/scaling
      break;

    default:
      // Invalid mode setting, handle the error or set default settings
      // ...
      break;
  }

  // Update the internal state of the PPU based on the mode settings
  // Update tile data, tilemaps, sprites, and palette based on the mode settings
  UpdateTileData();
  UpdatePaletteData();
}

// Internal methods to handle PPU rendering and operations
void PPU::UpdateTileData() {
  // Fetch tile data from VRAM and store it in the internal buffer
  for (uint16_t address = 0; address < tile_data_size_; ++address) {
    tile_data_[address] = memory_.ReadByte(vram_base_address_ + address);
  }

  // Update the tilemap entries based on the fetched tile data
  for (uint16_t entryIndex = 0; entryIndex < tilemap_.entries.size();
       ++entryIndex) {
    uint16_t tilemapAddress =
        tilemap_base_address_ + entryIndex * sizeof(TilemapEntry);
    // Assume ReadWord reads a 16-bit value from VRAM
    uint16_t tileData = memory_.ReadWord(tilemapAddress);

    // Extract tilemap entry attributes from the tile data
    TilemapEntry entry;
    // Tile number is stored in the lower 10 bits
    entry.tileNumber = tileData & 0x03FF;

    // Palette is stored in bits 10-12
    entry.palette = (tileData >> 10) & 0x07;

    // Priority is stored in bit 13
    entry.priority = (tileData >> 13) & 0x01;

    // Horizontal flip is stored in bit 14
    entry.hFlip = (tileData >> 14) & 0x01;

    // Vertical flip is stored in bit 15
    entry.vFlip = (tileData >> 15) & 0x01;

    tilemap_.entries[entryIndex] = entry;
  }

  // Update the sprites based on the fetched tile data
  for (uint16_t spriteIndex = 0; spriteIndex < sprites_.size(); ++spriteIndex) {
    uint16_t spriteAddress =
        oam_address_ + spriteIndex * sizeof(SpriteAttributes);
    // Assume ReadWord reads a 16-bit value from VRAM
    uint16_t spriteData = memory_.ReadWord(spriteAddress);

    // Extract sprite attributes from the sprite data
    SpriteAttributes sprite;

    sprite.x = memory_.ReadByte(spriteAddress);
    sprite.y = memory_.ReadByte(spriteAddress + 1);

    // Tile number is stored in the lower 9
    sprite.tile = spriteData & 0x01FF;

    // bits Palette is stored in bits 9-11
    sprite.palette = (spriteData >> 9) & 0x07;

    // Priority is stored in bits 12-13
    sprite.priority = (spriteData >> 12) & 0x03;

    // Horizontal flip is stored in bit 14
    sprite.hFlip = (spriteData >> 14) & 0x01;

    // Vertical flip is stored in bit 15
    sprite.vFlip = (spriteData >> 15) & 0x01;

    sprites_[spriteIndex] = sprite;
  }
}

void PPU::UpdateTileMapData() {}

void PPU::RenderBackground(int layer) {
  auto bg1_tilemap_info = BGSC(0);
  auto bg1_chr_data = BGNBA(0);
  auto bg2_tilemap_info = BGSC(0);
  auto bg2_chr_data = BGNBA(0);
  auto bg3_tilemap_info = BGSC(0);
  auto bg3_chr_data = BGNBA(0);
  auto bg4_tilemap_info = BGSC(0);
  auto bg4_chr_data = BGNBA(0);

  switch (layer) {
    case 1:
      // Render the first background layer
      bg1_tilemap_info = BGSC(memory_.ReadByte(BG1SC));
      bg1_chr_data = BGNBA(memory_.ReadByte(BG12NBA));
      break;
    case 2:
      // Render the second background layer
      bg2_tilemap_info = BGSC(memory_.ReadByte(BG2SC));
      bg2_chr_data = BGNBA(memory_.ReadByte(BG12NBA));
      break;
    case 3:
      // Render the third background layer
      bg3_tilemap_info = BGSC(memory_.ReadByte(BG3SC));
      bg3_chr_data = BGNBA(memory_.ReadByte(BG34NBA));
      break;
    case 4:
      // Render the fourth background layer
      bg4_tilemap_info = BGSC(memory_.ReadByte(BG4SC));
      bg4_chr_data = BGNBA(memory_.ReadByte(BG34NBA));
      break;
    default:
      // Invalid layer, do nothing
      break;
  }
}

void PPU::RenderSprites() {
  // ...
}

void PPU::UpdatePaletteData() {}

void PPU::ApplyEffects() {}

void PPU::ComposeLayers() {}

void PPU::DisplayFrameBuffer() {}

}  // namespace emu
}  // namespace app
}  // namespace yaze
