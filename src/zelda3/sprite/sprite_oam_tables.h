#ifndef YAZE_ZELDA3_SPRITE_SPRITE_OAM_TABLES_H
#define YAZE_ZELDA3_SPRITE_SPRITE_OAM_TABLES_H

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace yaze {
namespace zelda3 {

/**
 * @brief Single OAM entry for sprite rendering
 *
 * Represents one OAM tile relative to sprite origin.
 * Used for static (hardcoded) sprite rendering.
 */
struct SpriteOamEntry {
  int8_t x_offset;    // X offset relative to sprite origin
  int8_t y_offset;    // Y offset relative to sprite origin
  uint16_t tile_id;   // Tile ID in graphics buffer
  uint8_t palette;    // Palette index (0-7)
  bool size_16x16;    // false = 8x8, true = 16x16
  bool flip_x;        // Horizontal mirror
  bool flip_y;        // Vertical mirror
};

/**
 * @brief Complete OAM layout for a vanilla sprite
 *
 * Contains all tile definitions needed to render a sprite,
 * plus the graphics sheets required.
 */
struct SpriteOamLayout {
  uint8_t sprite_id;           // Sprite type ID (0x00-0xF2)
  const char* name;            // Display name
  std::vector<SpriteOamEntry> tiles;
  std::array<uint8_t, 4> required_sheets;  // Graphics sheet IDs needed
};

// ============================================================
// Common Sprite OAM Layouts
// ============================================================

// Sprite ID: 0x01 - Vulture
inline const SpriteOamLayout kVultureLayout = {
    .sprite_id = 0x01,
    .name = "Vulture",
    .tiles = {{0, 0, 0x20, 2, true, false, false},
              {0, -16, 0x00, 2, true, false, false}},
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

// Sprite ID: 0x08 - Octorok (1-way)
inline const SpriteOamLayout kOctorokLayout = {
    .sprite_id = 0x08,
    .name = "Octorok",
    .tiles = {{0, 0, 0x80, 6, true, false, false}},
    .required_sheets = {0x56, 0x57, 0x00, 0x00}};

// Sprite ID: 0x0B - Chicken
inline const SpriteOamLayout kChickenLayout = {
    .sprite_id = 0x0B,
    .name = "Chicken",
    .tiles = {{0, 0, 0x60, 2, true, false, false}},
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

// Sprite ID: 0x0E - Green Soldier (standing)
inline const SpriteOamLayout kGreenSoldierLayout = {
    .sprite_id = 0x0E,
    .name = "Green Soldier",
    .tiles = {{0, 8, 0x20, 6, true, false, false},   // Body
              {0, -8, 0x00, 6, true, false, false}}, // Head
    .required_sheets = {0x56, 0x57, 0x00, 0x00}};

// Sprite ID: 0x0F - Blue Soldier
inline const SpriteOamLayout kBlueSoldierLayout = {
    .sprite_id = 0x0F,
    .name = "Blue Soldier",
    .tiles = {{0, 8, 0x20, 1, true, false, false},   // Body
              {0, -8, 0x00, 1, true, false, false}}, // Head
    .required_sheets = {0x56, 0x57, 0x00, 0x00}};

// Sprite ID: 0x10 - Red Soldier
inline const SpriteOamLayout kRedSoldierLayout = {
    .sprite_id = 0x10,
    .name = "Red Soldier",
    .tiles = {{0, 8, 0x20, 0, true, false, false},   // Body
              {0, -8, 0x00, 0, true, false, false}}, // Head
    .required_sheets = {0x56, 0x57, 0x00, 0x00}};

// Sprite ID: 0x29 - Blue Guard
inline const SpriteOamLayout kBlueGuardLayout = {
    .sprite_id = 0x29,
    .name = "Blue Guard",
    .tiles = {{0, 8, 0x20, 1, true, false, false},   // Body
              {0, -8, 0x00, 1, true, false, false}}, // Head
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

// Sprite ID: 0x41 - Green Soldier (patrol)
inline const SpriteOamLayout kGreenPatrolLayout = {
    .sprite_id = 0x41,
    .name = "Green Patrol",
    .tiles = {{0, 8, 0x20, 6, true, false, false},   // Body
              {0, -8, 0x00, 6, true, false, false}}, // Head
    .required_sheets = {0x56, 0x57, 0x00, 0x00}};

// Sprite ID: 0x44 - Armos Knight
inline const SpriteOamLayout kArmosKnightLayout = {
    .sprite_id = 0x44,
    .name = "Armos Knight",
    .tiles = {{0, 0, 0x82, 4, true, false, false},
              {0, -16, 0x80, 4, true, false, false}},
    .required_sheets = {0x58, 0x59, 0x00, 0x00}};

// Sprite ID: 0x4B - Octoballoon
inline const SpriteOamLayout kOctoballoonLayout = {
    .sprite_id = 0x4B,
    .name = "Octoballoon",
    .tiles = {{0, 0, 0xE0, 4, true, false, false}},
    .required_sheets = {0x54, 0x55, 0x00, 0x00}};

// Sprite ID: 0x53 - Red Eyegore
inline const SpriteOamLayout kRedEyegoreLayout = {
    .sprite_id = 0x53,
    .name = "Red Eyegore",
    .tiles = {{0, 0, 0x84, 0, true, false, false},
              {0, -16, 0x80, 0, true, false, false}},
    .required_sheets = {0x58, 0x59, 0x00, 0x00}};

// Sprite ID: 0x54 - Green Eyegore
inline const SpriteOamLayout kGreenEyegoreLayout = {
    .sprite_id = 0x54,
    .name = "Green Eyegore",
    .tiles = {{0, 0, 0x84, 6, true, false, false},
              {0, -16, 0x80, 6, true, false, false}},
    .required_sheets = {0x58, 0x59, 0x00, 0x00}};

// Sprite ID: 0x64 - Moblin
inline const SpriteOamLayout kMoblinLayout = {
    .sprite_id = 0x64,
    .name = "Moblin",
    .tiles = {{0, 0, 0xC6, 5, true, false, false},
              {0, -16, 0xC4, 5, true, false, false}},
    .required_sheets = {0x54, 0x55, 0x00, 0x00}};

// Sprite ID: 0x81 - Hinox
inline const SpriteOamLayout kHinoxLayout = {
    .sprite_id = 0x81,
    .name = "Hinox",
    .tiles = {{-8, 0, 0xCC, 3, true, false, false},
              {8, 0, 0xCC, 3, true, true, false},
              {-8, -16, 0xC8, 3, true, false, false},
              {8, -16, 0xC8, 3, true, true, false}},
    .required_sheets = {0x5C, 0x5D, 0x00, 0x00}};

// Sprite ID: 0x88 - Link's Uncle (dying)
inline const SpriteOamLayout kUncleLayout = {
    .sprite_id = 0x88,
    .name = "Uncle",
    .tiles = {{0, 0, 0xAE, 7, true, false, false},
              {0, -16, 0x8E, 7, true, false, false}},
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

// Sprite ID: 0xD8 - Heart Container
inline const SpriteOamLayout kHeartContainerLayout = {
    .sprite_id = 0xD8,
    .name = "Heart Container",
    .tiles = {{0, 0, 0xEE, 0, true, false, false}},
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

// Sprite ID: 0xDA - Green Rupee
inline const SpriteOamLayout kGreenRupeeLayout = {
    .sprite_id = 0xDA,
    .name = "Green Rupee",
    .tiles = {{0, 0, 0xE8, 0, false, false, false}},
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

// Sprite ID: 0xDB - Blue Rupee
inline const SpriteOamLayout kBlueRupeeLayout = {
    .sprite_id = 0xDB,
    .name = "Blue Rupee",
    .tiles = {{0, 0, 0xE8, 1, false, false, false}},
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

// Sprite ID: 0xDC - Red Rupee
inline const SpriteOamLayout kRedRupeeLayout = {
    .sprite_id = 0xDC,
    .name = "Red Rupee",
    .tiles = {{0, 0, 0xE8, 2, false, false, false}},
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

// Sprite ID: 0xDE - Small Heart
inline const SpriteOamLayout kSmallHeartLayout = {
    .sprite_id = 0xDE,
    .name = "Small Heart",
    .tiles = {{0, 0, 0xEC, 0, false, false, false}},
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

// Sprite ID: 0xDF - Key
inline const SpriteOamLayout kKeyLayout = {
    .sprite_id = 0xDF,
    .name = "Key",
    .tiles = {{0, 0, 0xF4, 7, false, false, false}},
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

// Sprite ID: 0xE1 - Small Magic
inline const SpriteOamLayout kSmallMagicLayout = {
    .sprite_id = 0xE1,
    .name = "Small Magic",
    .tiles = {{0, 0, 0xF0, 6, false, false, false}},
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

// Sprite ID: 0xE2 - Large Magic
inline const SpriteOamLayout kLargeMagicLayout = {
    .sprite_id = 0xE2,
    .name = "Large Magic",
    .tiles = {{0, 0, 0xF6, 6, false, false, false}},
    .required_sheets = {0x52, 0x53, 0x00, 0x00}};

/**
 * @brief Registry of all known sprite OAM layouts
 *
 * Provides lookup by sprite ID for static rendering.
 */
class SpriteOamRegistry {
 public:
  /**
   * @brief Get the OAM layout for a sprite ID
   * @param sprite_id The sprite type ID
   * @return Pointer to layout, or nullptr if not defined
   */
  static const SpriteOamLayout* GetLayout(uint8_t sprite_id);

  /**
   * @brief Get required graphics sheets for a sprite
   * @param sprite_id The sprite type ID
   * @return Array of sheet IDs, or empty if not defined
   */
  static std::optional<std::array<uint8_t, 4>> GetRequiredSheets(
      uint8_t sprite_id);

  /**
   * @brief Check if a sprite has a defined layout
   * @param sprite_id The sprite type ID
   * @return true if layout exists
   */
  static bool HasLayout(uint8_t sprite_id);

  /**
   * @brief Get all defined layouts
   * @return Vector of all sprite layouts
   */
  static std::vector<const SpriteOamLayout*> GetAllLayouts();
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_SPRITE_SPRITE_OAM_TABLES_H
