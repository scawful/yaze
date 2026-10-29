#ifndef YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H
#define YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/emu/render/emulator_render_service.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/widgets/dungeon_object_emulator_preview.h"
#include "app/gui/widgets/palette_editor_widget.h"
#include "dungeon_canvas_viewer.h"
#include "dungeon_entrance_camera.h"
#include "dungeon_render_context.h"
#include "dungeon_room_loader.h"
#include "dungeon_room_selector.h"
#include "dungeon_room_store.h"
#include "dungeon_undo_actions.h"
#include "imgui/imgui.h"
#include "inspectors/door_editor_content.h"
#include "rom/rom.h"
#include "selectors/object_selector_content.h"
#include "util/lru_cache.h"
#include "util/stop_token.h"
#include "workspace/room_graphics_content.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_census.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/track_collision_generator.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

class CustomCollisionPanel;
class DungeonEntrancesPanel;
struct GraphicsEditContext;
class DungeonEditorPaletteRefreshTestPeer;
class DungeonEditorV2MinecartTrackTestPeer;
class DungeonEditorV2MinecartNodeTestPeer;
struct MinecartNodeBrush;
enum class MinecartNodeKind;
class DungeonEditorV2ObjectTileEditorTestPeer;
class DungeonEditorV2EntranceCameraTestPeer;
class DungeonEditorEntrancePropertiesTestPeer;
class DungeonEditorV2RegularEntranceTestPeer;
class DungeonEditorV2ReloadTestPeer;
class DungeonEditorV2ShortcutTestPeer;
class DungeonEditorV2SpawnPointTestPeer;
class DungeonEditorV2SpawnRejectionTestPeer;
class MinecartTrackEditorPanel;
struct MinecartCollisionStamp;
struct MinecartTrack;
class ObjectCoveragePanel;
class ObjectTileEditorPanel;
class OverlayManagerPanel;
class PaletteEditorContent;
class RoomTagEditorPanel;
class RoomMatrixContent;
class WaterFillPanel;

enum class DungeonRoomNavigationDirection {
  kUp,
  kDown,
  kLeft,
  kRight,
};

/**
 * @brief DungeonEditorV2 - Simplified dungeon editor using component delegation
 *
 * This is a drop-in replacement for DungeonEditor that properly delegates
 * to the component system instead of implementing everything inline.
 *
 * Architecture:
 * - DungeonRoomLoader handles ROM data loading
 * - DungeonRoomSelector handles room selection UI
 * - DungeonCanvasViewer handles canvas rendering and display
 * - DungeonObjectSelector handles object selection and preview
 * - InteractionCoordinator manages entity (door/sprite/item) interactions
 *
 * The editor acts as a coordinator, not an implementer.
 *
 * ## Ownership Model
 *
 * OWNED by DungeonEditorV2 (use unique_ptr or direct member):
 * - rooms_ (std::array) - full ownership
 * - entrances_ (std::array) - full ownership
 * - spawn_points_ (std::array) - full ownership
 * - room_viewers_ (map of unique_ptr) - owns canvas viewers per room
 * - dungeon_editor_system_ (unique_ptr) - owns editor system
 * - render_service_ (unique_ptr) - owns emulator render service
 * - room_loader_, room_selector_, palette_editor_ - direct members
 *
 * EXTERNALLY OWNED (raw pointers, lifetime managed elsewhere):
 * - rom_ - owned by Application, passed via SetRom()
 * - game_data_ - owned by Application, passed via SetGameData()
 * - renderer_ - owned by Application, passed via Initialize()
 *
 * OWNED BY WorkspaceWindowManager (registered EditorPanels):
 * - object_editor_panel_ - registered via RegisterWindowContent()
 * - room_graphics_panel_ - registered via RegisterWindowContent()
 * - sprite_editor_panel_ - registered via RegisterWindowContent()
 * - item_editor_panel_ - registered via RegisterWindowContent()
 *
 * Panel pointers are stored for convenience access but should NOT be
 * deleted by this class. WorkspaceWindowManager owns them.
 */
class DungeonEditorV2 : public Editor {
 public:
  explicit DungeonEditorV2(Rom* rom = nullptr);

  ~DungeonEditorV2() override;
  // Visit loaded models using the dungeon canvas's entrance resolver.
  void VisitGraphicsUsageRooms(
      const std::function<void(int, zelda3::Room&, const std::string&, bool,
                               int)>& visitor);
  void PrepareForSessionTeardown() override;

