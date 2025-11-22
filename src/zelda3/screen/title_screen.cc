#include "title_screen.h"

#include <cstdint>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "app/rom.h"
#include "app/snes.h"
#include "util/log.h"

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
  // Title screen uses 3BPP graphics (like all LTTP data) with composite
  // 64-color palette
  tiles8_bitmap_.metadata().source_bpp = 3;
  tiles8_bitmap_.metadata().palette_format = 0;  // Full 64-color palette
  tiles8_bitmap_.metadata().source_type = "graphics_sheet";
  tiles8_bitmap_.metadata().palette_colors = 64;

  tiles_bg1_bitmap_.metadata().source_bpp = 3;
  tiles_bg1_bitmap_.metadata().palette_format =
      0;  // Uses full palette with sub-palette indexing
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

  // Initialize composite bitmap for stacked BG rendering (256x256 = 65536
  // bytes)
  title_composite_bitmap_.Create(256, 256, 8, std::vector<uint8_t>(256 * 256));
  title_composite_bitmap_.metadata().source_bpp = 3;
  title_composite_bitmap_.metadata().palette_format = 0;
  title_composite_bitmap_.metadata().source_type = "screen_buffer";
  title_composite_bitmap_.metadata().palette_colors = 64;

  // Initialize tilemap buffers
  tiles_bg1_buffer_.fill(0x492);  // Default empty tile
  tiles_bg2_buffer_.fill(0x492);

  // Load palette (title screen uses 3BPP graphics with 8 palettes of 8 colors
  // each) Build composite palette from multiple sources (matches ZScream's
  // SetColorsPalette) Palette 0: OverworldMainPalettes[5] Palette 1:
  // OverworldAnimatedPalettes[0] Palette 2: OverworldAuxPalettes[3] Palette 3:
  // OverworldAuxPalettes[3] Palette 4: HudPalettes[0] Palette 5:
  // Transparent/black Palette 6: SpritesAux1Palettes[1] Palette 7:
  // SpritesAux1Palettes[1]

  auto pal_group = rom->palette_group();

  // Add each 8-color palette in sequence (EXACTLY 8 colors each for 64 total)
  size_t palette_start = palette_.size();

  // Palette 0: OverworldMainPalettes[5]
  if (pal_group.overworld_main.size() > 5) {
    const auto& src = pal_group.overworld_main[5];
    size_t added = 0;
    for (size_t i = 0; i < 8 && i < src.size(); i++) {
      palette_.AddColor(src[i]);
      added++;
    }
    // Pad with black if less than 8 colors
    while (added < 8) {
      palette_.AddColor(gfx::SnesColor(0, 0, 0));
      added++;
    }
    LOG_INFO("TitleScreen",
             "Palette 0: added %zu colors from overworld_main[5]", added);
  }

  // Palette 1: OverworldAnimatedPalettes[0]
  if (pal_group.overworld_animated.size() > 0) {
    const auto& src = pal_group.overworld_animated[0];
    size_t added = 0;
    for (size_t i = 0; i < 8 && i < src.size(); i++) {
      palette_.AddColor(src[i]);
      added++;
    }
    while (added < 8) {
      palette_.AddColor(gfx::SnesColor(0, 0, 0));
      added++;
    }
    LOG_INFO("TitleScreen",
             "Palette 1: added %zu colors from overworld_animated[0]", added);
  }

  // Palette 2 & 3: OverworldAuxPalettes[3] (used twice)
  if (pal_group.overworld_aux.size() > 3) {
    auto src = pal_group.overworld_aux[3];  // Copy, as this returns by value
    for (int pal = 0; pal < 2; pal++) {
      size_t added = 0;
      for (size_t i = 0; i < 8 && i < src.size(); i++) {
        palette_.AddColor(src[i]);
        added++;
      }
      while (added < 8) {
        palette_.AddColor(gfx::SnesColor(0, 0, 0));
        added++;
      }
      LOG_INFO("TitleScreen",
               "Palette %d: added %zu colors from overworld_aux[3]", 2 + pal,
               added);
    }
  }

  // Palette 4: HudPalettes[0]
  if (pal_group.hud.size() > 0) {
    auto src = pal_group.hud.palette(0);  // Copy, as this returns by value
    size_t added = 0;
    for (size_t i = 0; i < 8 && i < src.size(); i++) {
      palette_.AddColor(src[i]);
      added++;
    }
    while (added < 8) {
      palette_.AddColor(gfx::SnesColor(0, 0, 0));
      added++;
    }
    LOG_INFO("TitleScreen", "Palette 4: added %zu colors from hud[0]", added);
  }

  // Palette 5: 8 transparent/black colors
  for (int i = 0; i < 8; i++) {
    palette_.AddColor(gfx::SnesColor(0, 0, 0));
  }
  LOG_INFO("TitleScreen", "Palette 5: added 8 transparent/black colors");

  // Palette 6 & 7: SpritesAux1Palettes[1] (used twice)
  if (pal_group.sprites_aux1.size() > 1) {
    auto src = pal_group.sprites_aux1[1];  // Copy, as this returns by value
    for (int pal = 0; pal < 2; pal++) {
      size_t added = 0;
      for (size_t i = 0; i < 8 && i < src.size(); i++) {
        palette_.AddColor(src[i]);
        added++;
      }
      while (added < 8) {
        palette_.AddColor(gfx::SnesColor(0, 0, 0));
        added++;
      }
      LOG_INFO("TitleScreen",
               "Palette %d: added %zu colors from sprites_aux1[1]", 6 + pal,
               added);
    }
  }

  LOG_INFO("TitleScreen", "Built composite palette: %zu colors (should be 64)",
           palette_.size());

  // Build tile16 blockset from graphics
  RETURN_IF_ERROR(BuildTileset(rom));

  // Load tilemap data from ROM
  RETURN_IF_ERROR(LoadTitleScreen(rom));

  return absl::OkStatus();
}

