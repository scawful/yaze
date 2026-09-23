#ifndef YAZE_APP_EDITOR_OVERWORLD_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_OVERWORLD_UNDO_ACTIONS_H_

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "app/editor/overworld/maps/overworld_property_edit.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_item.h"

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
 * Consecutive paint actions in the same world within kMergeWindowMs
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
   * @param refresh_map_fn Optional callback for each changed map; supersedes
   *                       refresh_fn when provided
   */
  OverworldTilePaintAction(int map_id, int world,
                           std::vector<OverworldTileChange> tile_changes,
                           zelda3::Overworld* overworld,
                           std::function<void()> refresh_fn,
                           std::function<void(int)> refresh_map_fn = {},
                           bool allow_merge = true)
      : allow_merge_(allow_merge),
        map_id_(map_id),
        world_(world),
        tile_changes_(NormalizeChanges(std::move(tile_changes))),
        overworld_(overworld),
        refresh_fn_(std::move(refresh_fn)),
        refresh_map_fn_(std::move(refresh_map_fn)),
        timestamp_(std::chrono::steady_clock::now()) {}

  absl::Status Undo() override {
    if (!overworld_) {
      return absl::InternalError("Overworld pointer is null");
    }
    auto& world_tiles = overworld_->GetMapTiles(world_);
    for (const auto& change : tile_changes_) {
      world_tiles[change.x][change.y] = change.old_tile_id;
    }
    RefreshChangedMaps();
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
    RefreshChangedMaps();
    return absl::OkStatus();
  }

  std::string Description() const override {
    return absl::StrFormat("Paint %d tile%s on map %d", tile_changes_.size(),
                           tile_changes_.size() == 1 ? "" : "s", map_id_);
  }

  size_t MemoryUsage() const override {
    return sizeof(*this) + tile_changes_.size() * sizeof(OverworldTileChange);
  }

  bool CanMergeWith(const UndoAction& prev) const override {
    if (!allow_merge_)
      return false;
    const auto* prev_paint =
        dynamic_cast<const OverworldTilePaintAction*>(&prev);
    if (!prev_paint)
      return false;
    if (!prev_paint->allow_merge_)
      return false;
    if (prev_paint->overworld_ != overworld_)
      return false;
    if (prev_paint->world_ != world_)
      return false;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp_ - prev_paint->timestamp_);
    return elapsed.count() <= kMergeWindowMs;
  }

  void MergeWith(UndoAction& prev) override {
    auto& prev_paint = static_cast<OverworldTilePaintAction&>(prev);

    // Build a map of (x,y) -> index in our tile_changes_ for fast lookup
    // so we can keep the earliest old_tile_id for coordinates that appear
    // in both actions.
    std::unordered_map<uint64_t, size_t> coord_index;
    for (size_t i = 0; i < tile_changes_.size(); ++i) {
      const uint64_t key = CoordinateKey(tile_changes_[i]);
      coord_index[key] = i;
    }

    for (const auto& prev_change : prev_paint.tile_changes_) {
      const uint64_t key = CoordinateKey(prev_change);
      auto it = coord_index.find(key);
      if (it != coord_index.end()) {
        // Same coordinate exists in both: keep the older old_tile_id
        tile_changes_[it->second].old_tile_id = prev_change.old_tile_id;
      } else {
        // Coordinate only in prev: adopt it as-is
        coord_index[key] = tile_changes_.size();
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
  static uint64_t CoordinateKey(const OverworldTileChange& change) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(change.x)) << 32) |
           static_cast<uint32_t>(change.y);
  }

  static std::vector<OverworldTileChange> NormalizeChanges(
      std::vector<OverworldTileChange> changes) {
    std::unordered_map<uint64_t, size_t> coordinate_indices;
    std::vector<OverworldTileChange> normalized;
    normalized.reserve(changes.size());
    for (const auto& change : changes) {
      const auto [it, inserted] =
          coordinate_indices.emplace(CoordinateKey(change), normalized.size());
      if (inserted) {
        normalized.push_back(change);
      } else {
        // A drag can revisit a tile. Undo restores its value before the stroke,
        // while redo restores the final value from that stroke.
        normalized[it->second].new_tile_id = change.new_tile_id;
      }
    }
    return normalized;
  }

  void RefreshChangedMaps() const {
    if (!refresh_map_fn_) {
      if (refresh_fn_)
        refresh_fn_();
      return;
    }
    // Derive this set at replay time: MergeWith can add changes on other maps.
    std::array<bool, zelda3::kNumOverworldMaps> changed_maps{};
    for (const auto& change : tile_changes_) {
      const int map_id = world_ * 0x40 + change.x / 32 + (change.y / 32) * 8;
      if (map_id >= 0 && map_id < zelda3::kNumOverworldMaps)
        changed_maps[map_id] = true;
    }
    for (int map_id = 0; map_id < zelda3::kNumOverworldMaps; ++map_id) {
      if (changed_maps[map_id])
        refresh_map_fn_(map_id);
    }
  }

  bool allow_merge_;
  int map_id_;
  int world_;
  std::vector<OverworldTileChange> tile_changes_;
  zelda3::Overworld* overworld_;      // non-owning
  std::function<void()> refresh_fn_;  // callback to refresh map visuals
  std::function<void(int)> refresh_map_fn_;
  std::chrono::steady_clock::time_point timestamp_;
};

/**
 * @brief Snapshot of overworld item list + current item selection.
 *
 * Used to restore delete/duplicate/move item workflows in one undo step.
 */