  void SetGameData(zelda3::GameData* game_data) override {
    game_data_ = game_data;
    dependencies_.game_data = game_data;  // Also set base class dependency
    room_loader_.SetGameData(game_data);
    if (object_selector_panel_) {
      object_selector_panel_->SetGameData(game_data);
    }
    if (dungeon_editor_system_) {
      dungeon_editor_system_->SetGameData(game_data);
    }
    rooms_.SetGameData(game_data);
    // Note: Canvas viewer game data is set lazily in GetViewerForRoom
    // but we should update existing viewers
    room_viewers_.ForEach(
        [game_data](int, std::unique_ptr<DungeonCanvasViewer>& viewer) {
          if (viewer)
            viewer->SetGameData(game_data);
        });
    if (workbench_viewer_) {
      workbench_viewer_->SetGameData(game_data);
    }
    if (workbench_compare_viewer_) {
      workbench_compare_viewer_->SetGameData(game_data);
    }
  }

  // Editor interface
  void SetDependencies(const EditorDependencies& deps) override;
  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Cut() override;
  absl::Status Copy() override;
  absl::Status Paste() override;
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  // Undo() first finalizes pending gestures into history, so any pending
  // snapshot makes Undo available even while undo_manager_ is still empty.
  bool CanUndo() const override {
    return undo_manager_.CanUndo() || HasPendingUndoSnapshot();
  }
  absl::Status Save() override;
  // Reject excluded model edits before any writer runs. Apply Room narrows
  // room-local streams; shared tables still include all pending room records.
  // Dirty fixed headers in scope must not share storage with another room.
  absl::Status ValidateSaveScope(int only_room = -1,
                                 bool room_data_only = false) const;
  absl::Status BeginSaveTransaction() override;
  void RollbackSaveTransaction() override;
  void CommitSaveTransaction() override;
  void ContributeStatus(StatusBar* status_bar) override;
  EditorContextSnapshot BuildContextSnapshot() const override;
  absl::Status SaveRoom(int room_id);
  // Discrete room property edits share the dungeon undo history in both views.
  absl::StatusOr<DungeonConnectionPlan> PreviewDoorConnection(
      const DungeonConnectionRequest& request);
  absl::Status ApplyDoorConnection(const DungeonConnectionPlan& plan);
  absl::StatusOr<std::string> ExportRoomDocument(int room_id);
  absl::StatusOr<DungeonRoomTransferPlan> PreviewRoomTransfer(
      int target_room_id, int source_room_id, const std::string& json,
      const DungeonRoomTransferOptions& options);
  absl::Status ApplyRoomTransfer(const DungeonRoomTransferPlan& plan);
  absl::Status EditRoomMetadata(int room_id, const RoomMetadataEdit& edit);
  // Shared roomset assignment (background slots 3-6); zero inherits main.
  absl::Status EditRoomGraphicsSheet(int room_id, int slot, int sheet_id);
  // The owning EditorSet supplies an existing model only; usage queries must
  // not create another editor or read another ROM session's window context.
  void SetRoomGraphicsOverworldProvider(
      std::function<const zelda3::Overworld*()> provider) {
    room_graphics_overworld_provider_ = std::move(provider);
  }
  absl::StatusOr<zelda3::RoomsetUsageReport> GetRoomGraphicsUsage(
      int roomset) const;
  absl::Status EditRoomMetadataBatch(
      const std::vector<RoomMetadataRequest>& requests);
  absl::Status EditChest(int room_id, size_t index, uint8_t item_id,
                         bool big_chest);
  absl::Status DeleteChest(int room_id, size_t index);
  int LoadedRoomCount() const;
  // Room-specific pending state used by room counts and room-level UI.
  int PendingRoomCount() const;
  bool HasPendingRoomChanges() const;
  // All dungeon save domains plus unapplied editor-local work.
  bool HasPendingDungeonChanges() const;
  // Track source must be published separately from the ROM save domains.
  bool HasUnpublishedMinecartTrackChanges() const;
  bool HasPendingProjectDraftChanges() const;
  absl::Status PrepareProjectSave();
  // Rename (label) or reset (nullopt) a room's name in the active project.
  absl::Status ApplyRoomLabelEdit(int room_id,
                                  std::optional<std::string> label);
  bool CurrentRoomHasPendingChanges() const;
  int TotalRoomCount() const { return static_cast<int>(rooms_.size()); }

  // Collect PC write ranges for all dirty/loaded rooms (for write conflict
  // analysis). Returns pairs of (start_pc, end_pc) covering header and object
  // regions that would be written during save.
  std::vector<std::pair<uint32_t, uint32_t>> CollectWriteRanges() const;

