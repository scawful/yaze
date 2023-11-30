#include "app/emu/video/ppu.h"

#include <cstdint>
#include <iostream>
#include <vector>

#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {

using namespace PpuRegisters;

void Ppu::Update() {
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

void Ppu::UpdateInternalState(int cycles) {
  // Update the Ppu's internal state based on the number of cycles
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

void Ppu::RenderScanline() {
  for (int y = 0; y < 240; ++y) {
    for (int x = 0; x < 256; ++x) {
      // Calculate the color index based on the x and y coordinates
      uint8_t color_index = (x + y) % 8;

      // Set the pixel in the frame buffer to the calculated color index
      frame_buffer_[y * 256 + x] = color_index;
    }
  }

  // Fetch the tile data from VRAM, tile map data from memory, and palette data
  // from CGRAM
  // UpdateTileData();     // Fetches the tile data from VRAM and stores it in an
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

void Ppu::Notify(uint32_t address, uint8_t data) {
  // Handle communication in the Ppu.
  if (address >= 0x2100 && address <= 0x213F) {
    // Handle register notification
    switch (address) {
      case INIDISP:
        enable_forced_blanking_ = (data >> 7) & 0x01;
        break;
      case OBJSEL:
        oam_size_.base_selection = (data >> 2) & 0x03;
        oam_size_.name_selection = (data >> 4) & 0x07;
        oam_size_.object_size = data & 0x03;
        break;
      case OAMADDL:
        oam_address_.oam_address_low = data;
        break;
      case OAMADDH:
        oam_address_.oam_address_msb = data & 0x01;
        oam_address_.oam_priority_rotation = (data >> 1) & 0x01;
        break;
      case OAMDATA:
        // Write the data to OAM
        break;
      case BGMODE:
        // Update the Ppu mode settings
        UpdateModeSettings();
        break;
      case MOSAIC:
        mosaic_.bg_enable = (data >> 7) & 0x01;
        mosaic_.mosaic_size = data & 0x0F;
        break;
      case BG1SC:
        bgsc_[0] = BGSC(data);
        break;
      case BG2SC:
        bgsc_[1] = BGSC(data);
        break;
      case BG3SC:
        bgsc_[2] = BGSC(data);
        break;
      case BG4SC:
        bgsc_[3] = BGSC(data);
        break;
      case BG12NBA:
        bgnba_[0] = BGNBA(data);
        break;
      case BG34NBA:
        bgnba_[1] = BGNBA(data);
        break;
      case BG1HOFS:
        bghofs_[0].horizontal_scroll = data;
        break;
      case BG2HOFS:
        bghofs_[1].horizontal_scroll = data;
        break;
      case BG3HOFS:
        bghofs_[2].horizontal_scroll = data;
        break;
      case BG4HOFS:
        bghofs_[3].horizontal_scroll = data;
        break;
      case BG1VOFS:
        bgvofs_[0].vertical_scroll = data;
        break;
      case BG2VOFS:
        bgvofs_[1].vertical_scroll = data;
        break;
      case BG3VOFS:
        bgvofs_[2].vertical_scroll = data;
        break;
      case BG4VOFS:
        bgvofs_[3].vertical_scroll = data;
        break;
      case VMAIN:
        vmain_.increment_size = data & 0x03;
        vmain_.remapping = (data >> 2) & 0x03;
        vmain_.address_increment_mode = (data >> 4) & 0x01;
        break;
      case VMADDL:
        vmaddl_.address_low = data;
        break;
      case VMADDH:
        vmaddh_.address_high = data;
        break;
      case M7SEL:
        m7sel_.flip_horizontal = data & 0x01;
        m7sel_.flip_vertical = (data >> 1) & 0x01;
        m7sel_.fill = (data >> 2) & 0x01;
        m7sel_.tilemap_repeat = (data >> 3) & 0x01;
        break;
      case M7A:
        m7a_.matrix_a = data;
        break;
      case M7B:
        m7b_.matrix_b = data;
        break;
      case M7C:
        m7c_.matrix_c = data;
        break;
      case M7D:
        m7d_.matrix_d = data;
        break;
      case M7X:
        m7x_.center_x = data;
        break;
      case M7Y:
        m7y_.center_y = data;
        break;
      case CGADD:
        cgadd_.address = data;
        break;
      case CGDATA:
        // Write the data to CGRAM
        break;
      case W12SEL:
        w12sel_.enable_bg1_a = data & 0x01;
        w12sel_.invert_bg1_a = (data >> 1) & 0x01;
        w12sel_.enable_bg1_b = (data >> 2) & 0x01;
        w12sel_.invert_bg1_b = (data >> 3) & 0x01;
        w12sel_.enable_bg2_c = (data >> 4) & 0x01;
        w12sel_.invert_bg2_c = (data >> 5) & 0x01;
        w12sel_.enable_bg2_d = (data >> 6) & 0x01;
        w12sel_.invert_bg2_d = (data >> 7) & 0x01;
        break;
      case W34SEL:
        w34sel_.enable_bg3_e = data & 0x01;
        w34sel_.invert_bg3_e = (data >> 1) & 0x01;
        w34sel_.enable_bg3_f = (data >> 2) & 0x01;
        w34sel_.invert_bg3_f = (data >> 3) & 0x01;
        w34sel_.enable_bg4_g = (data >> 4) & 0x01;
        w34sel_.invert_bg4_g = (data >> 5) & 0x01;
        w34sel_.enable_bg4_h = (data >> 6) & 0x01;
        w34sel_.invert_bg4_h = (data >> 7) & 0x01;
        break;
      case WOBJSEL:
        wobjsel_.enable_obj_i = data & 0x01;
        wobjsel_.invert_obj_i = (data >> 1) & 0x01;
        wobjsel_.enable_obj_j = (data >> 2) & 0x01;
        wobjsel_.invert_obj_j = (data >> 3) & 0x01;
        wobjsel_.enable_color_k = (data >> 4) & 0x01;
        wobjsel_.invert_color_k = (data >> 5) & 0x01;
        wobjsel_.enable_color_l = (data >> 6) & 0x01;
        wobjsel_.invert_color_l = (data >> 7) & 0x01;
        break;
      case WH0:
        wh0_.left_position = data;
        break;
      case WH1:
        wh1_.right_position = data;
        break;
      case WH2:
        wh2_.left_position = data;
        break;
      case WH3:
        wh3_.right_position = data;
        break;
      case TM:
        tm_.enable_layer = (data >> 5) & 0x01;  //
        break;
      case TS:
        ts_.enable_layer = (data >> 5) & 0x01;
        break;
      case TMW:
        tmw_.enable_window = (data >> 5) & 0x01;
        break;
      case TSW:
        tsw_.enable_window = (data >> 5) & 0x01;
        break;
    }
  }
}

void Ppu::UpdateModeSettings() {
  // Read the Ppu mode settings from the Ppu registers
  uint8_t modeRegister = memory_.ReadByte(PpuRegisters::INIDISP);

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

  // Update the internal state of the Ppu based on the mode settings
  // Update tile data, tilemaps, sprites, and palette based on the mode settings
  UpdateTileData();
  UpdatePaletteData();
}

// Internal methods to handle Ppu rendering and operations
void Ppu::UpdateTileData() {
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
    uint16_t spriteAddress = spriteIndex * sizeof(SpriteAttributes);
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

void Ppu::UpdateTileMapData() {}

void Ppu::RenderBackground(int layer) {
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

void Ppu::RenderSprites() {}

void Ppu::UpdatePaletteData() {}

void Ppu::ApplyEffects() {}

void Ppu::ComposeLayers() {}

void Ppu::DisplayFrameBuffer() {
  if (!screen_->IsActive()) {
    screen_->Create(256, 240, 24, frame_buffer_);
    rom()->RenderBitmap(screen_.get());
  }
}

}  // namespace emu
}  // namespace app
}  // namespace yaze
