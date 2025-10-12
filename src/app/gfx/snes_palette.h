#ifndef YAZE_APP_GFX_PALETTE_H
#define YAZE_APP_GFX_PALETTE_H

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/snes_color.h"
#include "imgui/imgui.h"
#include "snes_color.h"
#include "util/macro.h"

namespace yaze {
namespace gfx {

constexpr int kNumPalettes = 14;

enum PaletteCategory {
  kSword,
  kShield,
  kClothes,
  kWorldColors,
  kAreaColors,
  kGlobalSprites,
  kSpritesAux1,
  kSpritesAux2,
  kSpritesAux3,
  kDungeons,
  kWorldMap,
  kDungeonMap,
  kTriforce,
  kCrystal
};

static constexpr absl::string_view kPaletteCategoryNames[] = {
    "Sword",        "Shield",         "Clothes",      "World Colors",
    "Area Colors",  "Global Sprites", "Sprites Aux1", "Sprites Aux2",
    "Sprites Aux3", "Dungeons",       "World Map",    "Dungeon Map",
    "Triforce",     "Crystal"};

static constexpr absl::string_view kPaletteGroupNames[] = {
    "swords",       "shields",        "armors",       "ow_main",
    "ow_aux",       "global_sprites", "sprites_aux1", "sprites_aux2",
    "sprites_aux3", "dungeon_main",   "ow_mini_map",  "ow_mini_map",
    "3d_object",    "3d_object"};

constexpr const char* kPaletteGroupAddressesKeys[] = {
    "ow_main",        "ow_aux",       "ow_animated",  "hud",
    "global_sprites", "armors",       "swords",       "shields",
    "sprites_aux1",   "sprites_aux2", "sprites_aux3", "dungeon_main",
    "grass",          "3d_object",    "ow_mini_map",
};

constexpr int kOverworldPaletteMain = 0xDE6C8;
constexpr int kOverworldPaletteAux = 0xDE86C;
constexpr int kOverworldPaletteAnimated = 0xDE604;
constexpr int kGlobalSpritesLW = 0xDD218;
constexpr int kGlobalSpritePalettesDW = 0xDD290;

/** < Green, Blue, Red, Bunny, Electrocuted (15 colors each) */
constexpr int kArmorPalettes = 0xDD308;
constexpr int kSpritesPalettesAux1 = 0xDD39E;  // 7 colors each
constexpr int kSpritesPalettesAux2 = 0xDD446;  // 7 colors each
constexpr int kSpritesPalettesAux3 = 0xDD4E0;  // 7 colors each
constexpr int kSwordPalettes = 0xDD630;        // 3 colors each - 4 entries
constexpr int kShieldPalettes = 0xDD648;       // 4 colors each - 3 entries
constexpr int kHudPalettes = 0xDD660;
constexpr int kDungeonMapPalettes = 0xDD70A;  // 21 colors

// (15*6) colors each - 20 entries
constexpr int kDungeonMainPalettes = 0xDD734;
constexpr int kDungeonMapBgPalettes = 0xDE544;  // 16*6

// Mirrored Value at 0x75645 : 0x75625
constexpr int kHardcodedGrassLW = 0x5FEA9;
constexpr int kHardcodedGrassDW = 0x05FEB3;  // 0x7564F
constexpr int kHardcodedGrassSpecial = 0x75640;
constexpr int kOverworldMiniMapPalettes = 0x55B27;
constexpr int kTriforcePalette = 0x64425;
constexpr int kCrystalPalette = 0xF4CD3;

/** < 2 bytes for each overworld area (320) */
constexpr int CustomAreaSpecificBGPalette = 0x140000;
constexpr int CustomAreaSpecificBGASM = 0x140150;

// 1 byte, not 0 if enabled
constexpr int kCustomAreaSpecificBGEnabled = 0x140140;

constexpr int HudPalettesMax = 2;
constexpr int OverworldMainPalettesMax = 6;
constexpr int OverworldAuxPalettesMax = 20;
constexpr int OverworldAnimatedPalettesMax = 14;
constexpr int GlobalSpritePalettesMax = 2;
constexpr int ArmorPalettesMax = 5;
constexpr int SwordsPalettesMax = 4;
constexpr int SpritesAux1PalettesMax = 12;
constexpr int SpritesAux2PalettesMax = 11;
constexpr int SpritesAux3PalettesMax = 24;
constexpr int ShieldsPalettesMax = 3;
constexpr int DungeonsMainPalettesMax = 20;
constexpr int OverworldBackgroundPaletteMax = 160;
constexpr int OverworldGrassPalettesMax = 3;
constexpr int Object3DPalettesMax = 2;
constexpr int OverworldMiniMapPalettesMax = 2;

uint32_t GetPaletteAddress(const std::string& group_name, size_t palette_index,
                           size_t color_index);

/**
 * @brief Represents a palette of colors for the Super Nintendo Entertainment
 * System (SNES).
 *
 * The `SnesPalette` class provides functionality to create, modify, and access
 * colors in an SNES palette. It supports various constructors to initialize the
 * palette with different types of data. The palette can be modified by adding
 * or changing colors, and it can be cleared to remove all colors. Colors in the
 * palette can be accessed using index-based access. The class also provides a
 * method to create a sub-palette by selecting a range of colors from the
 * original palette.
 */
class SnesPalette {
 public:
  static constexpr size_t kMaxColors = 256;
  using ColorArray = std::array<SnesColor, kMaxColors>;