  // ROM management
  void SetRom(Rom* rom) {
    const bool rom_changed = rom_ != rom;
    rom_ = rom;
    room_loader_ = DungeonRoomLoader(rom);
    room_loader_.SetGameData(game_data_);
    room_selector_.SetRom(rom);

    // Propagate ROM to all rooms
    rooms_.SetRom(rom);
    // Reset viewers and copied room data on ROM change.
    if (rom_changed) {
      selection_clipboard_ = {};
      automatic_entrance_preview_ = true;
      current_entrance_id_ = zelda3::kNumDungeonSpawnPoints;
      room_selector_.set_current_entrance_id(current_entrance_id_);
      rooms_.Clear();
      room_viewers_.Clear();
      workbench_viewer_.reset();
      workbench_compare_viewer_.reset();
      InvalidateDungeonRenderContextCensus();
    }
  }
  // Reloads same-address ROM state in place so rooms, viewers, and workspace
  // panel bindings retain stable addresses.
  absl::Status RefreshRomBackedState();
  Rom* rom() const { return rom_; }

  // Room management
  void add_room(int room_id);
  // Opens the room of regular entrance `entrance_id` (0x00-0x84, the overworld
  // entrance numbering) with that entrance's render context.
  void OpenRegularEntrance(int entrance_id);
  void FocusRoom(int room_id);
  absl::Status LoadProposalForReview(const std::string& path);
  std::string proposal_review_path() const;

  // Agent/Automation controls
  void SelectObject(int obj_id);
  void SetAgentMode(bool enabled);
  bool CanHandleDungeonNavigationShortcut() const;
  bool NavigateToAdjacentRoom(DungeonRoomNavigationDirection direction);
  bool CycleRoomSelection(int direction);
  // Select the next or previous room object in the active editable canvas.
  bool CycleObjectSelection(int direction);

  // ROM state
  bool IsRomLoaded() const override { return rom_ && rom_->is_loaded(); }
  std::string GetRomStatus() const override {
    if (!rom_)
      return "No ROM loaded";
    if (!rom_->is_loaded())
      return "ROM failed to load";
    return absl::StrFormat("ROM loaded: %s", rom_->title());
  }

  // Open a workspace window by its id using WorkspaceWindowManager.
  void OpenWindow(const std::string& window_id) {
    if (dependencies_.window_manager) {
      dependencies_.window_manager->OpenWindow(window_id);
    }
  }

  // Create and register the project-only Minecart Tracks surface on demand.
  // This supports enabling custom objects after the Dungeon editor has loaded.
  absl::Status EnsureMinecartTrackEditorPanel();

  // Explicit workflow toggle between integrated Workbench and standalone panels.
  void SetWorkbenchWorkflowMode(bool enabled, bool show_toast = true);
  // Queue a workflow mode change to run at a safe point in the next update.
  void QueueWorkbenchWorkflowMode(bool enabled, bool show_toast = true);
  // Queue a mode flip (Workbench <-> Standalone) for next update.
  void ToggleWorkbenchWorkflowMode(bool show_toast = true);
  bool IsWorkbenchWorkflowEnabled() const;

  // ShortcutManager runs before this editor draws. Queue destructive keyboard
  // actions so the room canvas can establish ownership for the current frame
  // before the action is evaluated.
  void QueueRoomCanvasDeleteShortcut();

  // Panel card IDs for programmatic access
  static constexpr const char* kRoomSelectorId = "dungeon.room_selector";
  static constexpr const char* kEntranceListId = "dungeon.entrance_list";
  static constexpr const char* kRoomMatrixId = "dungeon.room_matrix";
  static constexpr const char* kRoomGraphicsId = "dungeon.room_graphics";
  static constexpr const char* kObjectSelectorId = "dungeon.object_selector";
  static constexpr const char* kObjectToolsId = kObjectSelectorId;
  static constexpr const char* kDoorEditorId = "dungeon.door_editor";
  static constexpr const char* kPaletteEditorId = "dungeon.palette_editor";
  static constexpr const char* kMinecartTrackEditorId =
      "dungeon.minecart_tracks";

  // Public accessors for WASM API and automation
  int current_room_id() const { return room_selector_.current_room_id(); }
  int* mutable_current_room_id() { return &current_room_id_; }
  const ImVector<int>& active_rooms() const {
    return room_selector_.active_rooms();
  }
  DungeonRoomStore& rooms() { return rooms_; }
  const DungeonRoomStore& rooms() const { return rooms_; }
  gfx::IRenderer* renderer() const { return renderer_; }
  ObjectSelectorContent* object_selector_panel() const {
    return object_selector_panel_;
  }
  ObjectSelectorContent* object_editor_panel() const {
    return object_selector_panel_;
  }
  DoorEditorContent* door_editor_panel() const { return door_editor_panel_; }

  /**
   * @brief Get the list of recently visited room IDs
   * @return Deque of room IDs, most recent first (max 10 entries)
   */
  const std::deque<int>& GetRecentRooms() const { return recent_rooms_; }

