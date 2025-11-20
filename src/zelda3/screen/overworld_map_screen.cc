#include "zelda3/screen/overworld_map_screen.h"

#include <fstream>

#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_color.h"
#include "app/rom.h"
#include "app/snes.h"

namespace yaze {
namespace zelda3 {

absl::Status OverworldMapScreen::Create(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM is not loaded");
  }

  // Set metadata for overworld map bitmaps
  // Mode 7 graphics use full 128-color palettes

  // Load Mode 7 graphics (256 tiles, 8x8 pixels each, 8BPP)
  const int mode7_gfx_addr = 0x0C4000;
  std::vector<uint8_t> mode7_gfx_raw(0x4000);  // Raw tileset data from ROM

  for (int i = 0; i < 0x4000; i++) {
    ASSIGN_OR_RETURN(mode7_gfx_raw[i], rom->ReadByte(mode7_gfx_addr + i));
  }

  // Mode 7 tiles are stored in tiled format (each tile's rows are consecutive)
  // but we need linear bitmap format (all tiles' first rows, then all second rows)
  // Convert from tiled to linear bitmap layout
  std::vector<uint8_t> mode7_gfx(0x4000);
  int pos = 0;
  for (int sy = 0; sy < 16 * 1024; sy += 1024) {  // 16 rows of tiles
    for (int sx = 0; sx < 16 * 8; sx += 8) {      // 16 columns of tiles
      for (int y = 0; y < 8 * 128; y += 128) {    // 8 pixel rows within tile
        for (int x = 0; x < 8; x++) {             // 8 pixels per row
          mode7_gfx[x + sx + y + sy] = mode7_gfx_raw[pos];
          pos++;
        }
      }
    }
  }

  // Create tiles8 bitmap: 128×128 pixels (16×16 tiles = 256 tiles)
  tiles8_bitmap_.Create(128, 128, 8, mode7_gfx);
  tiles8_bitmap_.metadata().source_bpp = 8;
  tiles8_bitmap_.metadata().palette_format = 0;
  tiles8_bitmap_.metadata().source_type = "mode7_tileset";
  tiles8_bitmap_.metadata().palette_colors = 128;

  // Create map bitmap (512x512 for 64x64 tiles at 8x8 each)
  map_bitmap_.Create(512, 512, 8, std::vector<uint8_t>(512 * 512));
  map_bitmap_.metadata().source_bpp = 8;
  map_bitmap_.metadata().palette_format = 0;
  map_bitmap_.metadata().source_type = "mode7_map";
  map_bitmap_.metadata().palette_colors = 128;

  // Light World palette at 0x055B27
  const int lw_pal_addr = 0x055B27;
  for (int i = 0; i < 128; i++) {
    ASSIGN_OR_RETURN(uint16_t snes_color, rom->ReadWord(lw_pal_addr + (i * 2)));
    // Create SnesColor directly from SNES 15-bit format
    lw_palette_.AddColor(gfx::SnesColor(snes_color));
  }

  // Dark World palette at 0x055C27
  const int dw_pal_addr = 0x055C27;
  for (int i = 0; i < 128; i++) {
    ASSIGN_OR_RETURN(uint16_t snes_color, rom->ReadWord(dw_pal_addr + (i * 2)));
    // Create SnesColor directly from SNES 15-bit format
    dw_palette_.AddColor(gfx::SnesColor(snes_color));
  }

  // Load map tile data
  RETURN_IF_ERROR(LoadMapData(rom));

  // Render initial map (Light World)
  RETURN_IF_ERROR(RenderMapLayer(false));

  // Apply palettes AFTER bitmaps are fully initialized
  tiles8_bitmap_.SetPalette(lw_palette_);
  map_bitmap_.SetPalette(lw_palette_);  // Map also needs palette

  // Ensure bitmaps are marked as active
  tiles8_bitmap_.set_active(true);
  map_bitmap_.set_active(true);