absl::Status TitleScreen::BuildTileset(Rom* rom) {
  // Title screen uses specific graphics sheets
  // Load sheet configuration from ROM (matches ZScream implementation)
  uint8_t staticgfx[16] = {0};

  // Read title screen GFX group indices from ROM
  constexpr int kTitleScreenTilesGFX = 0x064207;
  constexpr int kTitleScreenSpritesGFX = 0x06420C;

  ASSIGN_OR_RETURN(uint8_t tiles_gfx_index,
                   rom->ReadByte(kTitleScreenTilesGFX));
  ASSIGN_OR_RETURN(uint8_t sprites_gfx_index,
                   rom->ReadByte(kTitleScreenSpritesGFX));

  LOG_INFO("TitleScreen", "GFX group indices: tiles=%d, sprites=%d",
           tiles_gfx_index, sprites_gfx_index);

  // Load main graphics sheets (slots 0-7) from GFX groups
  // First, read the GFX groups pointer (2 bytes at 0x6237)
  constexpr int kGfxGroupsPointer = 0x6237;
  ASSIGN_OR_RETURN(uint16_t gfx_groups_snes, rom->ReadWord(kGfxGroupsPointer));
  uint32_t main_gfx_table = SnesToPc(gfx_groups_snes);

  LOG_INFO("TitleScreen", "GFX groups table: SNES=0x%04X, PC=0x%06X",
           gfx_groups_snes, main_gfx_table);

  // Read 8 bytes from mainGfx[tiles_gfx_index]
  int main_gfx_offset = main_gfx_table + (tiles_gfx_index * 8);
  for (int i = 0; i < 8; i++) {
    ASSIGN_OR_RETURN(staticgfx[i], rom->ReadByte(main_gfx_offset + i));
  }

  // Load sprite graphics sheets (slots 8-12) - matches ZScream logic
  // Sprite GFX groups are after the 37 main groups (37 * 8 = 296 bytes)
  // and 82 room groups (82 * 4 = 328 bytes) = 624 bytes offset
  int sprite_gfx_table = main_gfx_table + (37 * 8) + (82 * 4);
  int sprite_gfx_offset = sprite_gfx_table + (sprites_gfx_index * 4);

  staticgfx[8] = 115 + 0;  // Title logo base
  ASSIGN_OR_RETURN(uint8_t sprite3, rom->ReadByte(sprite_gfx_offset + 3));
  staticgfx[9] = 115 + sprite3;  // Sprite graphics slot 3
  staticgfx[10] = 115 + 6;       // Additional graphics
  staticgfx[11] = 115 + 7;       // Additional graphics
  ASSIGN_OR_RETURN(uint8_t sprite0, rom->ReadByte(sprite_gfx_offset + 0));
  staticgfx[12] = 115 + sprite0;  // Sprite graphics slot 0
  staticgfx[13] = 112;            // UI graphics
  staticgfx[14] = 112;            // UI graphics
  staticgfx[15] = 112;            // UI graphics

  // Use pre-converted graphics from ROM buffer - simple and matches rest of
  // yaze Title screen uses standard 3BPP graphics, no special offset needed
  const auto& gfx_buffer = rom->graphics_buffer();
  auto& tiles8_data = tiles8_bitmap_.mutable_data();

  LOG_INFO("TitleScreen", "Graphics buffer size: %zu bytes", gfx_buffer.size());
  LOG_INFO("TitleScreen", "Tiles8 bitmap size: %zu bytes", tiles8_data.size());

  // Copy graphics sheets to tiles8_bitmap
  LOG_INFO("TitleScreen", "Loading 16 graphics sheets:");
  for (int i = 0; i < 16; i++) {
    LOG_INFO("TitleScreen", "  staticgfx[%d] = %d", i, staticgfx[i]);
  }

  for (int i = 0; i < 16; i++) {
    int sheet_id = staticgfx[i];

    // Validate sheet ID (ROM has 223 sheets: 0-222)
    if (sheet_id > 222) {
      LOG_ERROR(
          "TitleScreen",
          "Sheet %d: Invalid sheet_id=%d (max 222), using sheet 0 instead", i,
          sheet_id);
      sheet_id = 0;  // Fallback to a valid sheet
    }

    int source_offset = sheet_id * 0x1000;  // Each 8BPP sheet is 0x1000 bytes
    int dest_offset = i * 0x1000;

    if (source_offset + 0x1000 <= gfx_buffer.size() &&
        dest_offset + 0x1000 <= tiles8_data.size()) {
      std::copy(gfx_buffer.begin() + source_offset,
                gfx_buffer.begin() + source_offset + 0x1000,
                tiles8_data.begin() + dest_offset);

      // Sample first few pixels
      LOG_INFO("TitleScreen",
               "Sheet %d (ID %d): Sample pixels: %02X %02X %02X %02X", i,
               sheet_id, tiles8_data[dest_offset], tiles8_data[dest_offset + 1],
               tiles8_data[dest_offset + 2], tiles8_data[dest_offset + 3]);
    } else {
      LOG_ERROR("TitleScreen",
                "Sheet %d (ID %d): out of bounds! source=%d, dest=%d, "
                "buffer_size=%zu",
                i, sheet_id, source_offset, dest_offset, gfx_buffer.size());
    }
  }

  // Set palette on tiles8 bitmap
  tiles8_bitmap_.SetPalette(palette_);

  LOG_INFO("TitleScreen", "Applied palette to tiles8_bitmap: %zu colors",
           palette_.size());
  // Log first few colors
  if (palette_.size() >= 8) {
    LOG_INFO("TitleScreen",
             "  Palette colors 0-7: %04X %04X %04X %04X %04X %04X %04X %04X",
             palette_[0].snes(), palette_[1].snes(), palette_[2].snes(),
             palette_[3].snes(), palette_[4].snes(), palette_[5].snes(),
             palette_[6].snes(), palette_[7].snes());
  }

  // Queue texture creation via Arena's deferred system
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &tiles8_bitmap_);

  // TODO: Build tile16 blockset from tile8 data
  // This would involve composing 16x16 tiles from 8x8 tiles
  // For now, we'll use the tile8 data directly

  return absl::OkStatus();
}

