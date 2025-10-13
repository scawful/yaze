#include "title_screen.h"

#include <cstdint>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "app/rom.h"
#include "app/snes.h"

namespace yaze {
namespace zelda3 {

absl::Status TitleScreen::Create(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM is not loaded");
  }

  // Initialize bitmaps for each layer
  tiles8_bitmap_.Create(128, 512, 8, std::vector<uint8_t>(0x20000));
  tiles_bg1_bitmap_.Create(256, 256, 8, std::vector<uint8_t>(0x80000));
  tiles_bg2_bitmap_.Create(256, 256, 8, std::vector<uint8_t>(0x80000));
  oam_bg_bitmap_.Create(256, 256, 8, std::vector<uint8_t>(0x80000));
  
  // Set metadata for title screen bitmaps
  // Title screen uses 3BPP graphics with 8 palettes of 8 colors (64 total)
  tiles8_bitmap_.metadata().source_bpp = 3;
  tiles8_bitmap_.metadata().palette_format = 0;  // Full 64-color palette
  tiles8_bitmap_.metadata().source_type = "graphics_sheet";
  tiles8_bitmap_.metadata().palette_colors = 64;
  
  tiles_bg1_bitmap_.metadata().source_bpp = 3;
  tiles_bg1_bitmap_.metadata().palette_format = 0;  // Uses full palette with sub-palette indexing
  tiles_bg1_bitmap_.metadata().source_type = "screen_buffer";
  tiles_bg1_bitmap_.metadata().palette_colors = 64;
  
  tiles_bg2_bitmap_.metadata().source_bpp = 3;
  tiles_bg2_bitmap_.metadata().palette_format = 0;
  tiles_bg2_bitmap_.metadata().source_type = "screen_buffer";
  tiles_bg2_bitmap_.metadata().palette_colors = 64;
  
  oam_bg_bitmap_.metadata().source_bpp = 3;
  oam_bg_bitmap_.metadata().palette_format = 0;
  oam_bg_bitmap_.metadata().source_type = "screen_buffer";
  oam_bg_bitmap_.metadata().palette_colors = 64;

  // Initialize tilemap buffers
  tiles_bg1_buffer_.fill(0x492);  // Default empty tile
  tiles_bg2_buffer_.fill(0x492);

  // Load palette (title screen uses 3BPP graphics with 8 palettes of 8 colors each)
  // Build a full 64-color palette from sprite palettes
  auto sprite_pal_group = rom->palette_group().sprites_aux1;
  
  // Title screen needs 8 palettes (64 colors total for 3BPP mode)
  // Each palette in sprites_aux1 has 8 colors (7 actual + 1 transparent)
  for (int pal = 0; pal < 8 && pal < sprite_pal_group.size(); pal++) {
    auto sub_palette = sprite_pal_group[pal];
    for (int col = 0; col < sub_palette.size(); col++) {
      palette_.AddColor(sub_palette[col]);
    }
  }

  // Build tile16 blockset from graphics
  RETURN_IF_ERROR(BuildTileset(rom));

  // Load tilemap data from ROM
  RETURN_IF_ERROR(LoadTitleScreen(rom));

