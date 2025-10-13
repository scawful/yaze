#include "app/gfx/types/snes_palette.h"

#include <SDL.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/types/snes_color.h"
#include "imgui/imgui.h"
#include "util/macro.h"

namespace yaze::gfx {

SnesPalette::SnesPalette(char *data) {
  assert((sizeof(data) % 4 == 0) && (sizeof(data) <= 32));
  for (unsigned i = 0; i < sizeof(data); i += 2) {
    SnesColor col;
    col.set_snes(static_cast<uint8_t>(data[i + 1]) << 8);
    col.set_snes(col.snes() | static_cast<uint8_t>(data[i]));
    snes_color mColor = ConvertSnesToRgb(col.snes());
    col.set_rgb(ImVec4(mColor.red, mColor.green, mColor.blue, 1.f));
    colors_[size_++] = col;
  }
}

SnesPalette::SnesPalette(const unsigned char *snes_pal) {
  assert((sizeof(snes_pal) % 4 == 0) && (sizeof(snes_pal) <= 32));
  for (unsigned i = 0; i < sizeof(snes_pal); i += 2) {
    SnesColor col;
    col.set_snes(snes_pal[i + 1] << (uint16_t)8);
    col.set_snes(col.snes() | snes_pal[i]);
    snes_color mColor = ConvertSnesToRgb(col.snes());
    col.set_rgb(ImVec4(mColor.red, mColor.green, mColor.blue, 1.f));
    colors_[size_++] = col;
  }
}

SnesPalette::SnesPalette(const char *data, size_t length) : size_(0) {
  for (size_t i = 0; i < length && size_ < kMaxColors; i += 2) {
    uint16_t color = (static_cast<uint8_t>(data[i + 1]) << 8) |
                     static_cast<uint8_t>(data[i]);
    colors_[size_++] = SnesColor(color);
  }
}

SnesPalette::SnesPalette(const std::vector<uint16_t> &colors) : size_(0) {
  for (const auto &color : colors) {
    if (size_ < kMaxColors) {
      colors_[size_++] = SnesColor(color);
    }
  }
}

SnesPalette::SnesPalette(const std::vector<SnesColor> &colors) : size_(0) {
  for (const auto &color : colors) {
    if (size_ < kMaxColors) {
      colors_[size_++] = color;
    }
  }
}

SnesPalette::SnesPalette(const std::vector<ImVec4> &colors) : size_(0) {
  for (const auto &color : colors) {
    if (size_ < kMaxColors) {
      colors_[size_++] = SnesColor(color);
    }
  }
}

/**
 * @namespace yaze::gfx::palette_group_internal
 * @brief Internal functions for loading palettes by group.
 */
namespace palette_group_internal {
absl::Status LoadOverworldMainPalettes(const std::vector<uint8_t> &rom_data,
                                       gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 6; i++) {
    palette_groups.overworld_main.AddPalette(
        gfx::ReadPaletteFromRom(kOverworldPaletteMain + (i * (35 * 2)),
                                /*num_colors=*/35, data));
  }
  return absl::OkStatus();
}

absl::Status LoadOverworldAuxiliaryPalettes(
    const std::vector<uint8_t> &rom_data,
    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 20; i++) {
    palette_groups.overworld_aux.AddPalette(
        gfx::ReadPaletteFromRom(kOverworldPaletteAux + (i * (21 * 2)),
                                /*num_colors=*/21, data));
  }
  return absl::OkStatus();
}

absl::Status LoadOverworldAnimatedPalettes(
    const std::vector<uint8_t> &rom_data,
    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 14; i++) {
    palette_groups.overworld_animated.AddPalette(gfx::ReadPaletteFromRom(
        kOverworldPaletteAnimated + (i * (7 * 2)), /*num_colors=*/7, data));
  }
  return absl::OkStatus();
}