  SnesPalette() : size_(0) {}
  SnesPalette(char* data);
  SnesPalette(const unsigned char* snes_pal);
  SnesPalette(const char* data, size_t length);
  SnesPalette(const std::vector<uint16_t>& colors);
  SnesPalette(const std::vector<SnesColor>& colors);
  SnesPalette(const std::vector<ImVec4>& colors);

  const SnesColor& operator[](size_t index) const { return colors_[index]; }
  SnesColor& operator[](size_t index) { return colors_[index]; }

  void set_size(size_t size) { size_ = size; }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  // Resize
  void Resize(size_t size) { size_ = size; }

  auto begin() { return colors_.begin(); }
  auto end() { return colors_.begin() + size_; }
  auto begin() const { return colors_.begin(); }
  auto end() const { return colors_.begin() + size_; }

  void AddColor(const SnesColor& color) {
    if (size_ < kMaxColors) {
      colors_[size_++] = color;
    }
  }

  void UpdateColor(size_t index, const SnesColor& color) {
    if (index < size_) {
      colors_[index] = color;
    }
  }

  void clear() { size_ = 0; }

  SnesPalette sub_palette(size_t start, size_t length) const {
    SnesPalette result;
    if (start >= size_) {
      return result;
    }
    length = std::min(length, size_ - start);
    for (size_t i = 0; i < length; ++i) {
      result.AddColor(colors_[start + i]);
    }
    return result;
  }

  bool operator==(const SnesPalette& other) const {
    if (size_ != other.size_) {
      return false;
    }
    for (size_t i = 0; i < size_; ++i) {
      if (colors_[i].snes() != other.colors_[i].snes()) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const SnesPalette& other) const { return !(*this == other); }

 private:
  ColorArray colors_;
  size_t size_;
};

SnesPalette ReadPaletteFromRom(int offset, int num_colors, const uint8_t* rom);

std::array<float, 4> ToFloatArray(const SnesColor& color);

/**
 * @brief Represents a group of palettes.
 *
 * Supports adding palettes and colors, clearing the group, and accessing
 * palettes and colors by index.
 */
struct PaletteGroup {
  PaletteGroup() = default;
  PaletteGroup(const std::string& name) : name_(name) {}

  // ========== Basic Operations ==========
  
  void AddPalette(SnesPalette pal) { palettes.emplace_back(pal); }

  void AddColor(SnesColor color) {
    if (palettes.empty()) {
      palettes.emplace_back();
    }
    palettes[0].AddColor(color);
  }

  void clear() { palettes.clear(); }
  void resize(size_t new_size) { palettes.resize(new_size); }
  
  // ========== Accessors ==========
  
  auto name() const { return name_; }
  auto size() const { return palettes.size(); }
  bool empty() const { return palettes.empty(); }
  
  // Const access
  auto palette(int i) const { return palettes[i]; }
  const SnesPalette& palette_ref(int i) const { return palettes[i]; }
  
  // Mutable access
  auto mutable_palette(int i) { return &palettes[i]; }
  SnesPalette& palette_ref(int i) { return palettes[i]; }

  // ========== Color Operations ==========
  
  /**
   * @brief Get a specific color from a palette
   * @param palette_index The palette index
   * @param color_index The color index within the palette
   * @return The color, or SnesColor() if indices are invalid
   */
  SnesColor GetColor(int palette_index, int color_index) const {
    if (palette_index >= 0 && palette_index < palettes.size()) {
      const auto& pal = palettes[palette_index];
      if (color_index >= 0 && color_index < pal.size()) {
        return pal[color_index];
      }
    }
    return SnesColor();
  }
  