  // Queue texture creation
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &tiles8_bitmap_);
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &map_bitmap_);

  return absl::OkStatus();
}

absl::Status OverworldMapScreen::LoadMapData(Rom* rom) {
  // Map data is stored in interleaved format across 4 sections + 1 DW section
  // Based on ZScream's Constants.IDKZarby = 0x054727
  // The data alternates between left (32 columns) and right (32 columns)
  // for the first 2048 tiles, then continues for bottom half

  const int base_addr = 0x054727;  // IDKZarby constant from ZScream
  int p1 = base_addr + 0x0000;     // Top-left quadrant data
  int p2 = base_addr + 0x0400;     // Top-right quadrant data
  int p3 = base_addr + 0x0800;     // Bottom-left quadrant data
  int p4 = base_addr + 0x0C00;     // Bottom-right quadrant data
  int p5 = base_addr + 0x1000;     // Dark World additional section

  bool rSide = false;  // false = left side, true = right side
  int cSide = 0;       // Column counter within side (0-31)
  int count = 0;       // Output tile index

  // Load 64x64 map with interleaved left/right format
  while (count < 64 * 64) {
    if (count < 0x800) {  // Top half (first 2048 tiles)
      if (!rSide) {
        // Read from left side (p1)
        ASSIGN_OR_RETURN(uint8_t tile, rom->ReadByte(p1));
        lw_map_tiles_[count] = tile;
        dw_map_tiles_[count] = tile;
        p1++;

        if (cSide >= 31) {
          cSide = 0;
          rSide = true;
          count++;
          continue;
        }
      } else {
        // Read from right side (p2)
        ASSIGN_OR_RETURN(uint8_t tile, rom->ReadByte(p2));
        lw_map_tiles_[count] = tile;
        dw_map_tiles_[count] = tile;
        p2++;

        if (cSide >= 31) {
          cSide = 0;
          rSide = false;
          count++;
          continue;
        }
      }
    } else {  // Bottom half (remaining 2048 tiles)
      if (!rSide) {
        // Read from left side (p3)
        ASSIGN_OR_RETURN(uint8_t tile, rom->ReadByte(p3));
        lw_map_tiles_[count] = tile;
        dw_map_tiles_[count] = tile;
        p3++;

        if (cSide >= 31) {
          cSide = 0;
          rSide = true;
          count++;
          continue;
        }
      } else {
        // Read from right side (p4)
        ASSIGN_OR_RETURN(uint8_t tile, rom->ReadByte(p4));
        lw_map_tiles_[count] = tile;
        dw_map_tiles_[count] = tile;
        p4++;

        if (cSide >= 31) {
          cSide = 0;
          rSide = false;
          count++;
          continue;
        }
      }
    }

    cSide++;
    count++;
  }

  // Load Dark World specific data (bottom-right 32x32 section)
  count = 0;
  int line = 0;
  while (true) {
    ASSIGN_OR_RETURN(uint8_t tile, rom->ReadByte(p5));
    dw_map_tiles_[1040 + count + (line * 64)] = tile;
    p5++;
    count++;
    if (count >= 32) {
      count = 0;
      line++;
      if (line >= 32) {
        break;
      }
    }
  }

  return absl::OkStatus();
}

absl::Status OverworldMapScreen::RenderMapLayer(bool use_dark_world) {
  auto& map_data = map_bitmap_.mutable_data();
  const auto& tiles8_data = tiles8_bitmap_.vector();
  const auto& tile_source = use_dark_world ? dw_map_tiles_ : lw_map_tiles_;

  // Render 64x64 tiles (each 8x8 pixels) into 512x512 bitmap
  for (int yy = 0; yy < 64; yy++) {
    for (int xx = 0; xx < 64; xx++) {
      uint8_t tile_id = tile_source[xx + (yy * 64)];

      // Calculate tile position in tiles8_bitmap (16 tiles per row)
      int tile_x = (tile_id % 16) * 8;
      int tile_y = (tile_id / 16) * 8;

      // Copy 8x8 tile pixels
      for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
          int src_index = (tile_x + px) + ((tile_y + py) * 128);
          int dest_index = (xx * 8 + px) + ((yy * 8 + py) * 512);

          if (src_index < tiles8_data.size() && dest_index < map_data.size()) {
            map_data[dest_index] = tiles8_data[src_index];
          }
        }
      }
    }
  }

  // Copy pixel data to SDL surface
  map_bitmap_.UpdateSurfacePixels();

  return absl::OkStatus();
}

