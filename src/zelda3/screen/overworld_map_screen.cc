#include "overworld_map_screen.h"

#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_color.h"
#include "util/macro.h"

namespace yaze {
namespace zelda3 {

absl::Status OverworldMapScreen::Create(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM is not loaded");
  }

  // Initialize bitmaps
  tiles8_bitmap_.Create(128, 128, 8, std::vector<uint8_t>(128 * 128));
  map_bitmap_.Create(512, 512, 8, std::vector<uint8_t>(512 * 512));
  
  // Set metadata for overworld map bitmaps
  // Mode 7 graphics use full 128-color palettes
  tiles8_bitmap_.metadata().source_bpp = 8;
  tiles8_bitmap_.metadata().palette_format = 0;  // Full palette
  tiles8_bitmap_.metadata().source_type = "mode7";
  tiles8_bitmap_.metadata().palette_colors = 128;
  
  map_bitmap_.metadata().source_bpp = 8;
  map_bitmap_.metadata().palette_format = 0;  // Full palette
  map_bitmap_.metadata().source_type = "mode7";
  map_bitmap_.metadata().palette_colors = 128;

  // Load mode 7 graphics from 0x0C4000
  const int mode7_gfx_addr = 0x0C4000;
  auto& tiles8_data = tiles8_bitmap_.mutable_data();
  
  // Mode 7 graphics are stored as 8x8 tiles, 16 tiles per row
  int pos = 0;
  for (int sy = 0; sy < 16; sy++) {
    for (int sx = 0; sx < 16; sx++) {
      for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
          int dest_index = x + (sx * 8) + (y * 128) + (sy * 1024);
          if (dest_index < tiles8_data.size() && mode7_gfx_addr + pos < rom->size()) {
            ASSIGN_OR_RETURN(uint8_t pixel, rom->ReadByte(mode7_gfx_addr + pos));
            tiles8_data[dest_index] = pixel;
          }
          pos++;
        }
      }
    }
  }

  // Load palettes (128 colors each for mode 7 graphics)
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
  gfx::Arena::Get().QueueTextureCommand(
      gfx::Arena::TextureCommandType::CREATE, &tiles8_bitmap_);
  gfx::Arena::Get().QueueTextureCommand(
      gfx::Arena::TextureCommandType::CREATE, &map_bitmap_);

  return absl::OkStatus();
}

absl::Status OverworldMapScreen::LoadMapData(Rom* rom) {
  // Map data is stored in 4 sections with interleaved left/right format
  // Based on ZScream's implementation in ScreenEditor.cs lines 221-322
  
  const int p1_addr = 0x0564F8;  // First section (left)
  const int p2_addr = 0x05634C;  // First section (right)
  const int p3_addr = 0x056BF8;  // Second section (left)
  const int p4_addr = 0x056A4C;  // Second section (right)
  const int p5_addr = 0x057404;  // Dark World additional section

  int count = 0;
  int cSide = 0;
  bool rSide = false;

  // Load Light World and Dark World base data
  while (count < 0x1000) {
    int p1 = p1_addr + (count - (rSide ? 1 : 0));
    int p2 = p2_addr + (count - (rSide ? 1 : 0));
    int p3 = p3_addr + (count - (rSide ? 1 : 0) - 0x800);
    int p4 = p4_addr + (count - (rSide ? 1 : 0) - 0x800);

    if (count < 0x800) {
      if (!rSide) {
        ASSIGN_OR_RETURN(uint8_t tile, rom->ReadByte(p1));
        lw_map_tiles_[count] = tile;
        dw_map_tiles_[count] = tile;
        
        if (cSide >= 31) {
          cSide = 0;
          rSide = true;
          count++;
          continue;
        }
      } else {
        ASSIGN_OR_RETURN(uint8_t tile, rom->ReadByte(p2));
        lw_map_tiles_[count] = tile;
        dw_map_tiles_[count] = tile;
        
        if (cSide >= 31) {
          cSide = 0;
          rSide = false;
          count++;
          continue;
        }
      }
    } else {
      if (!rSide) {
        ASSIGN_OR_RETURN(uint8_t tile, rom->ReadByte(p3));
        lw_map_tiles_[count] = tile;
        dw_map_tiles_[count] = tile;
        
        if (cSide >= 31) {
          cSide = 0;
          rSide = true;
          count++;
          continue;
        }
      } else {
        ASSIGN_OR_RETURN(uint8_t tile, rom->ReadByte(p4));
        lw_map_tiles_[count] = tile;
        dw_map_tiles_[count] = tile;
        
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

  // Load Dark World specific section (bottom-right 32x32 area)
  count = 0;
  int line = 0;
  while (true) {
    int addr = p5_addr + count + (line * 32);
    if (addr < rom->size()) {
      ASSIGN_OR_RETURN(uint8_t tile, rom->ReadByte(addr));
      int dest_index = 1040 + count + (line * 64);
      if (dest_index < dw_map_tiles_.size()) {
        dw_map_tiles_[dest_index] = tile;
      }
    }
    
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

  // Apply appropriate palette
  map_bitmap_.SetPalette(use_dark_world ? dw_palette_ : lw_palette_);

  return absl::OkStatus();
}

absl::Status OverworldMapScreen::Save(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM is not loaded");
  }

  // Save data back in the same interleaved format
  const int p1_addr = 0x0564F8;
  const int p2_addr = 0x05634C;
  const int p3_addr = 0x056BF8;
  const int p4_addr = 0x056A4C;
  const int p5_addr = 0x057404;

  int count = 0;
  int cSide = 0;
  bool rSide = false;

  // Save Light World data (same pattern as loading)
  while (count < 0x1000) {
    int p1 = p1_addr + (count - (rSide ? 1 : 0));
    int p2 = p2_addr + (count - (rSide ? 1 : 0));
    int p3 = p3_addr + (count - (rSide ? 1 : 0) - 0x800);
    int p4 = p4_addr + (count - (rSide ? 1 : 0) - 0x800);

    if (count < 0x800) {
      if (!rSide) {
        RETURN_IF_ERROR(rom->WriteByte(p1, lw_map_tiles_[count]));
        
        if (cSide >= 31) {
          cSide = 0;
          rSide = true;
          count++;
          continue;
        }
      } else {
        RETURN_IF_ERROR(rom->WriteByte(p2, lw_map_tiles_[count]));
        
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
        
        if (cSide >= 31) {
          cSide = 0;
          rSide = true;
          count++;
          continue;
        }
      } else {
        RETURN_IF_ERROR(rom->WriteByte(p4, lw_map_tiles_[count]));
        
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

  // Save Dark World specific section
  count = 0;
  int line = 0;
  while (true) {
    int addr = p5_addr + count + (line * 32);
    int src_index = 1040 + count + (line * 64);
    
    if (src_index < dw_map_tiles_.size()) {
      RETURN_IF_ERROR(rom->WriteByte(addr, dw_map_tiles_[src_index]));
    }
    
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

}  // namespace zelda3
}  // namespace yaze