struct OverworldItemsSnapshot {
  std::vector<zelda3::OverworldItem> items;
  std::optional<zelda3::OverworldItem> selected_item_identity;
};

/**
 * @class OverworldItemsEditAction
 * @brief Undoable action for overworld item mutations.
 *
 * Stores before/after snapshots and applies them through a restore callback.
 */
class OverworldItemsEditAction : public UndoAction {
 public:
  using RestoreFn = std::function<void(const OverworldItemsSnapshot&)>;

  OverworldItemsEditAction(OverworldItemsSnapshot before,
                           OverworldItemsSnapshot after, RestoreFn restore,
                           std::string description)
      : before_(std::move(before)),
        after_(std::move(after)),
        restore_(std::move(restore)),
        description_(std::move(description)) {}

  absl::Status Undo() override {
    if (!restore_) {
      return absl::InternalError(
          "OverworldItemsEditAction: no restore callback");
    }
    restore_(before_);
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    if (!restore_) {
      return absl::InternalError(
          "OverworldItemsEditAction: no restore callback");
    }
    restore_(after_);
    return absl::OkStatus();
  }

  std::string Description() const override { return description_; }

  size_t MemoryUsage() const override {
    const size_t before_size =
        before_.items.size() * sizeof(zelda3::OverworldItem);
    const size_t after_size =
        after_.items.size() * sizeof(zelda3::OverworldItem);
    return sizeof(*this) + before_size + after_size;
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override { return false; }

 private:
  OverworldItemsSnapshot before_;
  OverworldItemsSnapshot after_;
  RestoreFn restore_;
  std::string description_;
};

class OverworldMapPropertyEditAction : public UndoAction {
 public:
  using ApplyFn = std::function<absl::Status(const OverworldPropertyEdit&)>;

  OverworldMapPropertyEditAction(OverworldPropertyEdit before,
                                 OverworldPropertyEdit after, ApplyFn apply,
                                 std::string description)
      : before_(std::move(before)),
        after_(std::move(after)),
        apply_(std::move(apply)),
        description_(std::move(description)) {}

  absl::Status Undo() override {
    if (!apply_) {
      return absl::InternalError(
          "OverworldMapPropertyEditAction: no apply callback");
    }
    return apply_(before_);
  }

  absl::Status Redo() override {
    if (!apply_) {
      return absl::InternalError(
          "OverworldMapPropertyEditAction: no apply callback");
    }
    return apply_(after_);
  }

  std::string Description() const override { return description_; }

  size_t MemoryUsage() const override {
    return sizeof(*this) + before_.description.size() +
           after_.description.size() + description_.size();
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override { return false; }

 private:
  OverworldPropertyEdit before_;
  OverworldPropertyEdit after_;
  ApplyFn apply_;
  std::string description_;
};

class OverworldMapPropertyBatchEditAction : public UndoAction {
 public:
  using ApplyFn = std::function<absl::Status(const OverworldPropertyEdit&)>;

  OverworldMapPropertyBatchEditAction(std::vector<OverworldPropertyEdit> before,
                                      std::vector<OverworldPropertyEdit> after,
                                      ApplyFn apply, std::string description)
      : before_(std::move(before)),
        after_(std::move(after)),
        apply_(std::move(apply)),
        description_(std::move(description)) {}

  absl::Status Undo() override {
    if (!apply_) {
      return absl::InternalError(
          "OverworldMapPropertyBatchEditAction: no apply callback");
    }
    for (auto it = before_.rbegin(); it != before_.rend(); ++it) {
      RETURN_IF_ERROR(apply_(*it));
    }
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    if (!apply_) {
      return absl::InternalError(
          "OverworldMapPropertyBatchEditAction: no apply callback");
    }
    for (const auto& edit : after_) {
      RETURN_IF_ERROR(apply_(edit));
    }
    return absl::OkStatus();
  }

  std::string Description() const override { return description_; }

  size_t MemoryUsage() const override {
    size_t total = sizeof(*this) + description_.size();
    for (const auto& edit : before_) {
      total += sizeof(edit) + edit.description.size();
    }
    for (const auto& edit : after_) {
      total += sizeof(edit) + edit.description.size();
    }
    return total;
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override { return false; }

 private:
  std::vector<OverworldPropertyEdit> before_;
  std::vector<OverworldPropertyEdit> after_;
  ApplyFn apply_;
  std::string description_;
};

class OverworldProjectLabelEditAction : public UndoAction {
 public:
  using ApplyFn = std::function<absl::Status(const std::string&)>;

  OverworldProjectLabelEditAction(std::string before, std::string after,
                                  ApplyFn apply, std::string description)
      : before_(std::move(before)),
        after_(std::move(after)),
        apply_(std::move(apply)),
        description_(std::move(description)) {}

  absl::Status Undo() override {
    if (!apply_) {
      return absl::InternalError(
          "OverworldProjectLabelEditAction: no apply callback");
    }
    return apply_(before_);
  }

  absl::Status Redo() override {
    if (!apply_) {
      return absl::InternalError(
          "OverworldProjectLabelEditAction: no apply callback");
    }
    return apply_(after_);
  }

  std::string Description() const override { return description_; }

  size_t MemoryUsage() const override {
    return sizeof(*this) + before_.size() + after_.size() + description_.size();
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override { return false; }

 private:
  std::string before_;
  std::string after_;
  ApplyFn apply_;
  std::string description_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_UNDO_ACTIONS_H_