absl::Status OverworldMapScreen::Save(Rom* rom) {
  // Write data back in the same interleaved format
  const int base_addr = 0x054727;
  int p1 = base_addr + 0x0000;
  int p2 = base_addr + 0x0400;
  int p3 = base_addr + 0x0800;
  int p4 = base_addr + 0x0C00;
  int p5 = base_addr + 0x1000;

  bool rSide = false;
  int cSide = 0;
  int count = 0;

  // Write 64x64 map with interleaved left/right format
  while (count < 64 * 64) {
    if (count < 0x800) {
      if (!rSide) {
        RETURN_IF_ERROR(rom->WriteByte(p1, lw_map_tiles_[count]));
        p1++;

        if (cSide >= 31) {
          cSide = 0;
          rSide = true;
          count++;
          continue;
        }
      } else {
        RETURN_IF_ERROR(rom->WriteByte(p2, lw_map_tiles_[count]));
        p2++;

        if (cSide >= 31) {
          cSide = 0;
          rSide = false;
          count++;
          continue;
        }
      }
    } else {
      if (!rSide) {
        RETURN_IF_ERROR(rom->WriteByte(p3, lw_map_tiles_[count]));
        p3++;

        if (cSide >= 31) {
          cSide = 0;
          rSide = true;
          count++;
          continue;
        }
      } else {
        RETURN_IF_ERROR(rom->WriteByte(p4, lw_map_tiles_[count]));
        p4++;

        if (cSide >= 31) {
          cSide = 0;
          rSide = false;
          count++;
          continue;
        }
      }
    }

    cSide++;
    count++;
  }

  // Write Dark World specific data
  count = 0;
  int line = 0;
  while (true) {
    RETURN_IF_ERROR(
        rom->WriteByte(p5, dw_map_tiles_[1040 + count + (line * 64)]));
    p5++;
    count++;
    if (count >= 32) {
      count = 0;
      line++;
      if (line >= 32) {
        break;
      }
    }
  }

  return absl::OkStatus();
}

absl::Status OverworldMapScreen::LoadCustomMap(const std::string& file_path) {
  // Load custom map from external binary file
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return absl::NotFoundError("Could not open custom map file: " + file_path);
  }

  std::streamsize size = file.tellg();
  if (size != 4096) {
    return absl::InvalidArgumentError(
        "Custom map file must be exactly 4096 bytes (64×64 tiles)");
  }

  file.seekg(0, std::ios::beg);

  // Read into Light World map buffer (could add option for Dark World later)
  file.read(reinterpret_cast<char*>(lw_map_tiles_.data()), 4096);

  if (!file) {
    return absl::InternalError("Failed to read custom map data");
  }

  // Re-render with new data
  RETURN_IF_ERROR(RenderMapLayer(false));
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &map_bitmap_);

  return absl::OkStatus();
}

absl::Status OverworldMapScreen::SaveCustomMap(const std::string& file_path,
                                               bool use_dark_world) {
  std::ofstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    return absl::InternalError("Could not create custom map file: " +
                               file_path);
  }

  const auto& tiles = use_dark_world ? dw_map_tiles_ : lw_map_tiles_;
  file.write(reinterpret_cast<const char*>(tiles.data()), tiles.size());

  if (!file) {
    return absl::InternalError("Failed to write custom map data");
  }

  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
