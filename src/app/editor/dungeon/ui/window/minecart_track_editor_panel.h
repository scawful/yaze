#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_MINECART_TRACK_EDITOR_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_MINECART_TRACK_EDITOR_PANEL_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/dungeon/minecart_track_source.h"
#include "app/editor/dungeon/ui/minecart/minecart_assembly.h"
#include "app/editor/dungeon/ui/minecart/minecart_collision_tool.h"
#include "app/editor/dungeon/ui/minecart/minecart_node_tool.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/track_collision_generator.h"

namespace yaze::editor {

class MinecartTrackEditorPanel : public WindowContent {
 public:
  using ProjectChangedCallback =
      std::function<absl::Status(const project::DungeonOverlaySettings&)>;
  using ProjectDraftChangedCallback = std::function<absl::Status()>;
  using ProjectSaveCallback = std::function<absl::Status()>;
  using CollisionBatchApplyCallback = std::function<absl::Status(
      const std::vector<zelda3::TrackCollisionResult>&,
      const zelda3::GeneratorOptions&)>;

  MinecartTrackEditorPanel() = default;

  // WindowContent overrides
  std::string GetId() const override { return "dungeon.minecart_tracks"; }
  std::string GetWorkflowGroup() const override { return "Editors"; }
  std::string GetDisplayName() const override { return "Minecart Tracks"; }
  std::string GetIcon() const override { return ICON_MD_TRAIN; }
  std::string GetEditorCategory() const override { return "Dungeon"; }

  void Draw(bool* p_open) override;

  // Custom methods
  void SetRooms(DungeonRoomStore* rooms) {
    if (rooms_ != rooms) {
      CancelAssemblyPreview();
      assembly_copy_.reset();
      collision_preview_.clear();
      collision_preview_options_ = {};
    }
    rooms_ = rooms;
    audit_dirty_ = true;
    audit_includes_all_rooms_ = false;
  }
  absl::Status SetProject(project::YazeProject* project);
  // Reapply project-backed panel state even when the stable session project
  // pointer and descriptor path did not change.
  absl::Status RebindProjectContext(project::YazeProject* project);
  absl::Status SaveTracks();
  absl::Status ReloadTracks();
  absl::Status DiscardUnpublishedChanges();
  bool HasUnpublishedChanges() const;
  absl::StatusOr<std::filesystem::path> ResolveTrackSourcePath() const;
  absl::Status UpdateTrack(size_t track_index, const MinecartTrack& track);
  bool HasPendingProjectDraftChanges() const;
  absl::Status PrepareProjectSave();

  using TrackEditCallback = std::function<absl::Status(
      const MinecartTrack&, const MinecartTrack&, bool)>;
  void SetTrackEditCallback(TrackEditCallback callback) {
    track_edit_callback_ = std::move(callback);
  }
  absl::Status MoveCartStart(const MinecartTrack& before,
                             const MinecartTrack& after);
  // Undo replay only; validates source generation and expected record first.
  absl::Status RestoreTrackDraft(uint64_t generation,
                                 const MinecartTrack& expected,
                                 const MinecartTrack& replacement);
  absl::Status RestoreTrackDraftBatch(
      uint64_t generation, const std::vector<MinecartTrack>& expected,
      const std::vector<MinecartTrack>& replacement);