  /**
   * @brief Set a specific color in a palette
   * @param palette_index The palette index
   * @param color_index The color index within the palette
   * @param color The new color value
   * @return true if color was set successfully
   */
  bool SetColor(int palette_index, int color_index, const SnesColor& color) {
    if (palette_index >= 0 && palette_index < palettes.size()) {
      auto& pal = palettes[palette_index];
      if (color_index >= 0 && color_index < pal.size()) {
        pal[color_index] = color;
        return true;
      }
    }
    return false;
  }
  
  // ========== Operator Overloads ==========

  SnesPalette operator[](int i) {
    if (i >= palettes.size()) {
      std::cout << "PaletteGroup: Index " << i << " out of bounds (size: " 
                << palettes.size() << ")" << std::endl;
      return SnesPalette();
    }
    return palettes[i];
  }

  const SnesPalette& operator[](int i) const {
    if (i >= palettes.size()) {
      std::cout << "PaletteGroup: Index " << i << " out of bounds (size: " 
                << palettes.size() << ")" << std::endl;
      static const SnesPalette empty_palette;
      return empty_palette;
    }
    return palettes[i];
  }

 private:
  std::string name_;
  std::vector<SnesPalette> palettes;
};

/**
 * @brief Represents a mapping of palette groups.
 *
 * Originally, this was an actual std::unordered_map but since the palette
 * groups supported never change, it was changed to a struct with a method to
 * get the group by name.
 */
struct PaletteGroupMap {
  PaletteGroup overworld_main = {kPaletteGroupAddressesKeys[0]};
  PaletteGroup overworld_aux = {kPaletteGroupAddressesKeys[1]};
  PaletteGroup overworld_animated = {kPaletteGroupAddressesKeys[2]};
  PaletteGroup hud = {kPaletteGroupAddressesKeys[3]};
  PaletteGroup global_sprites = {kPaletteGroupAddressesKeys[4]};
  PaletteGroup armors = {kPaletteGroupAddressesKeys[5]};
  PaletteGroup swords = {kPaletteGroupAddressesKeys[6]};
  PaletteGroup shields = {kPaletteGroupAddressesKeys[7]};
  PaletteGroup sprites_aux1 = {kPaletteGroupAddressesKeys[8]};
  PaletteGroup sprites_aux2 = {kPaletteGroupAddressesKeys[9]};
  PaletteGroup sprites_aux3 = {kPaletteGroupAddressesKeys[10]};
  PaletteGroup dungeon_main = {kPaletteGroupAddressesKeys[11]};
  PaletteGroup grass = {kPaletteGroupAddressesKeys[12]};
  PaletteGroup object_3d = {kPaletteGroupAddressesKeys[13]};
  PaletteGroup overworld_mini_map = {kPaletteGroupAddressesKeys[14]};

  auto get_group(const std::string& group_name) {
    if (group_name == "ow_main") {
      return &overworld_main;
    } else if (group_name == "ow_aux") {
      return &overworld_aux;
    } else if (group_name == "ow_animated") {
      return &overworld_animated;
    } else if (group_name == "hud") {
      return &hud;
    } else if (group_name == "global_sprites") {
      return &global_sprites;
    } else if (group_name == "armors") {
      return &armors;
    } else if (group_name == "swords") {
      return &swords;
    } else if (group_name == "shields") {
      return &shields;
    } else if (group_name == "sprites_aux1") {
      return &sprites_aux1;
    } else if (group_name == "sprites_aux2") {
      return &sprites_aux2;
    } else if (group_name == "sprites_aux3") {
      return &sprites_aux3;
    } else if (group_name == "dungeon_main") {
      return &dungeon_main;
    } else if (group_name == "grass") {
      return &grass;
    } else if (group_name == "3d_object") {
      return &object_3d;
    } else if (group_name == "ow_mini_map") {
      return &overworld_mini_map;
    } else {
      throw std::out_of_range("PaletteGroupMap: Group not found");
    }
  }

