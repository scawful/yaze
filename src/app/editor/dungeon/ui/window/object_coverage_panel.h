#ifndef YAZE_APP_EDITOR_DUNGEON_UI_WINDOW_OBJECT_COVERAGE_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_UI_WINDOW_OBJECT_COVERAGE_PANEL_H_

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/dungeon/object_coverage_model.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "core/project.h"

namespace yaze {
namespace editor {

/**
 * @class ObjectCoveragePanel
 * @brief Checklist for proving each dungeon object draws like the game.
 *
 * Lists every object the draw-routine registry maps, which rooms place it,
 * and a verdict per object (see ObjectEvidenceState). "Next object to check"
 * picks the next unchecked object in release-focus order and opens a room
 * that uses it with the object selected, so a review session is: compare,
 * set the verdict, press Next.
 *
 * Verdicts are saved as JSON under the app data directory, one file per
 * project (or per ROM SHA-1 when no project is open), so vanilla and Oracle
 * results stay separate.
 */
class ObjectCoveragePanel : public WindowContent {
 public:
  // Opens `room_id` and selects the placed object at `object_index`, which
  // should have ID `object_id`.
  using NavigateCallback =
      std::function<void(int room_id, size_t object_index, int object_id)>;

  ObjectCoveragePanel() = default;

  std::string GetId() const override { return "dungeon.object_coverage"; }
  std::string GetDisplayName() const override { return "Object Coverage"; }
  std::string GetIcon() const override { return ICON_MD_FACT_CHECK; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 46; }
  std::string GetWorkflowGroup() const override { return "Editors"; }

  void Draw(bool* p_open) override;

  void SetProject(project::YazeProject* project);
  void SetRooms(DungeonRoomStore* rooms);
  void SetNavigateCallback(NavigateCallback callback) {
    navigate_ = std::move(callback);
  }
  // Selects `object_id` and remembers `room_id` as where it was seen, so a
  // verdict set next is recorded against that room.
  void FocusObject(int object_id, int room_id);
  // Rooms changed on disk or a different ROM was loaded.
  void MarkRoomsDirty() { index_dirty_ = true; }

 private:
  void RebuildIndex();
  void EnsureEvidenceLoaded();
  void SetVerdict(int object_id, ObjectEvidenceState state);
  void SaveEvidence();
  void GoToOccurrence(int object_id, const ObjectOccurrence& occurrence);
  void GoToNextObject();

  // Automatic check against game tilemap captures (see
  // scripts/agents/capture-game-room-tilemaps.py).
  void DrawAutomaticCheck();
  void SetCaptureDir(const std::string& dir);
  void ReloadManifest();
  void StartAutomaticCheck();
  void StepAutomaticCheck();
  void CheckRoomAgainstGame(int room_id);
  void ApplyAutomaticVerdicts(ObjectEvidenceState state);

  void DrawSummary();
  void DrawFilters();
  void DrawObjectTable(float height);
  void DrawDetails();

  bool PassesFilters(int object_id) const;
  std::string EvidenceContextName() const;

  project::YazeProject* project_ = nullptr;
  DungeonRoomStore* rooms_ = nullptr;
  NavigateCallback navigate_;

  // Rebuilt by RebuildIndex().
  ObjectUsageIndex usage_;
  std::vector<int> review_order_;
  std::string rom_sha1_;
  bool index_dirty_ = true;

  ObjectEvidenceStore evidence_;
  std::filesystem::path evidence_path_;
  std::string loaded_context_;
  std::string status_message_;
  bool status_is_error_ = false;

  std::optional<int> selected_object_;
  bool scroll_to_selected_ = false;
  // Last room opened for each object, recorded with its verdict.
  std::map<int, int> last_room_for_object_;
  char note_buffer_[512] = {};
  std::optional<int> note_buffer_object_;

  std::optional<GameCaptureManifest> manifest_;
  std::string manifest_error_;
  std::string manifest_dir_;
  ObjectAutoCheckResults auto_results_;
  std::vector<int> auto_queue_;
  size_t auto_next_ = 0;
  bool auto_running_ = false;
  std::vector<std::string> auto_room_errors_;
  bool only_auto_differences_ = false;

  char filter_text_[64] = {};
  int state_filter_ = -1;  // -1 = all, else ObjectEvidenceState value.
  bool only_placed_ = true;
  bool only_focus_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_UI_WINDOW_OBJECT_COVERAGE_PANEL_H_
