#ifndef YAZE_CORE_ROM_SETTINGS_H
#define YAZE_CORE_ROM_SETTINGS_H

#include <optional>
#include <string>
#include <unordered_map>

namespace yaze::core {

namespace RomAddressKey {
constexpr char kExpandedMessageStart[] = "expanded_message_start";
constexpr char kExpandedMessageEnd[] = "expanded_message_end";
constexpr char kExpandedMusicHook[] = "expanded_music_hook";
constexpr char kExpandedMusicMain[] = "expanded_music_main";
constexpr char kExpandedMusicAux[] = "expanded_music_aux";
constexpr char kOverworldMessagesExpanded[] = "overworld_messages_expanded";
constexpr char kOverworldMapParentExpanded[] = "overworld_map_parent_expanded";
constexpr char kOverworldTransitionPosXExpanded[] =
    "overworld_transition_pos_x_expanded";
constexpr char kOverworldTransitionPosYExpanded[] =
    "overworld_transition_pos_y_expanded";
constexpr char kOverworldScreenChange1Expanded[] =
    "overworld_screen_change_1_expanded";
constexpr char kOverworldScreenChange2Expanded[] =
    "overworld_screen_change_2_expanded";
constexpr char kOverworldScreenChange3Expanded[] =
    "overworld_screen_change_3_expanded";
constexpr char kOverworldScreenChange4Expanded[] =
    "overworld_screen_change_4_expanded";
constexpr char kOverworldMap16Expanded[] = "overworld_map16_expanded";
constexpr char kOverworldMap32TrExpanded[] = "overworld_map32_tr_expanded";
constexpr char kOverworldMap32BlExpanded[] = "overworld_map32_bl_expanded";
constexpr char kOverworldMap32BrExpanded[] = "overworld_map32_br_expanded";
constexpr char kOverworldEntranceMapExpanded[] =
    "overworld_entrance_map_expanded";
constexpr char kOverworldEntrancePosExpanded[] =
    "overworld_entrance_pos_expanded";
constexpr char kOverworldEntranceIdExpanded[] =
    "overworld_entrance_id_expanded";
constexpr char kOverworldEntranceFlagExpanded[] =
    "overworld_entrance_flag_expanded";
constexpr char kOverworldExpandedPtrMarker[] = "overworld_ptr_marker_expanded";
constexpr char kOverworldExpandedPtrHigh[] = "overworld_ptr_high_expanded";
constexpr char kOverworldExpandedPtrLow[] = "overworld_ptr_low_expanded";
constexpr char kOverworldExpandedPtrMagic[] = "overworld_ptr_magic_expanded";
constexpr char kDungeonMapTile16Expanded[] = "dungeon_map_tile16_expanded";
}  // namespace RomAddressKey

struct RomAddressOverrides {
  std::unordered_map<std::string, uint32_t> addresses;

  bool empty() const { return addresses.empty(); }

  std::optional<uint32_t> GetAddress(const std::string& key) const {
    auto it = addresses.find(key);
    if (it == addresses.end()) {
      return std::nullopt;
    }
    return it->second;
  }
};

class RomSettings {
 public:
  static RomSettings& Get() {
    static RomSettings instance;
    return instance;
  }

  void SetAddressOverrides(const RomAddressOverrides& overrides) {
    overrides_ = overrides;
  }

  void ClearOverrides() { overrides_.addresses.clear(); }

  const RomAddressOverrides& address_overrides() const { return overrides_; }

  uint32_t GetAddressOr(const std::string& key, uint32_t default_value) const {
    auto value = overrides_.GetAddress(key);
    return value ? *value : default_value;
  }

 private:
  RomAddressOverrides overrides_;
};

}  // namespace yaze::core

#endif  // YAZE_CORE_ROM_SETTINGS_H