  using AssemblyPreview = std::function<absl::StatusOr<MinecartAssemblyPlan>(
      int, const DungeonSelectionClipboard&, int, int,
      const MinecartAssemblyMapping&)>;
  using AssemblyApply =
      std::function<absl::Status(const MinecartAssemblyPlan&)>;
  using AssemblyCapture =
      std::function<absl::StatusOr<DungeonSelectionClipboard>(int)>;
  void SetAssemblyCallbacks(AssemblyPreview preview, AssemblyApply apply,
                            AssemblyCapture capture) {
    preview_assembly_ = std::move(preview);
    apply_assembly_ = std::move(apply);
    capture_assembly_ = std::move(capture);
  }
  void BeginRailPath(bool pit);
  void CancelAssemblyPreview();
  void UpdateAssemblyPreview(int room, int x, int y, bool click);
  bool assembly_tool_active() const {
    return drawing_path_ || placing_assembly_;
  }
  const std::optional<MinecartAssemblyPlan>& assembly_preview() const {
    return assembly_preview_;
  }
  const std::string& assembly_error() const { return assembly_error_; }
  uint64_t assembly_preview_revision() const {
    return assembly_preview_revision_;
  }
  const std::vector<std::pair<int, int>>& path_waypoints() const {
    return path_waypoints_;
  }
  absl::Status ApplyAssemblyPreview();
  void DrawAssemblyTools();
  uint64_t track_generation() const { return track_generation_; }
  // Reload cancels an unfinished gesture even when Undo's source is unchanged.
  uint64_t canvas_revision() const { return canvas_revision_; }
  void MarkPairedTrackDraft() { paired_track_draft_ = HasUnpublishedChanges(); }
  bool HasPairedTrackDraft() const { return paired_track_draft_; }
  bool canvas_editing() const { return canvas_editing_; }
  void set_canvas_editing(bool enabled);
  int selected_track() const { return selected_track_; }
  void SelectTrack(int id) { selected_track_ = id; }
  void SetActiveRoom(int room);
  int active_room() const { return active_room_; }
  void CancelPick() { CancelCoordinatePicking(); }

  void SetRailPaletteCallback(std::function<bool()> draw) {
    draw_rail_palette_ = std::move(draw);
  }
  void SetCollisionStampCallback(MinecartCollisionApply apply) {
    apply_collision_stamp_ = std::move(apply);
  }
  const MinecartCollisionApply& collision_stamp_callback() const {
    return apply_collision_stamp_;
  }
  bool collision_stamping() const { return collision_stamping_; }
  void set_collision_stamping(bool enabled) {
    collision_stamping_ = enabled;
    if (enabled) {
      CancelNodePlacement();
      set_canvas_editing(false);
    }
  }
  uint8_t collision_stamp_value() const { return collision_stamp_value_; }
  int collision_stamp_size() const { return collision_stamp_large_ ? 2 : 1; }
  void DrawRailTools();
  void DrawNodeTools();
  using NodePlacementCallback =
      std::function<absl::Status(int, int, int, const MinecartNodeBrush&)>;
  using AvailableNodeIdCallback =
      std::function<absl::StatusOr<int>(MinecartNodeKind, int)>;
  void SetNodePlacementCallback(NodePlacementCallback callback) {
    place_node_ = std::move(callback);
  }
  const NodePlacementCallback& node_placement_callback() const {
    return place_node_;
  }
  void SetAvailableNodeIdCallback(AvailableNodeIdCallback callback) {
    available_node_id_ = std::move(callback);
  }
  absl::Status SelectAvailableNodeId(MinecartNodeKind kind);
  const std::optional<MinecartToolPayload>& placement_brush() const {
    return placement_brush_;
  }
  void SetPlacementBrush(const MinecartToolPayload& brush) {
    CancelNodePlacement();
    set_canvas_editing(false);
    collision_stamping_ = false;
    selected_placement_tool_ = PlacementToolSelection{
        brush.node.kind,      brush.node.id,   brush.node.collision,
        brush.collision_only, brush.collision, brush.size};
    BindPlacementBrushToRoom();
  }
  // Invalidate only an active payload owned by this panel. Repeated cleanup
  // cannot renew or repeatedly age a canceled gesture. A viewer filter avoids
  // canceling another editable canvas when a compare view becomes read only.
  void CancelNodeDrag(const void* source_viewer = nullptr) {
    if (!ImGui::GetCurrentContext())
      return;
    const auto* payload = ImGui::GetDragDropPayload();
    if (!payload || !payload->IsDataType(kMinecartNodeDragPayload) ||
        payload->DataSize != sizeof(MinecartToolPayload))
      return;
    const auto* token = static_cast<const MinecartToolPayload*>(payload->Data);
    if (token->owner == this && token->revision == canvas_revision_ &&
        (!source_viewer || token->source_viewer == source_viewer)) {
      ++canvas_revision_;
    }
  }
  void CancelNodePlacement() {
    CancelAssemblyPreview();
    CancelNodeDrag();
    selected_placement_tool_.reset();
    placement_brush_.reset();
  }
  void ReportToolStatus(const absl::Status& status) {
    status_message_ = status.ok() ? "" : std::string(status.message());
    show_success_ = status.ok();
  }
  bool automatic_rail_collision() const { return automatic_rail_collision_; }
  void set_automatic_rail_collision(bool enabled) {
    automatic_rail_collision_ = enabled;
  }

