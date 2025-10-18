#ifndef YAZE_APP_ZELDA3_OVERWORLD_EXIT_H
#define YAZE_APP_ZELDA3_OVERWORLD_EXIT_H

#include <cstdint>
#include <cstdlib>
#include <algorithm>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/rom.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::zelda3 {

// Forward declaration to avoid circular dependency
class Overworld;

constexpr int kNumOverworldExits = 0x4F;
constexpr int OWExitRoomId = 0x15D8A;  // 0x15E07 Credits sequences
// 105C2 Ending maps
// 105E2 Sprite Group Table for Ending
constexpr int OWExitMapId = 0x15E28;
constexpr int OWExitVram = 0x15E77;
constexpr int OWExitYScroll = 0x15F15;
constexpr int OWExitXScroll = 0x15FB3;
constexpr int OWExitYPlayer = 0x16051;
constexpr int OWExitXPlayer = 0x160EF;
constexpr int OWExitYCamera = 0x1618D;
constexpr int OWExitXCamera = 0x1622B;
constexpr int OWExitDoorPosition = 0x15724;
constexpr int OWExitUnk1 = 0x162C9;
constexpr int OWExitUnk2 = 0x16318;
constexpr int OWExitDoorType1 = 0x16367;
constexpr int OWExitDoorType2 = 0x16405;

constexpr int OWExitMapIdWhirlpool = 0x16AE5;    //  JP = ;016849
constexpr int OWExitVramWhirlpool = 0x16B07;     //  JP = ;01686B
constexpr int OWExitYScrollWhirlpool = 0x16B29;  // JP = ;01688D
constexpr int OWExitXScrollWhirlpool = 0x16B4B;  // JP = ;016DE7
constexpr int OWExitYPlayerWhirlpool = 0x16B6D;  // JP = ;016E09
constexpr int OWExitXPlayerWhirlpool = 0x16B8F;  // JP = ;016E2B
constexpr int OWExitYCameraWhirlpool = 0x16BB1;  // JP = ;016E4D
constexpr int OWExitXCameraWhirlpool = 0x16BD3;  // JP = ;016E6F
constexpr int OWExitUnk1Whirlpool = 0x16BF5;     //    JP = ;016E91
constexpr int OWExitUnk2Whirlpool = 0x16C17;     //    JP = ;016EB3
constexpr int OWWhirlpoolPosition = 0x16CF8;     //    JP = ;016F94

/**
 * @class OverworldExit
 * @brief Represents an overworld exit that transitions from dungeon to overworld
 * 
 * Coordinate System (inherited from GameEntity):
 * - x_, y_: World pixel coordinates (ZScream: PlayerX/PlayerY)
 * - game_x_, game_y_: Map-local tile coordinates (ZScream: AreaX/AreaY)
 * - map_id_: Parent map ID (ZScream: MapID)
 * 
 * Exit-Specific Properties:
 * - x_player_, y_player_: Player spawn position in world (saved to ROM)
 * - x_scroll_, y_scroll_: Camera scroll position
 * - x_camera_, y_camera_: Camera center position
 * - room_id_: Target dungeon room ID (ZScream: RoomID)
 * - is_automatic_: If true, scroll/camera auto-calculated from player position (ZScream: IsAutomatic)
 * 
 * ZScream Reference: ExitOW.cs, ExitMode.cs
 */
class OverworldExit : public GameEntity {
 public:
  uint16_t y_scroll_;
  uint16_t x_scroll_;
  uint16_t y_player_;  // Player spawn Y (0-4088 range, ZScream: PlayerY)
  uint16_t x_player_;  // Player spawn X (0-4088 range, ZScream: PlayerX)
  uint16_t y_camera_;  // Camera Y position
  uint16_t x_camera_;  // Camera X position
  uint8_t scroll_mod_y_;
  uint8_t scroll_mod_x_;
  uint16_t door_type_1_;
  uint16_t door_type_2_;
  uint16_t room_id_;   // ZScream: RoomID
  uint16_t map_pos_;   // VRAM location (ZScream: VRAMLocation)
  bool is_hole_ = false;
  bool deleted_ = false;
  bool is_automatic_ = true;  // FIX: Default to true (matches ZScream ExitOW.cs:101)

  OverworldExit() = default;
  
  /**
   * @brief Constructor for loading exits from ROM
   * 
   * Matches ZScream ExitOW.cs constructor (lines 129-167)
   * 
   * CRITICAL: Does NOT modify map_id parameter (Bug 1 fix)
   * Uses temporary normalized_map_id for calculations only.
   */
  OverworldExit(uint16_t room_id, uint8_t map_id, uint16_t vram_location,
                uint16_t y_scroll, uint16_t x_scroll, uint16_t player_y,
                uint16_t player_x, uint16_t camera_y, uint16_t camera_x,
                uint8_t scroll_mod_y, uint8_t scroll_mod_x,
                uint16_t door_type_1, uint16_t door_type_2,
                bool deleted = false)
      : y_scroll_(y_scroll),
        x_scroll_(x_scroll),
        y_player_(player_y),
        x_player_(player_x),
        y_camera_(camera_y),
        x_camera_(camera_x),
        scroll_mod_y_(scroll_mod_y),
        scroll_mod_x_(scroll_mod_x),
        door_type_1_(door_type_1),
        door_type_2_(door_type_2),
        room_id_(room_id),
        map_pos_(vram_location),
        is_hole_(false),
        deleted_(deleted) {
    // Initialize base entity fields (ZScream: PlayerX/PlayerY → x_/y_)
    x_ = player_x;
    y_ = player_y;
    map_id_ = map_id;  // FIX Bug 1: Store original map_id WITHOUT modification
    entity_type_ = kExit;

    // Calculate game coordinates using normalized map_id (0-63 range)
    // ZScream: ExitOW.cs:159-163
    uint8_t normalized_map_id = map_id % 0x40;
    int mapX = normalized_map_id % 8;
    int mapY = normalized_map_id / 8;

    // FIX Bug 4: Calculate game coords ONCE using correct formula
    // ZScream: ExitOW.cs:162-163 (AreaX/AreaY)
    game_x_ = static_cast<int>((std::abs(x_ - (mapX * 512)) / 16));
    game_y_ = static_cast<int>((std::abs(y_ - (mapY * 512)) / 16));
    
    // Door position calculations (used by door editor, not for coordinates)
    // ZScream: ExitOW.cs:145-157
    // These update DoorXEditor/DoorYEditor, NOT AreaX/AreaY
  }

  /**
   * @brief Update exit properties when moved or map changes
   * @param map_id Parent map ID to update to
   * @param context Pointer to const Overworld for area size queries (can be nullptr for vanilla)
   * 
   * Recalculates:
   * - game_x_/game_y_ (map-local tile coords)
   * - x_scroll_/y_scroll_ (if is_automatic_ = true)
   * - x_camera_/y_camera_ (if is_automatic_ = true)
   * - map_pos_ (VRAM location)
   * 
   * ZScream equivalent: ExitOW.cs:206-318 (UpdateMapStuff)
   */
  void UpdateMapProperties(uint16_t map_id, const void* context) override;
};

absl::StatusOr<std::vector<OverworldExit>> LoadExits(Rom* rom);
absl::Status SaveExits(Rom* rom, const std::vector<OverworldExit>& exits);

}  // namespace yaze::zelda3

#endif  // YAZE_APP_ZELDA3_OVERWORLD_EXIT_H_