 private:
  friend class DungeonCanvasViewerTestPeer;
  friend class DungeonEditorPaletteRefreshTestPeer;
  friend class DungeonEditorEntityUndoTestPeer;
  friend class DungeonRoomEditsTestPeer;
  friend class DungeonSelectionEditsTestPeer;
  friend class DungeonPotPlacementTestPeer;
  friend class DungeonConnectionEditsTestPeer;
  friend class DungeonRoomTransferTestPeer;
  friend class DungeonEditorV2MinecartTrackTestPeer;
  friend class DungeonEditorV2MinecartNodeTestPeer;
  friend class DungeonEditorV2ObjectTileEditorTestPeer;
  friend class DungeonEditorV2EntranceCameraTestPeer;
  friend class DungeonEditorEntrancePropertiesTestPeer;
  friend class DungeonEditorV2RegularEntranceTestPeer;
  friend class DungeonEditorV2ReloadTestPeer;
  friend class AuthoringSaveWorkflowTestPeer;
  friend class DungeonPaletteSourceTestPeer;
  friend class DungeonEditorV2ShortcutTestPeer;
  friend class DungeonEditorV2SpawnPointTestPeer;
  friend class DungeonEditorV2SpawnRejectionTestPeer;
  friend class DungeonEditorV2RomSafetyTest_UndoSnapshotLeakDetection_Test;
  friend class
      DungeonEditorV2RomSafetyTest_ObjectStreamUndoRedoRestoresSelectionIdentity_Test;
  friend class DungeonEditorV2RomSafetyTest_ViewerCacheLRUEviction_Test;
  friend class
      DungeonEditorV2RomSafetyTest_ViewerCacheNeverEvictsActiveRooms_Test;
  friend class
      DungeonEditorV2RomSafetyTest_ViewerCacheLRUAccessOrderUpdate_Test;
  friend class
      DungeonEditorV2RomSafetyTest_SaveAllRoomsRollsBackEarlierWritesOnLateFailure_Test;
  friend class
      DungeonEditorV2RomSafetyTest_LateCoordinatorRollbackRestoresEntranceDirtyState_Test;
  friend class
      DungeonEditorPaletteRefreshTest_SharedHudEditRefreshesRoomUsingDifferentDungeonPalette_Test;
  friend class
      DungeonEditorPaletteRefreshTest_DungeonMainEditRefreshesResolvedAliasesOnly_Test;
  friend class
      DungeonEditorPaletteRefreshTest_CachedRoomRefreshesThroughViewerCompositePreparation_Test;
  friend class
      DungeonEditorPaletteRefreshTest_CompareViewerScopesEntranceContextToRequestedRoom_Test;

  gfx::IRenderer* renderer_ = nullptr;

  // Draw the Room Panels
  void DrawRoomPanels();
  void DrawRoomTab(int room_id);

  // Texture processing (critical for rendering)
  void ProcessDeferredTextures();
  void ReloadWaterFillZones();

  // Room selection callback
  void OnRoomSelected(int room_id, bool request_focus = true);
  void OnRoomSelected(int room_id, RoomSelectionIntent intent);
  // Opens `room_id` and selects its placed object `object_index`. If the room
  // changed since the index was built, falls back to the first object with
  // `object_id`.
  void NavigateToPlacedObject(int room_id, size_t object_index, int object_id);
  // Object Coverage wiring, in dungeon_editor_v2_object_coverage.cc.
  std::unique_ptr<ObjectCoveragePanel> CreateObjectCoveragePanel();
  void FocusObjectCoverage(int room_id, const zelda3::RoomObject& object);
  void OnEntranceSelected(int entrance_id);
  void SetGraphicsPreviewEntrance(int entrance_id);
  int ResolveEntranceRoomId(int entrance_id) const;

