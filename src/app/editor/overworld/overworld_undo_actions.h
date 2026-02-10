#ifndef YAZE_APP_EDITOR_OVERWORLD_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_OVERWORLD_UNDO_ACTIONS_H_

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace editor {

/**
 * @brief A single tile coordinate + old/new value pair for undo/redo.
 */
struct OverworldTileChange {
  int x = 0;
  int y = 0;
  int old_tile_id = 0;
  int new_tile_id = 0;
};

/**
 * @class OverworldTilePaintAction
 * @brief Undoable action for painting tiles on the overworld map.
 *
 * Captures a batch of tile changes (from a single paint stroke or
 * rectangle fill) with both old and new values so that Undo() and
 * Redo() are fully self-contained.
 *
 * Consecutive paint actions on the same map within kMergeWindowMs
 * are merged into a single undo step via CanMergeWith/MergeWith.
 */
class OverworldTilePaintAction : public UndoAction {
 public:
  /// Merge window: consecutive paints within this duration become one step.
  static constexpr int kMergeWindowMs = 500;

  /**
   * @param map_id       Overworld map index where painting occurred
   * @param world        World index (0=Light, 1=Dark, 2=Special)
   * @param tile_changes Vector of individual tile changes with old+new values
   * @param overworld    Non-owning pointer to the Overworld data layer
   * @param refresh_fn   Callback to refresh map visuals after undo/redo
   */
  OverworldTilePaintAction(
      int map_id, int world,
      std::vector<OverworldTileChange> tile_changes,
      zelda3::Overworld* overworld,
      std::function<void()> refresh_fn)
      : map_id_(map_id),
        world_(world),
        tile_changes_(std::move(tile_changes)),
        overworld_(overworld),
        refresh_fn_(std::move(refresh_fn)),
        timestamp_(std::chrono::steady_clock::now()) {}

  absl::Status Undo() override {
    if (!overworld_) {
      return absl::InternalError("Overworld pointer is null");
    }
    auto& world_tiles = overworld_->GetMapTiles(world_);
    for (const auto& change : tile_changes_) {
      world_tiles[change.x][change.y] = change.old_tile_id;
    }
    if (refresh_fn_) {
      refresh_fn_();
    }
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    if (!overworld_) {
      return absl::InternalError("Overworld pointer is null");
    }
    auto& world_tiles = overworld_->GetMapTiles(world_);
    for (const auto& change : tile_changes_) {
      world_tiles[change.x][change.y] = change.new_tile_id;
    }
    if (refresh_fn_) {
      refresh_fn_();
    }
    return absl::OkStatus();
  }

  std::string Description() const override {
    return absl::StrFormat("Paint %d tile%s on map %d",
                           tile_changes_.size(),
                           tile_changes_.size() == 1 ? "" : "s",
                           map_id_);
  }

  size_t MemoryUsage() const override {
    return sizeof(*this) +
           tile_changes_.size() * sizeof(OverworldTileChange);
  }

  bool CanMergeWith(const UndoAction& prev) const override {
    const auto* prev_paint =
        dynamic_cast<const OverworldTilePaintAction*>(&prev);
    if (!prev_paint) return false;
    if (prev_paint->map_id_ != map_id_) return false;
    if (prev_paint->world_ != world_) return false;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp_ - prev_paint->timestamp_);
    return elapsed.count() <= kMergeWindowMs;
  }

  void MergeWith(UndoAction& prev) override {
    auto& prev_paint = static_cast<OverworldTilePaintAction&>(prev);

    // Build a map of (x,y) -> index in our tile_changes_ for fast lookup
    // so we can keep the earliest old_tile_id for coordinates that appear
    // in both actions.
    std::unordered_map<int64_t, size_t> coord_index;
    for (size_t i = 0; i < tile_changes_.size(); ++i) {
      int64_t key = (static_cast<int64_t>(tile_changes_[i].x) << 32) |
                    static_cast<int64_t>(tile_changes_[i].y);
      coord_index[key] = i;
    }

    for (const auto& prev_change : prev_paint.tile_changes_) {
      int64_t key = (static_cast<int64_t>(prev_change.x) << 32) |
                    static_cast<int64_t>(prev_change.y);
      auto it = coord_index.find(key);
      if (it != coord_index.end()) {
        // Same coordinate exists in both: keep the older old_tile_id
        tile_changes_[it->second].old_tile_id = prev_change.old_tile_id;
      } else {
        // Coordinate only in prev: adopt it as-is
        tile_changes_.push_back(prev_change);
      }
    }

    // Keep the earlier timestamp so subsequent merges measure from
    // the start of the combined stroke.
    timestamp_ = prev_paint.timestamp_;
  }

  int map_id() const { return map_id_; }
  int world() const { return world_; }
  const std::vector<OverworldTileChange>& tile_changes() const {
    return tile_changes_;
  }

 private:
  int map_id_;
  int world_;
  std::vector<OverworldTileChange> tile_changes_;
  zelda3::Overworld* overworld_;       // non-owning
  std::function<void()> refresh_fn_;   // callback to refresh map visuals
  std::chrono::steady_clock::time_point timestamp_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_UNDO_ACTIONS_H_
