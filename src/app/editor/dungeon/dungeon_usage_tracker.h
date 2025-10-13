#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_USAGE_TRACKER_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_USAGE_TRACKER_H

#include "absl/container/flat_hash_map.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @brief Tracks and analyzes usage statistics for dungeon resources
 * 
 * This component manages blockset, spriteset, and palette usage statistics
 * across all dungeon rooms, providing insights for optimization.
 */
class DungeonUsageTracker {
 public:
  DungeonUsageTracker() = default;
  
  // Statistics calculation
  void CalculateUsageStats(const std::array<zelda3::Room, 0x128>& rooms);
  void DrawUsageStats();
  void DrawUsageGrid();
  void RenderSetUsage(const absl::flat_hash_map<uint16_t, int>& usage_map,
                      uint16_t& selected_set, int spriteset_offset = 0x00);
  
  // Data access
  const absl::flat_hash_map<uint16_t, int>& GetBlocksetUsage() const { return blockset_usage_; }
  const absl::flat_hash_map<uint16_t, int>& GetSpritesetUsage() const { return spriteset_usage_; }
  const absl::flat_hash_map<uint16_t, int>& GetPaletteUsage() const { return palette_usage_; }
  
  // Selection state
  uint16_t GetSelectedBlockset() const { return selected_blockset_; }
  uint16_t GetSelectedSpriteset() const { return selected_spriteset_; }
  uint16_t GetSelectedPalette() const { return selected_palette_; }
  
  void SetSelectedBlockset(uint16_t blockset) { selected_blockset_ = blockset; }
  void SetSelectedSpriteset(uint16_t spriteset) { selected_spriteset_ = spriteset; }
  void SetSelectedPalette(uint16_t palette) { selected_palette_ = palette; }
  
  // Clear data
  void ClearUsageStats();

 private:
  absl::flat_hash_map<uint16_t, int> spriteset_usage_;
  absl::flat_hash_map<uint16_t, int> blockset_usage_;
  absl::flat_hash_map<uint16_t, int> palette_usage_;

  uint16_t selected_blockset_ = 0xFFFF;  // 0xFFFF indicates no selection
  uint16_t selected_spriteset_ = 0xFFFF;
  uint16_t selected_palette_ = 0xFFFF;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_USAGE_TRACKER_H
