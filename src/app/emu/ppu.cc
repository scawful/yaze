#include "app/emu/ppu.h"

#include <cstdint>
#include <iostream>
#include <vector>

#include "app/emu/mem.h"

namespace yaze {
namespace app {
namespace emu {

PPU::PPU(Memory& memory) : memory_(memory) {}

void PPU::RenderScanline() {
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
  DisplayFrameBuffer();  // Sends the frame buffer to the display hardware
                         // (e.g., SDL2)
}

void PPU::Update() {
  auto cycles_to_run = GetCycleCount();

  UpdateInternalState(cycles_to_run);

  // Render however many scanlines we're supposed to.
  if (currentScanline < visibleScanlines) {
    // Render the current scanline
    // This involves fetching tile data, applying palette colors, handling
    // sprite priorities, etc.
    RenderScanline();

    // Increment the current scanline
    currentScanline++;
  }
}

void PPU::UpdateInternalState(int cycles) {
  // Update the PPU's internal state based on the number of cycles
  cycleCount += cycles;

  // Check if it's time to move to the next scanline
  if (cycleCount >= cyclesPerScanline) {
    currentScanline++;
    cycleCount -= cyclesPerScanline;

    // If we've reached the end of the frame, reset to the first scanline
    if (currentScanline >= totalScanlines) {
      currentScanline = 0;
    }
  }
}

// Reads a byte from the specified PPU register
uint8_t PPU::ReadRegister(uint16_t address) {
  switch (address) {
    case 0x2102:  // OAM Address Register (low byte)
      return oam_address_ & 0xFF;
    case 0x2103:  // OAM Address Register (high byte)
      return (oam_address_ >> 8) & 0xFF;
    // ... handle other PPU registers
    default:
      // Invalid register address, return 0
      return 0;
  }
}

// Writes a byte to the specified PPU register
void PPU::WriteRegister(uint16_t address, uint8_t value) {
  switch (address) {
    case 0x2102:  // OAM Address Register (low byte)
      oam_address_ = (oam_address_ & 0xFF00) | value;
      break;
    case 0x2103:  // OAM Address Register (high byte)
      oam_address_ = (oam_address_ & 0x00FF) | (value << 8);
      break;
    // ... handle other PPU registers
    default:
      // Invalid register address, do nothing
      break;
  }
}

void PPU::UpdateModeSettings() {
  // Read the PPU mode settings from the PPU registers
  uint8_t modeRegister = ReadRegister(PPURegisters::INIDISP);

  // Extract the PPU mode and other relevant settings from the register value
  BackgroundMode mode = static_cast<BackgroundMode>(
      modeRegister & 0x07);  // Mode is stored in the lower 3 bits
  bool backgroundEnabled =
      (modeRegister >> 7) & 0x01;  // Background enabled flag is stored in bit 7
  bool spritesEnabled =
      (modeRegister >> 6) & 0x01;  // Sprites enabled flag is stored in bit 6

  // Update the internal mode settings based on the extracted values
  // modeSettings_.backgroundEnabled = backgroundEnabled;
  // modeSettings_.spritesEnabled = spritesEnabled;

  // Update the tilemap, tile data, and palette settings based on the selected
  // mode
  switch (mode) {
    case BackgroundMode::Mode0:
      // Mode 0: 4 background layers, all with 2bpp
      // Update the tilemap, tile data, and palette settings accordingly
      // ...
      break;

    case BackgroundMode::Mode1:
      // Mode 1: 3 background layers (2 with 4bpp, 1 with 2bpp)
      // Update the tilemap, tile data, and palette settings accordingly
      // ...
      break;

      // Handle other modes and update the settings accordingly
      // ...

    default:
      // Invalid mode setting, handle the error or set default settings
      // ...
      break;
  }

  // Update the internal state of the PPU based on the mode settings
  // Update tile data, tilemaps, sprites, and palette based on the mode settings
  UpdateTileData();
  UpdatePaletteData();
  // ...
}

void PPU::RenderBackground(int layer) {
  // ...
}

void PPU::RenderSprites() {
  // ...
}

uint32_t PPU::GetPaletteColor(uint8_t colorIndex) {
  return memory_.ReadWordLong(colorIndex);
}

uint8_t PPU::ReadVRAM(uint16_t address) {
  // ...
}

void PPU::WriteVRAM(uint16_t address, uint8_t value) {
  // ...
}

uint8_t PPU::ReadOAM(uint16_t address) { return memory_.ReadByte(address); }

void PPU::WriteOAM(uint16_t address, uint8_t value) {
  // ...
}

uint8_t PPU::ReadCGRAM(uint16_t address) { return memory_.ReadByte(address); }

void PPU::WriteCGRAM(uint16_t address, uint8_t value) {
  // ...
}

// Internal methods to handle PPU rendering and operations
void PPU::UpdateTileData() {
  // Fetch tile data from VRAM and store it in the internal buffer
  for (uint16_t address = 0; address < tileDataSize_; ++address) {
    tileData_[address] = memory_.ReadByte(vramBaseAddress_ + address);
  }

  // Update the tilemap entries based on the fetched tile data
  for (uint16_t entryIndex = 0; entryIndex < tilemap_.entries.size();
       ++entryIndex) {
    uint16_t tilemapAddress =
        tilemapBaseAddress_ + entryIndex * sizeof(TilemapEntry);
    uint16_t tileData = memory_.ReadWord(
        tilemapAddress);  // Assume ReadWord reads a 16-bit value from VRAM

    // Extract tilemap entry attributes from the tile data
    TilemapEntry entry;
    entry.tileNumber =
        tileData & 0x03FF;  // Tile number is stored in the lower 10 bits
    entry.palette = (tileData >> 10) & 0x07;  // Palette is stored in bits 10-12
    entry.priority = (tileData >> 13) & 0x01;  // Priority is stored in bit 13
    entry.hFlip =
        (tileData >> 14) & 0x01;  // Horizontal flip is stored in bit 14
    entry.vFlip = (tileData >> 15) & 0x01;  // Vertical flip is stored in bit 15

    tilemap_.entries[entryIndex] = entry;
  }

  // Update the sprites based on the fetched tile data
  for (uint16_t spriteIndex = 0; spriteIndex < sprites_.size(); ++spriteIndex) {
    uint16_t spriteAddress =
        oam_address_ + spriteIndex * sizeof(SpriteAttributes);
    uint16_t spriteData = memory_.ReadWord(
        spriteAddress);  // Assume ReadWord reads a 16-bit value from VRAM

    // Extract sprite attributes from the sprite data
    SpriteAttributes sprite;
    sprite.x = memory_.ReadByte(spriteAddress);
    sprite.y = memory_.ReadByte(spriteAddress + 1);
    sprite.tile =
        spriteData & 0x01FF;  // Tile number is stored in the lower 9 bits
    sprite.palette =
        (spriteData >> 9) & 0x07;  // Palette is stored in bits 9-11
    sprite.priority =
        (spriteData >> 12) & 0x03;  // Priority is stored in bits 12-13
    sprite.hFlip =
        (spriteData >> 14) & 0x01;  // Horizontal flip is stored in bit 14
    sprite.vFlip =
        (spriteData >> 15) & 0x01;  // Vertical flip is stored in bit 15

    sprites_[spriteIndex] = sprite;
  }
}

}  // namespace emu
}  // namespace app
}  // namespace yaze