absl::Status TitleScreen::LoadTitleScreen(Rom* rom) {
  // Check if ROM uses ZScream's expanded format (data at 0x108000 PC)
  // by reading the title screen pointer at 0x137A+3, 0x1383+3, 0x138C+3
  ASSIGN_OR_RETURN(uint8_t bank_byte, rom->ReadByte(0x138C + 3));
  ASSIGN_OR_RETURN(uint8_t high_byte, rom->ReadByte(0x1383 + 3));
  ASSIGN_OR_RETURN(uint8_t low_byte, rom->ReadByte(0x137A + 3));

  uint32_t snes_addr = (bank_byte << 16) | (high_byte << 8) | low_byte;
  uint32_t pc_addr = SnesToPc(snes_addr);

  LOG_INFO("TitleScreen", "Title screen pointer: SNES=0x%06X, PC=0x%06X",
           snes_addr, pc_addr);

  // Initialize buffers with default empty tile
  for (int i = 0; i < 1024; i++) {
    tiles_bg1_buffer_[i] = 0x492;
    tiles_bg2_buffer_[i] = 0x492;
  }

  // ZScream expanded format at 0x108000 (PC)
  if (pc_addr >= 0x108000 && pc_addr <= 0x10FFFF) {
    LOG_INFO("TitleScreen", "Detected ZScream expanded format");

    int pos = pc_addr;

    // Read BG1 header: dest (word), length (word)
    ASSIGN_OR_RETURN(uint16_t bg1_dest, rom->ReadWord(pos));
    pos += 2;
    ASSIGN_OR_RETURN(uint16_t bg1_length, rom->ReadWord(pos));
    pos += 2;

    LOG_INFO("TitleScreen", "BG1 Header: dest=0x%04X, length=0x%04X", bg1_dest,
             bg1_length);

    // Read 1024 BG1 tiles (2 bytes each = 2048 bytes)
    for (int i = 0; i < 1024; i++) {
      ASSIGN_OR_RETURN(uint16_t tile, rom->ReadWord(pos));
      tiles_bg1_buffer_[i] = tile;
      pos += 2;
    }

    // Read BG2 header: dest (word), length (word)
    ASSIGN_OR_RETURN(uint16_t bg2_dest, rom->ReadWord(pos));
    pos += 2;
    ASSIGN_OR_RETURN(uint16_t bg2_length, rom->ReadWord(pos));
    pos += 2;

    LOG_INFO("TitleScreen", "BG2 Header: dest=0x%04X, length=0x%04X", bg2_dest,
             bg2_length);

    // Read 1024 BG2 tiles (2 bytes each = 2048 bytes)
    for (int i = 0; i < 1024; i++) {
      ASSIGN_OR_RETURN(uint16_t tile, rom->ReadWord(pos));
      tiles_bg2_buffer_[i] = tile;
      pos += 2;
    }

    LOG_INFO("TitleScreen",
             "Loaded 2048 tilemap entries from ZScream expanded format");
  }
  // Vanilla format: Sequential DMA blocks at pointer location
  // NOTE: This reads from the pointer but may not be the correct format
  // See docs/screen-editor-status.md for details on this ongoing issue
  else {
    LOG_INFO("TitleScreen", "Using vanilla DMA format (EXPERIMENTAL)");

    int pos = pc_addr;
    int total_entries = 0;
    int blocks_read = 0;

    // Read DMA blocks until we hit terminator or safety limit
    while (pos < rom->size() && blocks_read < 20) {
      // Read destination address (word)
      ASSIGN_OR_RETURN(uint16_t dest_addr, rom->ReadWord(pos));
      pos += 2;

      // Check for terminator
      if (dest_addr == 0xFFFF || (dest_addr & 0xFF) == 0xFF) {
        LOG_INFO("TitleScreen", "Found DMA terminator at pos=0x%06X", pos - 2);
        break;
      }

      // Read length/flags (word)
      ASSIGN_OR_RETURN(uint16_t length_flags, rom->ReadWord(pos));
      pos += 2;

      bool increment64 = (length_flags & 0x8000) == 0x8000;
      bool fixsource = (length_flags & 0x4000) == 0x4000;
      int length = (length_flags & 0x0FFF);

      LOG_INFO("TitleScreen", "Block %d: dest=0x%04X, len=%d, inc64=%d, fix=%d",
               blocks_read, dest_addr, length, increment64, fixsource);

      int tile_count = (length / 2) + 1;
      int source_start = pos;

      // Read tiles
      for (int j = 0; j < tile_count; j++) {
        ASSIGN_OR_RETURN(uint16_t tiledata, rom->ReadWord(pos));

        // Determine which layer based on destination address
        if (dest_addr >= 0x1000 && dest_addr < 0x1400) {
          // BG1 layer
          int index = (dest_addr - 0x1000) / 2;
          if (index < 1024) {
            tiles_bg1_buffer_[index] = tiledata;
            total_entries++;
          }
        } else if (dest_addr < 0x0800) {
          // BG2 layer
          int index = dest_addr / 2;
          if (index < 1024) {
            tiles_bg2_buffer_[index] = tiledata;
            total_entries++;
          }
        }

        // Advance destination address
        if (increment64) {
          dest_addr += 64;
        } else {
          dest_addr += 2;
        }

        // Advance source position
        if (!fixsource) {
          pos += 2;
        }
      }

      // If fixsource, only advance by one tile
      if (fixsource) {
        pos = source_start + 2;
      }

      blocks_read++;
    }

    LOG_INFO("TitleScreen",
             "Loaded %d tilemap entries from %d DMA blocks (may be incorrect)",
             total_entries, blocks_read);
  }

  pal_selected_ = 2;

  // Render tilemaps into bitmap pixels
  RETURN_IF_ERROR(RenderBG1Layer());
  RETURN_IF_ERROR(RenderBG2Layer());

  // Apply palettes to layer bitmaps AFTER rendering
  tiles_bg1_bitmap_.SetPalette(palette_);
  tiles_bg2_bitmap_.SetPalette(palette_);
  oam_bg_bitmap_.SetPalette(palette_);
  title_composite_bitmap_.SetPalette(palette_);

  // Ensure bitmaps are marked as active
  tiles_bg1_bitmap_.set_active(true);
  tiles_bg2_bitmap_.set_active(true);
  oam_bg_bitmap_.set_active(true);
  title_composite_bitmap_.set_active(true);

  // Queue texture creation for all layer bitmaps
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &tiles_bg1_bitmap_);
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &tiles_bg2_bitmap_);
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &oam_bg_bitmap_);
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &title_composite_bitmap_);

  // Initial composite render (both layers visible)
  RETURN_IF_ERROR(RenderCompositeLayer(true, true));

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
      int tile_id = tile_word & 0x3FF;          // Bits 0-9: tile ID
      int palette = (tile_word >> 10) & 0x07;   // Bits 10-12: palette
      bool h_flip = (tile_word & 0x4000) != 0;  // Bit 14: horizontal flip
      bool v_flip = (tile_word & 0x8000) != 0;  // Bit 15: vertical flip

      // Debug: Log suspicious tile IDs
      if (tile_id > 512) {
        LOG_WARN("TitleScreen",
                 "BG1: Suspicious tile_id=%d at (%d,%d), word=0x%04X", tile_id,
                 tile_x, tile_y, tile_word);
      }

      // Calculate source position in tiles8_bitmap_
      // tiles8_bitmap_ is 128 pixels wide, 512 pixels tall (16 sheets × 32
      // pixels) Each sheet has 256 tiles (16×16 tiles, 128×32 pixels, 0x1000
      // bytes)
      int sheet_index = tile_id / 256;    // Which sheet (0-15)
      int tile_in_sheet = tile_id % 256;  // Tile within sheet (0-255)
      int src_tile_x = (tile_in_sheet % 16) * 8;
      int src_tile_y = (sheet_index * 32) + ((tile_in_sheet / 16) * 8);

      // Copy 8x8 tile pixels from tile8 bitmap to BG1 bitmap
      for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
          // Apply flipping
          int src_px = h_flip ? (7 - px) : px;
          int src_py = v_flip ? (7 - py) : py;

          // Calculate source and destination positions
          int src_x = src_tile_x + src_px;
          int src_y = src_tile_y + src_py;
          int src_pos =
              src_y * 128 + src_x;  // tiles8_bitmap_ is 128 pixels wide

          int dest_x = tile_x * 8 + px;
          int dest_y = tile_y * 8 + py;
          int dest_pos = dest_y * 256 + dest_x;  // BG1 is 256 pixels wide

          // Copy pixel with palette application
          // Graphics are 3BPP in ROM, converted to 8BPP indexed with +0x88
          // offset
          if (src_pos < tile8_bitmap_data.size() &&
              dest_pos < bg1_data.size()) {
            uint8_t pixel_value = tile8_bitmap_data[src_pos];
            // Pixel values already include palette information from +0x88
            // offset Just copy directly (color index 0 = transparent)
            bg1_data[dest_pos] = pixel_value;
          }
        }
      }
    }
  }

  // Update surface with rendered pixel data
  tiles_bg1_bitmap_.UpdateSurfacePixels();

  // Queue texture update
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &tiles_bg1_bitmap_);

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
      int tile_id = tile_word & 0x3FF;          // Bits 0-9: tile ID
      int palette = (tile_word >> 10) & 0x07;   // Bits 10-12: palette
      bool h_flip = (tile_word & 0x4000) != 0;  // Bit 14: horizontal flip
      bool v_flip = (tile_word & 0x8000) != 0;  // Bit 15: vertical flip

      // Calculate source position in tiles8_bitmap_
      // tiles8_bitmap_ is 128 pixels wide, 512 pixels tall (16 sheets × 32
      // pixels) Each sheet has 256 tiles (16×16 tiles, 128×32 pixels, 0x1000
      // bytes)
      int sheet_index = tile_id / 256;    // Which sheet (0-15)
      int tile_in_sheet = tile_id % 256;  // Tile within sheet (0-255)
      int src_tile_x = (tile_in_sheet % 16) * 8;
      int src_tile_y = (sheet_index * 32) + ((tile_in_sheet / 16) * 8);

      // Copy 8x8 tile pixels from tile8 bitmap to BG2 bitmap
      for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
          // Apply flipping
          int src_px = h_flip ? (7 - px) : px;
          int src_py = v_flip ? (7 - py) : py;

          // Calculate source and destination positions
          int src_x = src_tile_x + src_px;
          int src_y = src_tile_y + src_py;
          int src_pos =
              src_y * 128 + src_x;  // tiles8_bitmap_ is 128 pixels wide

          int dest_x = tile_x * 8 + px;
          int dest_y = tile_y * 8 + py;
          int dest_pos = dest_y * 256 + dest_x;  // BG2 is 256 pixels wide

          // Copy pixel with palette application
          // Graphics are 3BPP in ROM, converted to 8BPP indexed with +0x88
          // offset
          if (src_pos < tile8_bitmap_data.size() &&
              dest_pos < bg2_data.size()) {
            uint8_t pixel_value = tile8_bitmap_data[src_pos];
            // Pixel values already include palette information from +0x88
            // offset Just copy directly (color index 0 = transparent)
            bg2_data[dest_pos] = pixel_value;
          }
        }
      }
    }
  }

  // Update surface with rendered pixel data
  tiles_bg2_bitmap_.UpdateSurfacePixels();

  // Queue texture update
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &tiles_bg2_bitmap_);

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
      while (i + run_length < 1024 &&
             tiles_bg2_buffer_[i + run_length] == tile_value) {
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
      while (i + run_length < 1024 &&
             tiles_bg1_buffer_[i + run_length] == tile_value) {
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

absl::Status TitleScreen::RenderCompositeLayer(bool show_bg1, bool show_bg2) {
  auto& composite_data = title_composite_bitmap_.mutable_data();
  const auto& bg1_data = tiles_bg1_bitmap_.vector();
  const auto& bg2_data = tiles_bg2_bitmap_.vector();

  // Clear to transparent (color index 0)
  std::fill(composite_data.begin(), composite_data.end(), 0);

  // Layer BG2 first (if visible) - background layer
  if (show_bg2) {
    for (int i = 0; i < 256 * 256; i++) {
      composite_data[i] = bg2_data[i];
    }
  }

  // Layer BG1 on top (if visible), respecting transparency
  if (show_bg1) {
    for (int i = 0; i < 256 * 256; i++) {
      uint8_t pixel = bg1_data[i];
      // Check if color 0 in the sub-palette (transparent)
      // Pixel format is (palette<<3) | color, so color is bits 0-2
      if ((pixel & 0x07) != 0) {
        composite_data[i] = pixel;
      }
    }
  }

  // Copy pixel data to SDL surface
  title_composite_bitmap_.UpdateSurfacePixels();

  // Queue texture update
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &title_composite_bitmap_);

  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