  return absl::OkStatus();
}

absl::Status TitleScreen::BuildTileset(Rom* rom) {
  // Title screen uses specific graphics sheets
  // Sheet arrangement for title screen (from ALTTP disassembly):
  // 8-15: Graphics sheets 115, 115+6, 115+7, 112, etc.
  uint8_t staticgfx[16] = {0};

  // Title screen specific graphics sheets
  staticgfx[8] = 115 + 0;   // Title logo graphics
  staticgfx[9] = 115 + 3;   // Sprite graphics
  staticgfx[10] = 115 + 6;  // Additional graphics
  staticgfx[11] = 115 + 7;  // Additional graphics
  staticgfx[12] = 115 + 0;  // More sprite graphics
  staticgfx[13] = 112;      // UI graphics
  staticgfx[14] = 112;      // UI graphics
  staticgfx[15] = 112;      // UI graphics

  // Get ROM graphics buffer (contains 3BPP/4BPP SNES format data)
  const auto& gfx_buffer = rom->graphics_buffer();
  auto& tiles8_data = tiles8_bitmap_.mutable_data();

  // Load and convert each graphics sheet from 3BPP SNES format to 8BPP indexed
  // Each sheet is 2048 bytes in 3BPP format -> converts to 0x1000 bytes (4096) in 8BPP
  for (int i = 0; i < 16; i++) {
    int sheet_id = staticgfx[i];
    int source_offset = sheet_id * 2048;

    if (source_offset + 2048 <= gfx_buffer.size()) {
      // Extract 3BPP sheet data
      std::vector<uint8_t> sheet_3bpp(gfx_buffer.begin() + source_offset,
                                       gfx_buffer.begin() + source_offset + 2048);

      // Convert 3BPP SNES format to 8BPP indexed format
      auto sheet_8bpp = gfx::SnesTo8bppSheet(sheet_3bpp, 3, 1);

      // Copy converted data to tiles8_bitmap at correct position
      // Each converted sheet is 0x1000 bytes (128x32 pixels)
      int dest_offset = i * 0x1000;
      if (dest_offset + sheet_8bpp.size() <= tiles8_data.size()) {
        std::copy(sheet_8bpp.begin(), sheet_8bpp.end(),
                  tiles8_data.begin() + dest_offset);
      }
    }
  }

  // Set palette on tiles8 bitmap
  tiles8_bitmap_.SetPalette(palette_);

  // Queue texture creation via Arena's deferred system
  gfx::Arena::Get().QueueTextureCommand(
      gfx::Arena::TextureCommandType::CREATE, &tiles8_bitmap_);

  // TODO: Build tile16 blockset from tile8 data
  // This would involve composing 16x16 tiles from 8x8 tiles
  // For now, we'll use the tile8 data directly

  return absl::OkStatus();
}

absl::Status TitleScreen::LoadTitleScreen(Rom* rom) {
  // Title screen tilemap data addresses (PC format)
  // These point to tilemap data for different screen sections
  constexpr int kTilemapAddresses[7] = {
      0x53de4, 0x53e2c, 0x53e08, 0x53e50, 0x53e74, 0x53e98, 0x53ebc};
  constexpr int kTilemapGfxAddresses[7] = {
      0x53ee0, 0x53f04, 0x53ef2, 0x53f16, 0x53f28, 0x53f3a, 0x53f4c};

  // Note: The title screen uses a simpler tilemap format than dungeons
  // For now, we'll use the compressed format loader which should work
  // TODO: Implement proper title screen tilemap loading using the addresses above

  ASSIGN_OR_RETURN(uint8_t byte0, rom->ReadByte(0x137A + 3));
  ASSIGN_OR_RETURN(uint8_t byte1, rom->ReadByte(0x1383 + 3));
  ASSIGN_OR_RETURN(uint8_t byte2, rom->ReadByte(0x138C + 3));

  int pos = (byte2 << 16) + (byte1 << 8) + byte0;
  pos = SnesToPc(pos);

  // Initialize buffers with default empty tile
  for (int i = 0; i < 1024; i++) {
    tiles_bg1_buffer_[i] = 0x492;
    tiles_bg2_buffer_[i] = 0x492;
  }

  // Read compressed tilemap data
  // Format: destination address (word), length (word), tile data
  while (pos < rom->size()) {
    ASSIGN_OR_RETURN(uint8_t first_byte, rom->ReadByte(pos));
    if ((first_byte & 0x80) == 0x80) {
      break;  // End of data marker
    }

    ASSIGN_OR_RETURN(uint16_t dest_addr, rom->ReadWord(pos));
    pos += 2;

    ASSIGN_OR_RETURN(uint16_t length_flags, rom->ReadWord(pos));
    pos += 2;

    bool increment64 = (length_flags & 0x8000) == 0x8000;
    bool fixsource = (length_flags & 0x4000) == 0x4000;
    int length = (length_flags & 0x07FF);

    int posB = pos;
    for (int j = 0; j < (length / 2) + 1; j++) {
      ASSIGN_OR_RETURN(uint16_t tiledata, rom->ReadWord(pos));

      // Determine which layer this tile belongs to
      if (dest_addr >= 0x1000 && dest_addr < 0x2000) {
        // BG1 layer
        tiles_bg1_buffer_[dest_addr - 0x1000] = tiledata;
      } else if (dest_addr < 0x1000) {
        // BG2 layer
        tiles_bg2_buffer_[dest_addr] = tiledata;
      }

      // Advance destination address
      if (increment64) {
        dest_addr += 32;
      } else {
        dest_addr++;
      }

      // Advance source position
      if (!fixsource) {
        pos += 2;
      }
    }

    if (fixsource) {
      pos += 2;
    } else {
      pos = posB + ((length / 2) + 1) * 2;
    }
  }

  pal_selected_ = 2;

  // Render tilemaps into bitmap pixels
  RETURN_IF_ERROR(RenderBG1Layer());
  RETURN_IF_ERROR(RenderBG2Layer());

  // Apply palettes to layer bitmaps AFTER rendering
  tiles_bg1_bitmap_.SetPalette(palette_);
  tiles_bg2_bitmap_.SetPalette(palette_);
  oam_bg_bitmap_.SetPalette(palette_);
  
  // Ensure bitmaps are marked as active
  tiles_bg1_bitmap_.set_active(true);
  tiles_bg2_bitmap_.set_active(true);
  oam_bg_bitmap_.set_active(true);

  // Queue texture creation for all layer bitmaps
  gfx::Arena::Get().QueueTextureCommand(
      gfx::Arena::TextureCommandType::CREATE, &tiles_bg1_bitmap_);
  gfx::Arena::Get().QueueTextureCommand(
      gfx::Arena::TextureCommandType::CREATE, &tiles_bg2_bitmap_);
  gfx::Arena::Get().QueueTextureCommand(
      gfx::Arena::TextureCommandType::CREATE, &oam_bg_bitmap_);

  return absl::OkStatus();
}

absl::Status TitleScreen::RenderBG1Layer() {
  // BG1 layer is 32x32 tiles (256x256 pixels)
  auto& bg1_data = tiles_bg1_bitmap_.mutable_data();
  const auto& tile8_bitmap_data = tiles8_bitmap_.vector();

  // Render each tile in the 32x32 tilemap
  for (int tile_y = 0; tile_y < 32; tile_y++) {
    for (int tile_x = 0; tile_x < 32; tile_x++) {
      int tilemap_index = tile_y * 32 + tile_x;
      uint16_t tile_word = tiles_bg1_buffer_[tilemap_index];

      // Extract tile info from SNES tile word (vhopppcc cccccccc format)
      int tile_id = tile_word & 0x3FF;  // Bits 0-9: tile ID
      int palette = (tile_word >> 10) & 0x07;  // Bits 10-12: palette
      bool h_flip = (tile_word & 0x4000) != 0;  // Bit 14: horizontal flip
      bool v_flip = (tile_word & 0x8000) != 0;  // Bit 15: vertical flip

      // Calculate source position in tiles8_bitmap_ (16 tiles per row, 8x8 each)
      int src_tile_x = (tile_id % 16) * 8;
      int src_tile_y = (tile_id / 16) * 8;

      // Copy 8x8 tile pixels from tile8 bitmap to BG1 bitmap
      for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
          // Apply flipping
          int src_px = h_flip ? (7 - px) : px;
          int src_py = v_flip ? (7 - py) : py;

          // Calculate source and destination positions
          int src_x = src_tile_x + src_px;
          int src_y = src_tile_y + src_py;
          int src_pos = src_y * 128 + src_x;  // tiles8_bitmap_ is 128 pixels wide

          int dest_x = tile_x * 8 + px;
          int dest_y = tile_y * 8 + py;
          int dest_pos = dest_y * 256 + dest_x;  // BG1 is 256 pixels wide

          // Copy pixel with palette application
          // Title screen uses 3BPP graphics (8 colors per palette)
          if (src_pos < tile8_bitmap_data.size() && dest_pos < bg1_data.size()) {
            uint8_t pixel_value = tile8_bitmap_data[src_pos];
            // Apply palette index (title screen uses 8-color palettes)
            // Mask to 3 bits for 8 colors, then apply palette offset
            bg1_data[dest_pos] = (pixel_value & 0x07) | ((palette & 0x07) << 3);
          }
        }
      }
    }
  }
  
  // Update surface with rendered pixel data
  tiles_bg1_bitmap_.UpdateSurfacePixels();

  return absl::OkStatus();
}