absl::Status LoadHUDPalettes(const std::vector<uint8_t> &rom_data,
                             gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 2; i++) {
    palette_groups.hud.AddPalette(gfx::ReadPaletteFromRom(
        kHudPalettes + (i * 64), /*num_colors=*/32, data));
  }
  return absl::OkStatus();
}

absl::Status LoadGlobalSpritePalettes(const std::vector<uint8_t> &rom_data,
                                      gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  palette_groups.global_sprites.AddPalette(
      gfx::ReadPaletteFromRom(kGlobalSpritesLW, /*num_colors=*/60, data));
  palette_groups.global_sprites.AddPalette(gfx::ReadPaletteFromRom(
      kGlobalSpritePalettesDW, /*num_colors=*/60, data));
  return absl::OkStatus();
}

absl::Status LoadArmorPalettes(const std::vector<uint8_t> &rom_data,
                               gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 5; i++) {
    palette_groups.armors.AddPalette(gfx::ReadPaletteFromRom(
        kArmorPalettes + (i * 30), /*num_colors=*/15, data));
  }
  return absl::OkStatus();
}

absl::Status LoadSwordPalettes(const std::vector<uint8_t> &rom_data,
                               gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 4; i++) {
    palette_groups.swords.AddPalette(gfx::ReadPaletteFromRom(
        kSwordPalettes + (i * 6), /*num_colors=*/3, data));
  }
  return absl::OkStatus();
}

absl::Status LoadShieldPalettes(const std::vector<uint8_t> &rom_data,
                                gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 3; i++) {
    palette_groups.shields.AddPalette(gfx::ReadPaletteFromRom(
        kShieldPalettes + (i * 8), /*num_colors=*/4, data));
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux1Palettes(const std::vector<uint8_t> &rom_data,
                                    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 12; i++) {
    palette_groups.sprites_aux1.AddPalette(gfx::ReadPaletteFromRom(
        kSpritesPalettesAux1 + (i * 14), /*num_colors=*/7, data));
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux2Palettes(const std::vector<uint8_t> &rom_data,
                                    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 11; i++) {
    palette_groups.sprites_aux2.AddPalette(gfx::ReadPaletteFromRom(
        kSpritesPalettesAux2 + (i * 14), /*num_colors=*/7, data));
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux3Palettes(const std::vector<uint8_t> &rom_data,
                                    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 24; i++) {
    palette_groups.sprites_aux3.AddPalette(gfx::ReadPaletteFromRom(
        kSpritesPalettesAux3 + (i * 14), /*num_colors=*/7, data));
  }
  return absl::OkStatus();
}

absl::Status LoadDungeonMainPalettes(const std::vector<uint8_t> &rom_data,
                                     gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 20; i++) {
    palette_groups.dungeon_main.AddPalette(gfx::ReadPaletteFromRom(
        kDungeonMainPalettes + (i * 180), /*num_colors=*/90, data));
  }
  return absl::OkStatus();
}

absl::Status LoadGrassColors(const std::vector<uint8_t> &rom_data,
                             gfx::PaletteGroupMap &palette_groups) {
  palette_groups.grass.AddColor(
      gfx::ReadColorFromRom(kHardcodedGrassLW, rom_data.data()));
  palette_groups.grass.AddColor(
      gfx::ReadColorFromRom(kHardcodedGrassDW, rom_data.data()));
  palette_groups.grass.AddColor(
      gfx::ReadColorFromRom(kHardcodedGrassSpecial, rom_data.data()));
  return absl::OkStatus();
}

absl::Status Load3DObjectPalettes(const std::vector<uint8_t> &rom_data,
                                  gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  palette_groups.object_3d.AddPalette(
      gfx::ReadPaletteFromRom(kTriforcePalette, 8, data));
  palette_groups.object_3d.AddPalette(
      gfx::ReadPaletteFromRom(kCrystalPalette, 8, data));
  return absl::OkStatus();
}

absl::Status LoadOverworldMiniMapPalettes(
    const std::vector<uint8_t> &rom_data,
    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 2; i++) {
    palette_groups.overworld_mini_map.AddPalette(gfx::ReadPaletteFromRom(
        kOverworldMiniMapPalettes + (i * 256), /*num_colors=*/128, data));
  }
  return absl::OkStatus();
}
}  // namespace palette_group_internal

