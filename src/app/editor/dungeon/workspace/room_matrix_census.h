#ifndef YAZE_APP_EDITOR_DUNGEON_WORKSPACE_ROOM_MATRIX_CENSUS_H_
#define YAZE_APP_EDITOR_DUNGEON_WORKSPACE_ROOM_MATRIX_CENSUS_H_

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "absl/status/statusor.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room_census.h"

namespace yaze {
class Rom;
namespace core {
class HackManifest;
}

namespace editor {

/**
 * @brief "Census" overlay for the Room Matrix.
 *
 * Runs zelda3::ComputeRoomCensus (the model behind `z3ed dungeon-room-census`)
 * on a copy of the ROM in the background, then colors cells by owner, outlines
 * free (green) and reclaimable (yellow) rooms, draws the legend/filter chips
 * and the largest-free-block summary, and fills the cell tooltip.
 */
class RoomMatrixCensusOverlay {
 public:
  using ManifestProvider = std::function<const core::HackManifest*()>;

  RoomMatrixCensusOverlay();
  ~RoomMatrixCensusOverlay();

  void SetManifestProvider(ManifestProvider provider) {
    manifest_provider_ = std::move(provider);
  }

  bool enabled() const { return enabled_; }
  void SetEnabled(bool enabled) { enabled_ = enabled; }

  // Starts a computation when enabled and the ROM changed; polls a running
  // one. Call once per frame before drawing.
  void Update(Rom* rom);
  void Invalidate() { stale_ = true; }

  // Toggle, status line, summary, and wrapping legend/filter chips.
  void DrawControls(Rom* rom);

  const zelda3::RoomCensus* census() const {
    return enabled_ && census_.has_value() ? &*census_ : nullptr;
  }
  bool computing() const { return pending_.valid(); }

  // Cell styling. Only meaningful when census() is non-null.
  ImU32 FillColor(int room_id) const;
  // Outline color and whether one applies (free / reclaimable).
  std::optional<ImU32> StatusOutline(int room_id) const;
  bool InLargestFreeBlock(int room_id) const;
  static ImU32 OrphanMarkColor();
  static ImU32 BlockHaloColor(float alpha);
  bool IsOrphan(int room_id) const;
  // True when no chip is active or the room matches an active chip.
  bool MatchesChips(int room_id) const;
  bool HasActiveChips() const {
    return filter_free_ || filter_reclaimable_ || filter_orphan_ ||
           !filter_owners_.empty();
  }

  // Census block for the hovered cell's tooltip.
  void DrawTooltip(int room_id) const;

  // Test hooks.
  void SetFilterFree(bool on) { filter_free_ = on; }
  void SetFilterReclaimable(bool on) { filter_reclaimable_ = on; }
  void ToggleOwnerFilter(int owner_index);
  static ImVec4 OwnerColor(int owner_index, bool interior);

 private:
  void StartComputation(Rom* rom);
  // Places the next chip on this line or wraps; returns false when it wraps.
  static void WrapBeforeItem(float item_width, bool first);
  bool DrawChip(const char* id, const std::string& label, ImVec4 swatch,
                bool outline_only, bool active, bool first);

  ManifestProvider manifest_provider_;
  bool enabled_ = false;
  bool stale_ = true;
  const Rom* computed_for_ = nullptr;
  std::optional<zelda3::RoomCensus> census_;
  std::string error_;
  std::future<absl::StatusOr<zelda3::RoomCensus>> pending_;
  std::set<int> largest_block_;

  bool filter_free_ = false;
  bool filter_reclaimable_ = false;
  bool filter_orphan_ = false;
  std::set<int> filter_owners_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_WORKSPACE_ROOM_MATRIX_CENSUS_H_