  // Advanced Pick uses the same paired move operation as marker dragging.
  bool IsPickingCoordinates() const { return picking_mode_; }
  int GetPickingTrackIndex() const { return picking_track_index_; }
  const std::vector<MinecartTrack>& GetTracks();

  // Callback to navigate to a specific room for coordinate picking
  using RoomNavigationCallback = std::function<void(int room_id)>;
  void SetRoomNavigationCallback(RoomNavigationCallback callback) {
    room_navigation_callback_ = std::move(callback);
  }
  void SetProjectChangedCallback(ProjectChangedCallback callback) {
    project_changed_callback_ = std::move(callback);
  }
  void SetProjectDraftChangedCallback(ProjectDraftChangedCallback callback) {
    project_draft_changed_callback_ = std::move(callback);
  }
  void SetProjectSaveCallback(ProjectSaveCallback callback) {
    project_save_callback_ = std::move(callback);
  }
  void SetCollisionBatchApplyCallback(CollisionBatchApplyCallback callback) {
    collision_batch_apply_callback_ = std::move(callback);
  }
  void InvalidateRoomAudit() {
    ClearCollisionPreview();
    audit_dirty_ = true;
    audit_includes_all_rooms_ = false;
  }
  void DetachRuntimeContext() {
    CancelAssemblyPreview();
    assembly_copy_.reset();
    preview_assembly_ = {};
    apply_assembly_ = {};
    capture_assembly_ = {};
    room_navigation_callback_ = {};
    project_changed_callback_ = {};
    project_draft_changed_callback_ = {};
    project_save_callback_ = {};
    collision_batch_apply_callback_ = {};
    track_edit_callback_ = {};
    draw_rail_palette_ = {};
    apply_collision_stamp_ = {};
    place_node_ = {};
    available_node_id_ = {};
    selected_placement_tool_.reset();
    placement_brush_.reset();
    collision_stamping_ = false;
    canvas_editing_ = false;
    rooms_ = nullptr;
    project_ = nullptr;
    picking_mode_ = false;
    picking_track_index_ = -1;
    collision_preview_.clear();
  }

 private:
  AssemblyPreview preview_assembly_;
  AssemblyApply apply_assembly_;
  AssemblyCapture capture_assembly_;
  bool drawing_path_ = false, placing_assembly_ = false, path_pit_ = false;
  int assembly_room_ = -1, preview_x_ = -1, preview_y_ = -1;
  int assembly_hover_x_ = -1, assembly_hover_y_ = -1;
  uint64_t assembly_revision_ = 0;
  uint64_t assembly_preview_revision_ = 0;
  std::vector<std::pair<int, int>> path_waypoints_;
  std::optional<DungeonSelectionClipboard> assembly_copy_;
  MinecartAssemblyMapping assembly_mapping_;
  std::optional<MinecartAssemblyPlan> assembly_preview_;
  std::string assembly_error_;
  friend class MinecartTrackEditorPanelTestPeer;

  absl::Status LoadTracks();
  absl::Status ValidateLoadedManifestIdentity() const;
  absl::Status RefreshProjectBinding();
  void ResetTrackSession();
  void StartCoordinatePicking(int track_index);
  void CancelCoordinatePicking();
  void RebuildAuditCache(bool include_unmaterialized = false);
  void DrawRouteRooms();
  std::vector<int> AuditedRoomIds() const;
  absl::Status BuildCollisionPreview(const std::vector<int>& room_ids);
  absl::Status BuildAllEligibleCollisionPreview();
  absl::Status ApplyCollisionPreview();
  void ClearCollisionPreview();
  absl::StatusOr<zelda3::GeneratorOptions> ResolveGeneratorOptions() const;
  bool IsDefaultTrack(const MinecartTrack& track) const;
  void DrawOverlaySettings();
  void InitializeOverlayInputs();
  void ClearOverlayInputs();
  void BindPlacementBrushToRoom();
  void RefreshPlacementToolIdentity(MinecartNodeKind kind);