absl::Status TitleScreen::RenderBG2Layer() {
  // BG2 layer is 32x32 tiles (256x256 pixels)
  auto& bg2_data = tiles_bg2_bitmap_.mutable_data();
  const auto& tile8_bitmap_data = tiles8_bitmap_.vector();

  // Render each tile in the 32x32 tilemap
  for (int tile_y = 0; tile_y < 32; tile_y++) {
    for (int tile_x = 0; tile_x < 32; tile_x++) {
      int tilemap_index = tile_y * 32 + tile_x;
      uint16_t tile_word = tiles_bg2_buffer_[tilemap_index];

      // Extract tile info from SNES tile word (vhopppcc cccccccc format)
      int tile_id = tile_word & 0x3FF;  // Bits 0-9: tile ID
      int palette = (tile_word >> 10) & 0x07;  // Bits 10-12: palette
      bool h_flip = (tile_word & 0x4000) != 0;  // Bit 14: horizontal flip
      bool v_flip = (tile_word & 0x8000) != 0;  // Bit 15: vertical flip

      // Calculate source position in tiles8_bitmap_ (16 tiles per row, 8x8 each)
      int src_tile_x = (tile_id % 16) * 8;
      int src_tile_y = (tile_id / 16) * 8;

      // Copy 8x8 tile pixels from tile8 bitmap to BG2 bitmap
      for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
          // Apply flipping
          int src_px = h_flip ? (7 - px) : px;
          int src_py = v_flip ? (7 - py) : py;

          // Calculate source and destination positions
          int src_x = src_tile_x + src_px;
          int src_y = src_tile_y + src_py;
          int src_pos = src_y * 128 + src_x;  // tiles8_bitmap_ is 128 pixels wide

          int dest_x = tile_x * 8 + px;
          int dest_y = tile_y * 8 + py;
          int dest_pos = dest_y * 256 + dest_x;  // BG2 is 256 pixels wide

          // Copy pixel with palette application
          // Title screen uses 3BPP graphics (8 colors per palette)
          if (src_pos < tile8_bitmap_data.size() && dest_pos < bg2_data.size()) {
            uint8_t pixel_value = tile8_bitmap_data[src_pos];
            // Apply palette index (title screen uses 8-color palettes)
            // Mask to 3 bits for 8 colors, then apply palette offset
            bg2_data[dest_pos] = (pixel_value & 0x07) | ((palette & 0x07) << 3);
          }
        }
      }
    }
  }
  
  // Update surface with rendered pixel data
  tiles_bg2_bitmap_.UpdateSurfacePixels();

  return absl::OkStatus();
}