  // Sync all sub-panels to the current room configuration
  void SyncPanelsToRoom(int room_id);
  void HandleDungeonPaletteChanged(gui::DungeonPaletteChange change);
  void HandleArenaPaletteChanged(const std::string& group_name,
                                 int palette_index);
  void RegisterPaletteListener();
  void InvalidateDungeonPaletteUsers(gui::DungeonPaletteChange change);
  DungeonRenderContext ResolveDungeonRenderContextForRoom(int room_id) const;
  uint8_t ResolveSelectedEntranceBlocksetForRoom(int room_id) const;
  std::vector<DungeonRenderEntranceCandidate>
  BuildDungeonRenderEntranceCandidates() const;
  const zelda3::RoomCensus* GetDungeonRenderContextCensus() const;
  void InvalidateDungeonRenderContextCensus();
  void ApplyEntranceRenderContext(int room_id);
  void ConfigureViewerRenderContext(DungeonCanvasViewer* viewer, int room_id);
  std::optional<DungeonEntranceCameraState> GetEntranceCameraState(
      int slot_index) const;
  absl::Status RepairEntranceCamera(int slot_index);
  absl::Status RestoreEntranceCameraState(
      int slot_index, const DungeonEntranceCameraState& state);
  void RefreshEntranceCameraOverlays();
  void ConfigureEntrancePropertyEditCallbacks(DungeonEntrancesPanel* panel);
  void RecordEntrancePropertyEdit(int slot_index,
                                  const zelda3::RoomEntrance& before,
                                  const zelda3::RoomEntrance& after,
                                  std::shared_ptr<const int> gesture);
  void RecordSpawnPropertyEdit(int slot_index,
                               const zelda3::DungeonSpawnPoint& before,
                               const zelda3::DungeonSpawnPoint& after,
                               std::shared_ptr<const int> gesture);
  absl::Status RestoreEntranceProperties(int slot_index,
                                         const zelda3::RoomEntrance& entrance);
  absl::Status RestoreSpawnProperties(int slot_index,
                                      const zelda3::DungeonSpawnPoint& spawn);
  void RefreshEntranceDirtyState(int slot_index);
  void WireViewerPanelCallbacks(DungeonCanvasViewer* viewer);
  void ConfigureMinecartProjectCallbacks();
  absl::Status ApplyMinecartTrackEdit(const MinecartTrack& before,
                                      const MinecartTrack& after,
                                      bool move_cart);
  absl::StatusOr<std::unique_ptr<UndoAction>> PairMinecartObjectCollision(
      int room_id, const std::vector<zelda3::RoomObject>& before,
      const std::vector<zelda3::RoomObject>& after,
      std::unique_ptr<UndoAction> object_action);
  absl::Status PreflightMinecartObjectCollision(
      int room_id, const std::vector<zelda3::RoomObject>& objects) const;
  absl::Status PlaceMinecartNode(int room_id, int tile_x, int tile_y,
                                 const MinecartNodeBrush& brush);
  absl::StatusOr<int> FindAvailableMinecartNodeId(MinecartNodeKind kind,
                                                  int room_id) const;
  void SynchronizeCustomObjectAssets();

  // Show or create a standalone room panel
  void ShowRoomPanel(int room_id);

  // Convenience action for Settings panel.
  void SaveAllRooms();

  // Object placement callback
  void HandleObjectPlaced(const zelda3::RoomObject& obj);
  void OpenGraphicsEditorForObject(int room_id,
                                   const zelda3::RoomObject& object);
  // Supplies the detached art-candidate preview and its source revision.
  void ConfigureStagedGraphicsPreview(
      GraphicsEditContext& context, int room_id,
      std::function<absl::Status(GraphicsEditContext&)> resolve_live_source);
  absl::Status OpenObjectTileEditorForObject(int room_id,
                                             const zelda3::RoomObject& object);

  // Helper to get or create a viewer for a specific room
  DungeonCanvasViewer* GetViewerForRoom(int room_id);
  // An explicitly open room panel keeps its own viewer even while the
  // Workbench is the active workflow (for example, a pinned room panel).
  DungeonCanvasViewer* GetStandaloneViewerForRoom(int room_id);
  DungeonCanvasViewer* GetWorkbenchViewer();
  DungeonCanvasViewer* GetWorkbenchCompareViewer(int room_id);
  void RefreshViewerRuntimeContext(DungeonCanvasViewer* viewer, int room_id,
                                   bool rebind_project = false);
  void ConfigureEditableViewer(DungeonCanvasViewer* viewer);
  bool ConsumeRoomCanvasDeleteShortcut(DungeonCanvasViewer& viewer);
  void ExpireStaleRoomCanvasDeleteShortcut();
  void TouchViewerLru(int room_id);
  void RemoveViewerFromLru(int room_id);

  absl::Status SaveRoomData(int room_id);
  absl::Status SaveRoomImpl(int room_id, bool room_data_only);
  // Fixed 14-byte room headers have no copy-on-write allocator. Refuse an
  // in-place header write that would also change another room's header.
  static absl::Status ValidateRoomHeaderOwnership(const Rom& rom, int room_id);
  absl::Status RunWithSaveTransaction(
      const std::function<absl::Status()>& operation);

  // Data
  Rom* rom_;
  zelda3::GameData* game_data_ = nullptr;
  DungeonRoomStore rooms_;
  std::array<zelda3::RoomEntrance, zelda3::kNumDungeonEntranceSlots> entrances_;
  std::array<zelda3::DungeonSpawnPoint, zelda3::kNumDungeonSpawnPoints>
      spawn_points_;
  mutable std::optional<zelda3::RoomCensus> dungeon_render_context_census_;
  mutable std::future<absl::StatusOr<zelda3::RoomCensus>>
      dungeon_render_context_census_pending_;
  mutable util::StopSource dungeon_render_context_census_stop_{
      util::kNoStopState};
  bool dungeon_render_context_census_shutdown_ = false;
  mutable bool dungeon_render_context_census_attempted_ = false;
  mutable uint64_t dungeon_render_context_census_generation_ = 0;
  mutable uint64_t dungeon_render_context_pending_generation_ = 0;
  mutable std::string dungeon_render_context_census_error_;