  using OverlayListMember =
      std::vector<uint16_t> project::DungeonOverlaySettings::*;
  bool UpdateOverlayList(const char* label, std::string& input,
                         OverlayListMember member);
  absl::StatusOr<bool> CommitOverlayList(std::string& input,
                                         OverlayListMember member);
  absl::StatusOr<bool> CommitOverlayInputsForSave();
  absl::StatusOr<bool> ResetOverlaySettings();
  absl::Status NotifyProjectChanged(
      const project::DungeonOverlaySettings& overlay);
  absl::Status NotifyProjectDraftChanged();
  absl::Status SaveProjectSettings();

  struct RoomTrackAudit {
    bool has_any_custom_collision = false;
    bool has_track_collision = false;
    bool has_stop_tiles = false;
    bool has_minecart_sprite = false;
    bool has_minecart_on_stop = false;
    std::vector<int> track_subtypes;
    std::vector<int> route_slots;
  };

  std::function<bool()> draw_rail_palette_;
  MinecartCollisionApply apply_collision_stamp_;
  bool collision_stamping_ = false;
  bool automatic_rail_collision_ = true;
  // Tool intent survives room navigation. Source anchors and collision CAS
  // snapshots belong only to the fresh per-room gesture payload below.
  struct PlacementToolSelection {
    MinecartNodeKind kind;
    int id;
    uint8_t node_collision;
    bool collision_only;
    uint8_t collision;
    int size;
  };
  std::optional<PlacementToolSelection> selected_placement_tool_;
  std::optional<MinecartToolPayload> placement_brush_;
  NodePlacementCallback place_node_;
  AvailableNodeIdCallback available_node_id_;
  int node_id_ = 0;
  int node_channel_ = 0;
  int node_departure_ = 0;
  int node_switch_orientation_ = 0;
  bool collision_stamp_large_ = false;
  uint8_t collision_stamp_value_ = 0xB1;
  TrackEditCallback track_edit_callback_;
  uint64_t track_generation_ = 0;
  uint64_t canvas_revision_ = 0;
  int selected_track_ = -1;
  int active_room_ = -1;
  bool canvas_editing_ = false;
  bool paired_track_draft_ = false;
  std::vector<MinecartTrack> tracks_;
  std::vector<MinecartTrack> loaded_tracks_;
  std::optional<MinecartTrackSourceDocument> source_document_;
  std::optional<core::MinecartTrackLayout::Source> loaded_source_identity_;
  std::filesystem::path loaded_source_path_;
  std::string loaded_source_sha256_;
  std::string bound_project_filepath_;
  std::optional<core::MinecartTrackLayout::Source> bound_source_identity_;
  DungeonRoomStore* rooms_ = nullptr;
  project::YazeProject* project_ = nullptr;
  std::unordered_map<int, RoomTrackAudit> room_audit_;
  std::unordered_map<int, std::vector<int>> route_usage_rooms_;
  std::vector<bool> route_slot_used_;
  std::vector<zelda3::TrackCollisionResult> collision_preview_;
  zelda3::GeneratorOptions collision_preview_options_;
  bool audit_dirty_ = true;
  bool audit_includes_all_rooms_ = false;
  bool load_attempted_ = false;
  bool loaded_ = false;
  std::string status_message_;
  bool show_success_ = false;
  float success_timer_ = 0.0f;

  // Coordinate picking state
  bool picking_mode_ = false;
  int picking_track_index_ = -1;

  RoomNavigationCallback room_navigation_callback_;
  ProjectChangedCallback project_changed_callback_;
  ProjectDraftChangedCallback project_draft_changed_callback_;
  ProjectSaveCallback project_save_callback_;
  CollisionBatchApplyCallback collision_batch_apply_callback_;

  // Overlay config input state
  bool overlay_inputs_initialized_ = false;
  std::optional<project::DungeonOverlaySettings> overlay_inputs_model_;
  std::string overlay_track_tiles_input_;
  std::string overlay_track_stop_tiles_input_;
  std::string overlay_track_switch_tiles_input_;
  std::string overlay_track_object_ids_input_;
  std::string overlay_minecart_sprite_ids_input_;
};

}  // namespace yaze::editor

#endif