absl::Status TitleScreen::Save(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM is not loaded");
  }

  // Title screen uses compressed tilemap format
  // We'll write the data back in the same compressed format
  std::vector<uint8_t> compressed_data;
  
  // Helper to write word (little endian)
  auto WriteWord = [&compressed_data](uint16_t value) {
    compressed_data.push_back(value & 0xFF);
    compressed_data.push_back((value >> 8) & 0xFF);
  };
  
  // Compress BG2 layer (dest < 0x1000)
  uint16_t bg2_dest = 0x0000;
  for (int i = 0; i < 1024; i++) {
    if (i == 0 || tiles_bg2_buffer_[i] != tiles_bg2_buffer_[i - 1]) {
      // Start a new run
      WriteWord(bg2_dest + i);  // Destination address
      
      // Count consecutive identical tiles
      int run_length = 1;
      uint16_t tile_value = tiles_bg2_buffer_[i];
      while (i + run_length < 1024 && tiles_bg2_buffer_[i + run_length] == tile_value) {
        run_length++;
      }
      
      // Write length/flags (bit 14 = fixsource if run > 1)
      uint16_t length_flags = (run_length - 1) * 2;  // Length in bytes
      if (run_length > 1) {
        length_flags |= 0x4000;  // fixsource flag
      }
      WriteWord(length_flags);
      
      // Write tile data
      WriteWord(tile_value);
      
      i += run_length - 1;  // Skip already processed tiles
    }
  }
  
  // Compress BG1 layer (dest >= 0x1000)
  uint16_t bg1_dest = 0x1000;
  for (int i = 0; i < 1024; i++) {
    if (i == 0 || tiles_bg1_buffer_[i] != tiles_bg1_buffer_[i - 1]) {
      // Start a new run
      WriteWord(bg1_dest + i);  // Destination address
      
      // Count consecutive identical tiles
      int run_length = 1;
      uint16_t tile_value = tiles_bg1_buffer_[i];
      while (i + run_length < 1024 && tiles_bg1_buffer_[i + run_length] == tile_value) {
        run_length++;
      }
      
      // Write length/flags (bit 14 = fixsource if run > 1)
      uint16_t length_flags = (run_length - 1) * 2;  // Length in bytes
      if (run_length > 1) {
        length_flags |= 0x4000;  // fixsource flag
      }
      WriteWord(length_flags);
      
      // Write tile data
      WriteWord(tile_value);
      
      i += run_length - 1;  // Skip already processed tiles
    }
  }
  
  // Write terminator byte
  compressed_data.push_back(0x80);
  
  // Calculate ROM address to write to
  ASSIGN_OR_RETURN(uint8_t byte0, rom->ReadByte(0x137A + 3));
  ASSIGN_OR_RETURN(uint8_t byte1, rom->ReadByte(0x1383 + 3));
  ASSIGN_OR_RETURN(uint8_t byte2, rom->ReadByte(0x138C + 3));
  
  int pos = (byte2 << 16) + (byte1 << 8) + byte0;
  int write_pos = SnesToPc(pos);
  
  // Write compressed data to ROM
  for (size_t i = 0; i < compressed_data.size(); i++) {
    RETURN_IF_ERROR(rom->WriteByte(write_pos + i, compressed_data[i]));
  }
  
  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