  struct SaveTransactionSnapshot {
    std::vector<std::pair<int, zelda3::Room::SaveDirtySnapshot>> room_states;
    std::array<bool, zelda3::kNumDungeonEntranceSlots> entrance_dirty_states{};
    std::array<bool, zelda3::kNumDungeonSpawnPoints> spawn_dirty_states{};
    bool has_pit_damage_table = false;
    bool pit_damage_dirty = false;
    bool has_palette_transaction = false;
  };
  std::optional<SaveTransactionSnapshot> save_transaction_snapshot_;
  uint64_t entrance_property_save_generation_ = 0;

  std::optional<int> room_canvas_delete_shortcut_frame_;

  // Current selection state
  // Combined slots begin with spawn points; regular Entrance 00 follows them.
  int current_entrance_id_ = zelda3::kNumDungeonSpawnPoints;
  bool automatic_entrance_preview_ = true;

  // Active room tabs and card tracking for jump-to
  ImVector<int> active_rooms_;
  int current_room_id_ = 0;

  // Recent rooms history for quick navigation (most recent first, max 10)
  static constexpr size_t kMaxRecentRooms = 10;
  std::deque<int> recent_rooms_;
  std::vector<int> pinned_rooms_;

  // Workbench panel pointer (owned by WorkspaceWindowManager, stored for notifications).
  class DungeonWorkbenchContent* workbench_panel_ = nullptr;
  RoomMatrixContent* room_matrix_panel_ = nullptr;
  DungeonEntrancesPanel* entrance_navigator_panel_ = nullptr;

  // Palette management
  gfx::SnesPalette current_palette_;
  gfx::PaletteGroup current_palette_group_;
  uint64_t current_palette_id_ = 0;
  uint64_t current_palette_group_id_ = 0;
  int palette_listener_id_ = -1;

  // Components - these do all the work
  DungeonRoomLoader room_loader_;
  DungeonRoomSelector room_selector_;
  static constexpr int kMaxCachedViewers = 20;
  // Shared by this editor's room panels, never by other ROM sessions.
  DungeonSelectionClipboard selection_clipboard_;
  util::LruCache<int, std::unique_ptr<DungeonCanvasViewer>> room_viewers_{
      kMaxCachedViewers};
  std::unique_ptr<DungeonCanvasViewer> workbench_viewer_;
  std::unique_ptr<DungeonCanvasViewer> workbench_compare_viewer_;

  gui::PaletteEditorWidget palette_editor_;
  // Panel pointers. WorkspaceWindowManager owns these when available; fallback
  // unique_ptrs keep non-workspace tests and direct embedding paths alive.
  // Workbench mode embeds room-local utilities in the Tools inspector. Their
  // standalone entries remain discoverable, but one content instance has only
  // one active presentation owner at a time.
  ObjectSelectorContent* object_selector_panel_ = nullptr;
  DoorEditorContent* door_editor_panel_ = nullptr;
  RoomGraphicsContent* room_graphics_panel_ = nullptr;
  std::function<const zelda3::Overworld*()> room_graphics_overworld_provider_;
  PaletteEditorContent* palette_editor_panel_ = nullptr;
  class SpriteEditorPanel* sprite_editor_panel_ = nullptr;
  class ItemEditorPanel* item_editor_panel_ = nullptr;
  class MinecartTrackEditorPanel* minecart_track_editor_panel_ = nullptr;
  class RoomTagEditorPanel* room_tag_editor_panel_ = nullptr;
  class ObjectCoveragePanel* object_coverage_panel_ = nullptr;
  class CustomCollisionPanel* custom_collision_panel_ = nullptr;
  class WaterFillPanel* water_fill_panel_ = nullptr;
  ObjectTileEditorPanel* object_tile_editor_panel_ = nullptr;
  OverlayManagerPanel* overlay_manager_panel_ = nullptr;

  // Owned content fields are fallbacks for tests without WorkspaceWindowManager.
  std::unique_ptr<ObjectSelectorContent> owned_object_selector_panel_;
  std::unique_ptr<DoorEditorContent> owned_door_editor_panel_;
  std::unique_ptr<RoomTagEditorPanel> owned_room_tag_editor_panel_;
  std::unique_ptr<ObjectCoveragePanel> owned_object_coverage_panel_;
  std::unique_ptr<CustomCollisionPanel> owned_custom_collision_panel_;
  std::unique_ptr<WaterFillPanel> owned_water_fill_panel_;
  std::unique_ptr<MinecartTrackEditorPanel> owned_minecart_track_editor_panel_;
  std::unique_ptr<zelda3::DungeonEditorSystem> dungeon_editor_system_;
  std::unique_ptr<emu::render::EmulatorRenderService> render_service_;

