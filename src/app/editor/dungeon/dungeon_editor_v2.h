#ifndef YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H
#define YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/editor/system/panel_manager.h"
#include "app/emu/render/emulator_render_service.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/widgets/dungeon_object_emulator_preview.h"
#include "app/gui/widgets/palette_editor_widget.h"
#include "dungeon_canvas_viewer.h"
#include "dungeon_room_loader.h"
#include "dungeon_room_selector.h"
#include "imgui/imgui.h"
#include "panels/dungeon_room_graphics_panel.h"
#include "panels/object_editor_panel.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

class MinecartTrackEditorPanel;

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
 * OWNED BY PanelManager (registered EditorPanels):
 * - object_editor_panel_ - registered via RegisterEditorPanel()
 * - room_graphics_panel_ - registered via RegisterEditorPanel()
 * - sprite_editor_panel_ - registered via RegisterEditorPanel()
 * - item_editor_panel_ - registered via RegisterEditorPanel()
 *
 * Panel pointers are stored for convenience access but should NOT be
 * deleted by this class. PanelManager owns them.
 */
class DungeonEditorV2 : public Editor {
 public:
  explicit DungeonEditorV2(Rom* rom = nullptr)
      : rom_(rom), room_loader_(rom), room_selector_(rom) {
    type_ = EditorType::kDungeon;
    if (rom) {
      dungeon_editor_system_ = zelda3::CreateDungeonEditorSystem(rom);
      for (auto& room : rooms_) {
        room.SetRom(rom);
      }
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
    for (auto& room : rooms_) {
      room.SetGameData(game_data);
    }
    // Note: Canvas viewer game data is set lazily in GetViewerForRoom
    // but we should update existing viewers
    for (auto& [id, viewer] : room_viewers_) {
      if (viewer)
        viewer->SetGameData(game_data);
    }
  }

  // Editor interface
  void Initialize(gfx::IRenderer* renderer, Rom* rom);
  void Initialize() override;
  absl::Status Load();
  absl::Status Update() override;
  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Cut() override;
  absl::Status Copy() override;
  absl::Status Paste() override;
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  absl::Status Save() override;

  // ROM management
  void SetRom(Rom* rom) {
    rom_ = rom;
    room_loader_ = DungeonRoomLoader(rom);
    room_selector_.SetRom(rom);

    // Propagate ROM to all rooms
    if (rom) {
      for (auto& room : rooms_) {
        room.SetRom(rom);
      }
    }

    // Reset viewers on ROM change
    room_viewers_.clear();

    // Create render service if needed
    if (rom && rom->is_loaded() && !render_service_) {
      render_service_ =
          std::make_unique<emu::render::EmulatorRenderService>(rom);
      render_service_->Initialize();
    }
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

  // Show a panel by its card_id using PanelManager
  void ShowPanel(const std::string& card_id) {
    if (dependencies_.panel_manager) {
      dependencies_.panel_manager->ShowPanel(card_id);
    }
  }

  // Panel card IDs for programmatic access
  static constexpr const char* kRoomSelectorId = "dungeon.room_selector";
  static constexpr const char* kEntranceListId = "dungeon.entrance_list";
  static constexpr const char* kRoomMatrixId = "dungeon.room_matrix";
  static constexpr const char* kRoomGraphicsId = "dungeon.room_graphics";
  static constexpr const char* kObjectToolsId = "dungeon.object_tools";
  static constexpr const char* kPaletteEditorId = "dungeon.palette_editor";

  // Public accessors for WASM API and automation
  int current_room_id() const { return room_selector_.current_room_id(); }
  int* mutable_current_room_id() { return &current_room_id_; }
  const ImVector<int>& active_rooms() const {
    return room_selector_.active_rooms();
  }
  std::array<zelda3::Room, 0x128>& rooms() { return rooms_; }
  const std::array<zelda3::Room, 0x128>& rooms() const { return rooms_; }
  gfx::IRenderer* renderer() const { return renderer_; }
  ObjectEditorPanel* object_editor_panel() const {
    return object_editor_panel_;
  }

 private:
  gfx::IRenderer* renderer_ = nullptr;

  // Draw the Room Panels
  void DrawRoomPanels();
  void DrawRoomTab(int room_id);

  // Texture processing (critical for rendering)
  void ProcessDeferredTextures();

  // Room selection callback
  void OnRoomSelected(int room_id, bool request_focus = true);
  void OnEntranceSelected(int entrance_id);

  // Swap room in current panel (for arrow navigation)
  void SwapRoomInPanel(int old_room_id, int new_room_id);

  // Object placement callback
  void HandleObjectPlaced(const zelda3::RoomObject& obj);
  void OpenGraphicsEditorForObject(int room_id,
                                   const zelda3::RoomObject& object);

  // Helper to get or create a viewer for a specific room
  DungeonCanvasViewer* GetViewerForRoom(int room_id);

  // Data
  Rom* rom_;
  zelda3::GameData* game_data_ = nullptr;
  std::array<zelda3::Room, 0x128> rooms_;
  std::array<zelda3::RoomEntrance, 0x8C> entrances_;

  // Current selection state
  int current_entrance_id_ = 0;

  // Active room tabs and card tracking for jump-to
  ImVector<int> active_rooms_;
  int current_room_id_ = 0;

  // Palette management
  gfx::SnesPalette current_palette_;
  gfx::PaletteGroup current_palette_group_;
  uint64_t current_palette_id_ = 0;
  uint64_t current_palette_group_id_ = 0;

  // Components - these do all the work
  DungeonRoomLoader room_loader_;
  DungeonRoomSelector room_selector_;
  // canvas_viewer_ removed in favor of room_viewers_
  std::map<int, std::unique_ptr<DungeonCanvasViewer>> room_viewers_;

  gui::PaletteEditorWidget palette_editor_;
  // Panel pointers - these are owned by PanelManager when available.
  // Store pointers for direct access to panel methods.
  ObjectEditorPanel* object_editor_panel_ = nullptr;
  DungeonRoomGraphicsPanel* room_graphics_panel_ = nullptr;
  class SpriteEditorPanel* sprite_editor_panel_ = nullptr;
  class ItemEditorPanel* item_editor_panel_ = nullptr;
  class MinecartTrackEditorPanel* minecart_track_editor_panel_ = nullptr;

  // Fallback ownership for tests when PanelManager is not available.
  // In production, this remains nullptr and panels are owned by PanelManager.
  std::unique_ptr<ObjectEditorPanel> owned_object_editor_panel_;
  std::unique_ptr<zelda3::DungeonEditorSystem> dungeon_editor_system_;
  std::unique_ptr<emu::render::EmulatorRenderService> render_service_;

  bool is_loaded_ = false;

  // Docking class for room windows to dock together
  ImGuiWindowClass room_window_class_;

  // Shared dock ID for all room panels to auto-dock together
  ImGuiID room_dock_id_ = 0;

  // Dynamic room cards - created per open room
  std::unordered_map<int, std::shared_ptr<gui::PanelWindow>> room_cards_;

  // Undo/Redo history: store snapshots of room objects
  std::unordered_map<int, std::vector<std::vector<zelda3::RoomObject>>>
      undo_history_;
  std::unordered_map<int, std::vector<std::vector<zelda3::RoomObject>>>
      redo_history_;

  // Pending room swap (deferred until after draw phase completes)
  struct PendingSwap {
    int old_room_id = -1;
    int new_room_id = -1;
    bool pending = false;
  };
  PendingSwap pending_swap_;

  void PushUndoSnapshot(int room_id);
  absl::Status RestoreFromSnapshot(int room_id,
                                   std::vector<zelda3::RoomObject> snapshot);
  void ClearRedo(int room_id);
  void ProcessPendingSwap();  // Process deferred swap after draw
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H