  template <typename Func>
  absl::Status for_each(Func&& func) {
    RETURN_IF_ERROR(func(overworld_aux));
    RETURN_IF_ERROR(func(overworld_animated));
    RETURN_IF_ERROR(func(hud));
    RETURN_IF_ERROR(func(global_sprites));
    RETURN_IF_ERROR(func(armors));
    RETURN_IF_ERROR(func(swords));
    RETURN_IF_ERROR(func(shields));
    RETURN_IF_ERROR(func(sprites_aux1));
    RETURN_IF_ERROR(func(sprites_aux2));
    RETURN_IF_ERROR(func(sprites_aux3));
    RETURN_IF_ERROR(func(dungeon_main));
    RETURN_IF_ERROR(func(grass));
    RETURN_IF_ERROR(func(object_3d));
    RETURN_IF_ERROR(func(overworld_mini_map));
    return absl::OkStatus();
  }

  void clear() {
    overworld_main.clear();
    overworld_aux.clear();
    overworld_animated.clear();
    hud.clear();
    global_sprites.clear();
    armors.clear();
    swords.clear();
    shields.clear();
    sprites_aux1.clear();
    sprites_aux2.clear();
    sprites_aux3.clear();
    dungeon_main.clear();
    grass.clear();
    object_3d.clear();
    overworld_mini_map.clear();
  }

  bool empty() {
    return overworld_main.size() == 0 && overworld_aux.size() == 0 &&
           overworld_animated.size() == 0 && hud.size() == 0 &&
           global_sprites.size() == 0 && armors.size() == 0 &&
           swords.size() == 0 && shields.size() == 0 &&
           sprites_aux1.size() == 0 && sprites_aux2.size() == 0 &&
           sprites_aux3.size() == 0 && dungeon_main.size() == 0 &&
           grass.size() == 0 && object_3d.size() == 0 &&
           overworld_mini_map.size() == 0;
  }
};

absl::StatusOr<PaletteGroup> CreatePaletteGroupFromColFile(
    std::vector<SnesColor>& colors);

/**
 * @brief Take a SNESPalette, divide it into palettes of 8 colors
 */
absl::StatusOr<PaletteGroup> CreatePaletteGroupFromLargePalette(
    SnesPalette& palette, int num_colors = 8);

/**
 * @brief Loads all the palettes for the game.
 *
 * This function loads all the palettes for the game, including overworld,
 * HUD, armor, swords, shields, sprites, dungeon, grass, and 3D object
 * palettes. It also adds the loaded palettes to their respective palette
 * groups.
 *
 */
absl::Status LoadAllPalettes(const std::vector<uint8_t>& rom_data,
                             PaletteGroupMap& groups);

/**
 * @brief Represents a set of palettes used in a SNES graphics system.
 */
struct Paletteset {
  /**
   * @brief Default constructor for Paletteset.
   */
  Paletteset() = default;

  /**
   * @brief Constructor for Paletteset.
   * @param main The main palette.
   * @param animated The animated palette.
   * @param aux1 The first auxiliary palette.
   * @param aux2 The second auxiliary palette.
   * @param background The background color.
   * @param hud The HUD palette.
   * @param spr The sprite palette.
   * @param spr2 The second sprite palette.
   * @param comp The composite palette.
   */
  Paletteset(const gfx::SnesPalette& main, const gfx::SnesPalette& animated,
             const gfx::SnesPalette& aux1, const gfx::SnesPalette& aux2,
             const gfx::SnesColor& background, const gfx::SnesPalette& hud,
             const gfx::SnesPalette& spr, const gfx::SnesPalette& spr2,
             const gfx::SnesPalette& comp)
      : main_(main),
        animated(animated),
        aux1(aux1),
        aux2(aux2),
        background(background),
        hud(hud),
        spr(spr),
        spr2(spr2),
        composite(comp) {}

  gfx::SnesPalette main_;     /**< The main palette. */
  gfx::SnesPalette animated;  /**< The animated palette. */
  gfx::SnesPalette aux1;      /**< The first auxiliary palette. */
  gfx::SnesPalette aux2;      /**< The second auxiliary palette. */
  gfx::SnesColor background;  /**< The background color. */
  gfx::SnesPalette hud;       /**< The HUD palette. */
  gfx::SnesPalette spr;       /**< The sprite palette. */
  gfx::SnesPalette spr2;      /**< The second sprite palette. */
  gfx::SnesPalette composite; /**< The composite palette. */
};

/**
 * @brief Shared graphical context across editors.
 */
class GfxContext {
 protected:
  // Palettesets for the tile16 individual tiles
  static std::unordered_map<uint8_t, gfx::Paletteset> palettesets_;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_PALETTE_H