const absl::flat_hash_map<std::string, uint32_t> kPaletteGroupAddressMap = {
    {"ow_main", kOverworldPaletteMain},
    {"ow_aux", kOverworldPaletteAux},
    {"ow_animated", kOverworldPaletteAnimated},
    {"hud", kHudPalettes},
    {"global_sprites", kGlobalSpritesLW},
    {"armors", kArmorPalettes},
    {"swords", kSwordPalettes},
    {"shields", kShieldPalettes},
    {"sprites_aux1", kSpritesPalettesAux1},
    {"sprites_aux2", kSpritesPalettesAux2},
    {"sprites_aux3", kSpritesPalettesAux3},
    {"dungeon_main", kDungeonMainPalettes},
    {"grass", kHardcodedGrassLW},
    {"3d_object", kTriforcePalette},
    {"ow_mini_map", kOverworldMiniMapPalettes},
};

const absl::flat_hash_map<std::string, uint32_t> kPaletteGroupColorCounts = {
    {"ow_main", 35},     {"ow_aux", 21},         {"ow_animated", 7},
    {"hud", 32},         {"global_sprites", 60}, {"armors", 15},
    {"swords", 3},       {"shields", 4},         {"sprites_aux1", 7},
    {"sprites_aux2", 7}, {"sprites_aux3", 7},    {"dungeon_main", 90},
    {"grass", 1},        {"3d_object", 8},       {"ow_mini_map", 128},
};

uint32_t GetPaletteAddress(const std::string &group_name, size_t palette_index,
                           size_t color_index) {
  // Retrieve the base address for the palette group
  uint32_t base_address = kPaletteGroupAddressMap.at(group_name);

  // Retrieve the number of colors for each palette in the group
  uint32_t colors_per_palette = kPaletteGroupColorCounts.at(group_name);

  // Calculate the address for thes specified color in the ROM
  uint32_t address = base_address + (palette_index * colors_per_palette * 2) +
                     (color_index * 2);

  return address;
}

/**
 * @brief Read a palette from ROM data
 * 
 * SNES ROM stores colors in 15-bit BGR format (2 bytes each):
 * - Byte 0: rrrrrggg (low byte)
 * - Byte 1: 0bbbbbgg (high byte)
 * - Full format: 0bbbbbgggggrrrrr
 * 
 * This function:
 * 1. Reads SNES 15-bit colors from ROM
 * 2. Converts to RGB 0-255 range (multiply by 8 to expand 5-bit to 8-bit)
 * 3. Creates SnesColor objects that store all formats
 * 
 * IMPORTANT: Transparency is NOT marked here!
 * - The SNES hardware automatically treats color index 0 of each sub-palette as transparent
 * - This is a rendering concern, not a data property
 * - ROM palette data stores actual color values, including at index 0
 * - Transparency is applied later during rendering (in SetPaletteWithTransparent or SDL)
 * 
 * @param offset ROM offset to start reading
 * @param num_colors Number of colors to read
 * @param rom Pointer to ROM data
 * @return SnesPalette containing the colors (no transparency flags set)
 */
