#ifndef YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H
#define YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/editor/system/workspace_window_manager.h"
#include "app/emu/render/emulator_render_service.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/widgets/dungeon_object_emulator_preview.h"
#include "app/gui/widgets/palette_editor_widget.h"
#include "dungeon_canvas_viewer.h"
#include "dungeon_room_loader.h"
#include "dungeon_room_selector.h"
#include "dungeon_room_store.h"
#include "dungeon_undo_actions.h"
#include "imgui/imgui.h"
#include "inspectors/door_editor_content.h"
#include "selectors/object_selector_content.h"
#include "workspace/room_graphics_content.h"
#include "rom/rom.h"
#include "util/lru_cache.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

class MinecartTrackEditorPanel;
class ObjectTileEditorPanel;
class OverlayManagerPanel;
class RoomTagEditorPanel;

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
  explicit DungeonEditorV2(Rom* rom = nullptr)
      : rom_(rom), room_loader_(rom), room_selector_(rom), rooms_(rom) {
    type_ = EditorType::kDungeon;
    if (rom) {
      dungeon_editor_system_ = zelda3::CreateDungeonEditorSystem(rom);
    }
  }

  ~DungeonEditorV2() override;

  void SetGameData(zelda3::GameData* game_data) override {
    game_data_ = game_data;
    dependencies_.game_data = game_data;  // Also set base class dependency
    room_loader_.SetGameData(game_data);
    if (object_editor_panel_) {
      object_editor_panel_->SetGameData(game_data);
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
  }

  // Editor interface
  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Cut() override;
  absl::Status Copy() override;
  absl::Status Paste() override;
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  absl::Status Save() override;
  void ContributeStatus(StatusBar* status_bar) override;
  absl::Status SaveRoom(int room_id);
  int LoadedRoomCount() const;
  int TotalRoomCount() const { return static_cast<int>(rooms_.size()); }

  // Collect PC write ranges for all dirty/loaded rooms (for write conflict
  // analysis). Returns pairs of (start_pc, end_pc) covering header and object
  // regions that would be written during save.
  std::vector<std::pair<uint32_t, uint32_t>> CollectWriteRanges() const;

  // ROM management
  void SetRom(Rom* rom) {
    rom_ = rom;
    room_loader_ = DungeonRoomLoader(rom);
    room_selector_.SetRom(rom);

    // Propagate ROM to all rooms
    if (rom) {
      rooms_.SetRom(rom);
    }

    // Reset viewers on ROM change
    room_viewers_.Clear();
  }
  Rom* rom() const { return rom_; }

  // Room management
  void add_room(int room_id);
  void FocusRoom(int room_id);

  // Agent/Automation controls
  void SelectObject(int obj_id);
  void SetAgentMode(bool enabled);

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

  // Explicit workflow toggle between integrated Workbench and standalone panels.
  void SetWorkbenchWorkflowMode(bool enabled, bool show_toast = true);
  // Queue a workflow mode change to run at a safe point in the next update.
  void QueueWorkbenchWorkflowMode(bool enabled, bool show_toast = true);
  // Queue a mode flip (Workbench <-> Standalone) for next update.
  void ToggleWorkbenchWorkflowMode(bool show_toast = true);
  bool IsWorkbenchWorkflowEnabled() const;

  // Panel card IDs for programmatic access
  static constexpr const char* kRoomSelectorId = "dungeon.room_selector";
  static constexpr const char* kEntranceListId = "dungeon.entrance_list";
  static constexpr const char* kRoomMatrixId = "dungeon.room_matrix";
  static constexpr const char* kRoomGraphicsId = "dungeon.room_graphics";
  static constexpr const char* kObjectToolsId = "dungeon.object_editor";
  static constexpr const char* kDoorEditorId = "dungeon.door_editor";
  static constexpr const char* kPaletteEditorId = "dungeon.palette_editor";

  // Public accessors for WASM API and automation
  int current_room_id() const { return room_selector_.current_room_id(); }
  int* mutable_current_room_id() { return &current_room_id_; }
  const ImVector<int>& active_rooms() const {
    return room_selector_.active_rooms();
  }
  DungeonRoomStore& rooms() { return rooms_; }
  const DungeonRoomStore& rooms() const { return rooms_; }
  gfx::IRenderer* renderer() const { return renderer_; }
  ObjectSelectorContent* object_editor_panel() const {
    return object_editor_panel_;
  }
  DoorEditorContent* door_editor_panel() const { return door_editor_panel_; }

  /**
   * @brief Get the list of recently visited room IDs
   * @return Deque of room IDs, most recent first (max 10 entries)
   */
  const std::deque<int>& GetRecentRooms() const { return recent_rooms_; }

 private:
  friend class DungeonEditorV2RomSafetyTest_UndoSnapshotLeakDetection_Test;
  friend class DungeonEditorV2RomSafetyTest_ViewerCacheLRUEviction_Test;
  friend class
      DungeonEditorV2RomSafetyTest_ViewerCacheNeverEvictsActiveRooms_Test;
  friend class
      DungeonEditorV2RomSafetyTest_ViewerCacheLRUAccessOrderUpdate_Test;

  gfx::IRenderer* renderer_ = nullptr;

  // Draw the Room Panels
  void DrawRoomPanels();
  void DrawRoomTab(int room_id);

  // Texture processing (critical for rendering)
  void ProcessDeferredTextures();

  // Room selection callback
  void OnRoomSelected(int room_id, bool request_focus = true);
  void OnRoomSelected(int room_id, RoomSelectionIntent intent);
  void OnEntranceSelected(int entrance_id);

  // Sync all sub-panels to the current room configuration
  void SyncPanelsToRoom(int room_id);

  // Show or create a standalone room panel
  void ShowRoomPanel(int room_id);

  // Convenience action for Settings panel.
  void SaveAllRooms();

  // Object placement callback
  void HandleObjectPlaced(const zelda3::RoomObject& obj);
  void OpenGraphicsEditorForObject(int room_id,
                                   const zelda3::RoomObject& object);

  // Helper to get or create a viewer for a specific room
  DungeonCanvasViewer* GetViewerForRoom(int room_id);
  DungeonCanvasViewer* GetWorkbenchViewer();
  DungeonCanvasViewer* GetWorkbenchCompareViewer();
  void TouchViewerLru(int room_id);
  void RemoveViewerFromLru(int room_id);

  absl::Status SaveRoomData(int room_id);

  // Data
  Rom* rom_;
  zelda3::GameData* game_data_ = nullptr;
  DungeonRoomStore rooms_;
  std::array<zelda3::RoomEntrance, 0x8C> entrances_;

  // Current selection state
  int current_entrance_id_ = 0;

  // Active room tabs and card tracking for jump-to
  ImVector<int> active_rooms_;
  int current_room_id_ = 0;

  // Recent rooms history for quick navigation (most recent first, max 10)
  static constexpr size_t kMaxRecentRooms = 10;
  std::deque<int> recent_rooms_;
  std::vector<int> pinned_rooms_;

  // Workbench panel pointer (owned by WorkspaceWindowManager, stored for notifications).
  class DungeonWorkbenchContent* workbench_panel_ = nullptr;

  // Palette management
  gfx::SnesPalette current_palette_;
  gfx::PaletteGroup current_palette_group_;
  uint64_t current_palette_id_ = 0;
  uint64_t current_palette_group_id_ = 0;

  // Components - these do all the work
  DungeonRoomLoader room_loader_;
  DungeonRoomSelector room_selector_;
  static constexpr int kMaxCachedViewers = 20;
  util::LruCache<int, std::unique_ptr<DungeonCanvasViewer>> room_viewers_{
      kMaxCachedViewers};
  std::unique_ptr<DungeonCanvasViewer> workbench_viewer_;
  std::unique_ptr<DungeonCanvasViewer> workbench_compare_viewer_;

  gui::PaletteEditorWidget palette_editor_;
  // Panel pointers - these are owned by WorkspaceWindowManager when available.
  // Store pointers for direct access to panel methods.
  ObjectSelectorContent* object_editor_panel_ = nullptr;
  DoorEditorContent* door_editor_panel_ = nullptr;
  RoomGraphicsContent* room_graphics_panel_ = nullptr;
  class SpriteEditorPanel* sprite_editor_panel_ = nullptr;
  class ItemEditorPanel* item_editor_panel_ = nullptr;
  class MinecartTrackEditorPanel* minecart_track_editor_panel_ = nullptr;
  class RoomTagEditorPanel* room_tag_editor_panel_ = nullptr;
  class CustomCollisionPanel* custom_collision_panel_ = nullptr;
  class WaterFillPanel* water_fill_panel_ = nullptr;
  ObjectTileEditorPanel* object_tile_editor_panel_ = nullptr;
  class DungeonSettingsPanel* dungeon_settings_panel_ = nullptr;
  OverlayManagerPanel* overlay_manager_panel_ = nullptr;

  // Fallback ownership for tests when WorkspaceWindowManager is not available.
  // In production, this remains nullptr and panels are owned by WorkspaceWindowManager.
  std::unique_ptr<ObjectSelectorContent> owned_object_editor_panel_;
  std::unique_ptr<DoorEditorContent> owned_door_editor_panel_;
  std::unique_ptr<zelda3::DungeonEditorSystem> dungeon_editor_system_;
  std::unique_ptr<emu::render::EmulatorRenderService> render_service_;

  bool is_loaded_ = false;

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
  };
  PendingUndo pending_undo_;
  bool has_pending_undo_ = false;

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

  // Two-phase undo capture: BeginUndoSnapshot saves state before mutation,
  // FinalizeUndoAction captures state after mutation and pushes the action.
  void BeginUndoSnapshot(int room_id);
  void FinalizeUndoAction(int room_id);
  void RestoreRoomObjects(int room_id,
                          const std::vector<zelda3::RoomObject>& objects);

  void BeginCollisionUndoSnapshot(int room_id);
  void FinalizeCollisionUndoAction(int room_id);
  void RestoreRoomCustomCollision(int room_id,
                                  const zelda3::CustomCollisionMap& map);

  void BeginWaterFillUndoSnapshot(int room_id);
  void FinalizeWaterFillUndoAction(int room_id);
  void RestoreRoomWaterFill(int room_id, const WaterFillSnapshot& snap);
  void SwapRoomInPanel(int old_room_id, int new_room_id);
  void ProcessPendingSwap();  // Process deferred swap after draw
  void ProcessPendingWorkflowMode();

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