  bool is_loaded_ = false;
  uint64_t observed_custom_object_generation_ = 0;
  bool observed_custom_objects_enabled_ = false;

  // Docking class for room windows to dock together
  ImGuiWindowClass room_window_class_;

  // Shared dock ID for all room panels to auto-dock together
  ImGuiID room_dock_id_ = 0;

  // Dynamic room cards - created per open room
  std::unordered_map<int, std::shared_ptr<gui::PanelWindow>> room_cards_;

  // Stable window slot mapping: room_id -> slot_id.
  // Slot IDs are used in the "###" part of the window title so ImGui treats the
  // window as the same entity even when its displayed room changes.
  int next_room_panel_slot_id_ = 1;
  std::unordered_map<int, int> room_panel_slot_ids_;

  // Pending undo snapshot: captured on mutation callback (before edit),
  // finalized on cache invalidation callback (after edit) by pushing an undo
  // action to the inherited undo_manager_.
  struct PendingUndo {
    int room_id = -1;
    std::vector<zelda3::RoomObject> before_objects;
    std::vector<chest_data> before_chests;
    std::vector<size_t> before_selection;
    zelda3::Room::SaveDirtySnapshot dirty_before;
  };
  PendingUndo pending_undo_;
  // Ownership is deliberately session-local. Imported collision never becomes
  // disposable merely because its value resembles a generated rail.
  std::unordered_map<int, zelda3::MinecartTrackCollisionOwnership>
      minecart_collision_ownership_;
  uint64_t minecart_collision_generation_ = 0;
  bool has_pending_undo_ = false;
  bool undo_restore_triggered_ping_ = false;

  struct PendingSelectionUndo {
    std::optional<DungeonSelectionEditPlan> plan;
    zelda3::Room::SaveDirtySnapshot dirty_before;
  };
  PendingSelectionUndo pending_selection_undo_;

  struct PendingEntityUndo {
    int room_id = -1;
    DungeonEntitySnapshot before;
  };
  // Doors, sprites and pot items can be dragged together; keep each domain's
  // original snapshot until its corresponding completion notification.
  std::array<PendingEntityUndo, 3> pending_entity_undo_;

  struct PendingCollisionUndo {
    int room_id = -1;
    zelda3::CustomCollisionMap before;
  };
  PendingCollisionUndo pending_collision_undo_;

  struct PendingWaterFillUndo {
    int room_id = -1;
    WaterFillSnapshot before;
  };
  PendingWaterFillUndo pending_water_fill_undo_;

  // True while any gesture snapshot is waiting for FinalizePendingUndoActions.
  bool HasPendingUndoSnapshot() const {
    if (pending_undo_.room_id >= 0 || pending_collision_undo_.room_id >= 0 ||
        pending_water_fill_undo_.room_id >= 0 ||
        pending_selection_undo_.plan.has_value()) {
      return true;
    }
    for (const auto& pending : pending_entity_undo_) {
      if (pending.room_id >= 0) {
        return true;
      }
    }
    return false;
  }

  // Pending room swap (deferred until after draw phase completes)
  struct PendingSwap {
    int old_room_id = -1;
    int new_room_id = -1;
    bool pending = false;
  };
  PendingSwap pending_swap_;

  struct PendingWorkflowMode {
    bool enabled = false;
    bool show_toast = true;
    bool pending = false;
  };
  PendingWorkflowMode pending_workflow_mode_;

  // Opening or focusing a standalone tool mutates WorkspaceWindowManager and
  // Dear ImGui focus state. Workbench buttons are drawn inside child windows,
  // so defer that mutation to the next Update() safe point. Capture the
  // originating session so a session switch cannot redirect the request.
  struct PendingStandaloneToolWindow {
    size_t session_id = 0;
    std::string window_id;
    bool pending = false;
  };
  PendingStandaloneToolWindow pending_standalone_tool_window_;

  // Unpinned room-navigation panels hidden by the most recent transition into
  // the integrated Workbench. Standalone editing tools remain user-owned and
  // are never added to this restoration set.
  std::vector<std::string> workbench_suspended_navigation_window_ids_;