SnesPalette ReadPaletteFromRom(int offset, int num_colors, const uint8_t *rom) {
  int color_offset = 0;
  std::vector<gfx::SnesColor> colors(num_colors);

  while (color_offset < num_colors) {
    // Read SNES 15-bit color (little endian)
    uint16_t snes_color_word = (uint16_t)((rom[offset + 1]) << 8) | rom[offset];
    
    // Extract RGB components (5-bit each) and expand to 8-bit (0-255)
    snes_color new_color;
    new_color.red = (snes_color_word & 0x1F) * 8;        // Bits 0-4
    new_color.green = ((snes_color_word >> 5) & 0x1F) * 8;  // Bits 5-9
    new_color.blue = ((snes_color_word >> 10) & 0x1F) * 8; // Bits 10-14
    
    // Create SnesColor by converting RGB back to SNES format
    // (This ensures all internal representations are consistent)
    colors[color_offset].set_snes(ConvertRgbToSnes(new_color));
    
    // DO NOT mark as transparent - preserve actual ROM color data!
    // Transparency is handled at render time, not in the data
    
    color_offset++;
    offset += 2;  // SNES colors are 2 bytes each
  }

  return gfx::SnesPalette(colors);
}

std::array<float, 4> ToFloatArray(const SnesColor &color) {
  std::array<float, 4> colorArray;
  colorArray[0] = color.rgb().x / 255.0f;
  colorArray[1] = color.rgb().y / 255.0f;
  colorArray[2] = color.rgb().z / 255.0f;
  colorArray[3] = color.rgb().w;
  return colorArray;
}

absl::StatusOr<PaletteGroup> CreatePaletteGroupFromColFile(
    std::vector<SnesColor> &palette_rows) {
  PaletteGroup palette_group;
  for (int i = 0; i < palette_rows.size(); i += 8) {
    SnesPalette palette;
    for (int j = 0; j < 8; j++) {
      palette.AddColor(palette_rows[i + j]);
    }
    palette_group.AddPalette(palette);
  }
  return palette_group;
}

/**
 * @brief Create a PaletteGroup by dividing a large palette into sub-palettes
 * 
 * Takes a large palette (e.g., 256 colors) and divides it into smaller
 * palettes of num_colors each (typically 8 colors for SNES).
 * 
 * IMPORTANT: Does NOT mark colors as transparent!
 * - Color data is preserved as-is from the source palette
 * - Transparency is a rendering concern handled by SetPaletteWithTransparent
 * - The SNES hardware handles color 0 transparency automatically
 * 
 * @param palette Source palette to divide
 * @param num_colors Number of colors per sub-palette (default 8)
 * @return PaletteGroup containing the sub-palettes
 */
absl::StatusOr<PaletteGroup> CreatePaletteGroupFromLargePalette(
    SnesPalette &palette, int num_colors) {
  PaletteGroup palette_group;
  for (int i = 0; i < palette.size(); i += num_colors) {
    SnesPalette new_palette;
    if (i + num_colors <= palette.size()) {
      for (int j = 0; j < num_colors; j++) {
        auto color = palette[i + j];
        // DO NOT mark as transparent - preserve actual color data!
        // Transparency is handled at render time, not in the data
        new_palette.AddColor(color);
      }
    }
    palette_group.AddPalette(new_palette);
  }
  return palette_group;
}

using namespace palette_group_internal;

// TODO: Refactor LoadAllPalettes to use group names, move to zelda3 namespace
absl::Status LoadAllPalettes(const std::vector<uint8_t> &rom_data,
                             PaletteGroupMap &groups) {
  RETURN_IF_ERROR(LoadOverworldMainPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadOverworldAuxiliaryPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadOverworldAnimatedPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadHUDPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadGlobalSpritePalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadArmorPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadSwordPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadShieldPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadSpriteAux1Palettes(rom_data, groups))
  RETURN_IF_ERROR(LoadSpriteAux2Palettes(rom_data, groups))
  RETURN_IF_ERROR(LoadSpriteAux3Palettes(rom_data, groups))
  RETURN_IF_ERROR(LoadDungeonMainPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadGrassColors(rom_data, groups))
  RETURN_IF_ERROR(Load3DObjectPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadOverworldMiniMapPalettes(rom_data, groups))
  return absl::OkStatus();
}

std::unordered_map<uint8_t, gfx::Paletteset> GfxContext::palettesets_;

}  // namespace yaze::gfx