  // Two-phase undo capture: BeginUndoSnapshot saves state before mutation,
  // FinalizeUndoAction captures state after mutation and pushes the action.
  void BeginUndoSnapshot(int room_id);
  void FinalizeUndoAction(int room_id);
  absl::Status RestoreRoomObjects(
      int room_id, const std::vector<zelda3::RoomObject>& objects,
      const std::vector<size_t>& selected_indices,
      const std::vector<chest_data>& chests);
  absl::Status PreflightObjectMutation(
      int room_id, const std::vector<zelda3::RoomObject>& objects,
      const std::vector<chest_data>& chests, bool check_streams = true);
  absl::Status PreflightStreamMutation(
      int room_id, const DungeonSelectionEditState& candidate, uint8_t domains);
  absl::Status PreflightSpriteMutation(
      int room_id, const std::vector<zelda3::Sprite>& sprites);
  absl::Status PreflightPotItemPlacement(
      int room_id, const std::vector<zelda3::PotItem>& items);
  absl::StatusOr<std::vector<std::pair<uint32_t, uint32_t>>>
  PlanPendingRoomStreamRanges(
      const std::vector<DungeonSelectionEditPlan>& candidates) const;
  absl::Status PreflightDoorMutation(
      int room_id, const std::vector<zelda3::Room::Door>& doors);
  absl::Status ApplyChestObjectEdit(
      int room_id, const std::vector<zelda3::RoomObject>& objects,
      const std::vector<chest_data>& chests,
      const std::vector<size_t>& selection);

  absl::Status CommitSelectionEdit(const DungeonSelectionEditPlan& plan,
                                   bool continuous);
  void FinalizeSelectionUndoAction();
  absl::Status RestoreSelectionEditBatch(
      const std::vector<DungeonSelectionEditPlan>& plans, bool after);
  absl::Status EnsureConnectionRoomLoaded(int room_id);
  absl::Status EnsureRoomTransferLoaded(int room_id);
  absl::Status PreflightRoomTransfer(const DungeonRoomTransferPlan& plan);
  absl::Status RestoreRoomTransfer(int room_id,
                                   const DungeonRoomDocument& document);
  void RefreshSelectionEditViews(int room_id,
                                 const DungeonSelectionEditState* selection);
  void PushSelectionUndoAction(DungeonSelectionEditPlan plan);
  void PushSelectionUndoBatch(std::vector<DungeonSelectionEditPlan> plans,
                              std::string description);
  void ConfigureViewerUndoHooks(DungeonCanvasViewer* viewer);
  void FinalizePendingUndoActions();
  absl::Status RestoreRoomMetadataBatch(
      const std::vector<std::pair<int, zelda3::Room::MetadataSnapshot>>&
          states);
  absl::Status RestoreChest(int room_id, size_t index, chest_data chest);
  void RefreshRoomMetadataViews(int room_id);
  DungeonEntitySnapshot CaptureRoomEntities(int room_id, MutationDomain domain);
  void BeginEntityUndoSnapshot(int room_id, MutationDomain domain);
  void FinalizeEntityUndoAction(int room_id, MutationDomain domain);
  void FinalizePendingEntityUndoActions();
  absl::Status RestoreRoomEntities(int room_id, MutationDomain domain,
                                   const DungeonEntitySnapshot& snapshot);

  void InvalidateConnectedRoomGraphs();
  void BeginCollisionUndoSnapshot(int room_id);
  void FinalizeCollisionUndoAction(int room_id);
  void RestoreRoomCustomCollision(int room_id,
                                  const zelda3::CustomCollisionMap& map);
  absl::Status ApplyMinecartCollisionStamp(
      int room_id, const zelda3::CustomCollisionMap& expected,
      const MinecartCollisionStamp& stamp);
  absl::Status ApplyMinecartCollisionBatch(
      const std::vector<zelda3::TrackCollisionResult>& preview,
      const zelda3::GeneratorOptions& options);
  absl::Status ApplyCustomCollisionImportBatch(
      const std::vector<zelda3::CustomCollisionRoomEntry>& entries);
  absl::Status RestoreRoomCustomCollisionBatch(
      const std::vector<DungeonCustomCollisionSnapshot>& snapshots);

  void BeginWaterFillUndoSnapshot(int room_id);
  void FinalizeWaterFillUndoAction(int room_id);
  void RestoreRoomWaterFill(int room_id, const WaterFillSnapshot& snap);
  void SwapRoomInPanel(int old_room_id, int new_room_id);
  void ProcessPendingSwap();  // Process deferred swap after draw
  void ProcessPendingWorkflowMode();
  void ProcessPendingStandaloneToolWindow();

  // Room panel slot IDs provide stable ImGui window IDs across "swap room in
  // panel" navigation. This keeps the window position/dock state when the room
  // changes via the directional arrows.
  int GetOrCreateRoomPanelSlotId(int room_id);
  void ReleaseRoomPanelSlotId(int room_id);

  // Defensive guard: returns true iff room_id is within the valid range
  // [0, kNumberOfRooms). Use this instead of open-coded range checks.
  static bool IsValidRoomId(int room_id) {
    return room_id >= 0 && room_id < zelda3::kNumberOfRooms;
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H
