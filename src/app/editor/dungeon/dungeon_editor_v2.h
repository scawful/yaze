#ifndef YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H
#define YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H

#include <memory>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/emu/render/emulator_render_service.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/widgets/dungeon_object_emulator_preview.h"
#include "app/gui/widgets/palette_editor_widget.h"
#include "app/rom.h"
#include "dungeon_canvas_viewer.h"
#include "dungeon_object_selector.h"
#include "dungeon_room_loader.h"
#include "dungeon_room_selector.h"
#include "imgui/imgui.h"
#include "object_editor_card.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/dungeon_editor_system.h"

namespace yaze {
namespace editor {

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
 *
 * The editor acts as a coordinator, not an implementer.
 */
class DungeonEditorV2 : public Editor {
 public:
  explicit DungeonEditorV2(Rom* rom = nullptr)
      : rom_(rom),
        room_loader_(rom),
        room_selector_(rom),
        canvas_viewer_(rom) {
    type_ = EditorType::kDungeon;
    if (rom) {
        dungeon_editor_system_ = zelda3::CreateDungeonEditorSystem(rom);
        for (auto& room : rooms_) {
          room.SetRom(rom);
        }
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
  void set_rom(Rom* rom) {
    rom_ = rom;
    room_loader_ = DungeonRoomLoader(rom);
    room_selector_.set_rom(rom);
    canvas_viewer_.SetRom(rom);
    
    // Propagate ROM to all rooms
    if (rom) {
      for (auto& room : rooms_) {
        room.SetRom(rom);
      }
    }

    // Create render service if needed
    if (rom && rom->is_loaded() && !render_service_) {
      render_service_ = std::make_unique<emu::render::EmulatorRenderService>(rom);
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

  // Card visibility flags - Public for command-line flag access
  bool show_room_selector_ = false;  // Room selector/list card
  bool show_room_matrix_ = false;    // Dungeon matrix layout
  bool show_entrances_list_ =
      false;  // Entrance list card (renamed from entrances_matrix_)
  bool show_room_graphics_ = false;   // Room graphics card
  bool show_object_editor_ = false;   // Object editor card
  bool show_palette_editor_ = false;  // Palette editor card
  bool show_debug_controls_ = false;   // Debug controls card
  bool show_control_panel_ = true;     // Control panel (visible by default)


  // Public accessors for WASM API and automation
  int current_room_id() const { return room_selector_.current_room_id(); }
  const ImVector<int>& active_rooms() const { return room_selector_.active_rooms(); }
  ObjectEditorCard* object_editor_card() const { return object_editor_card_.get(); }

 private:
  gfx::IRenderer* renderer_ = nullptr;

  // UI drawing (Phase 4: Static panels now use EditorPanel, only dynamic rooms here)
  void DrawLayout();
  void DrawRoomTab(int room_id);

  // Texture processing (critical for rendering)
  void ProcessDeferredTextures();

  // Room selection callback
  void OnRoomSelected(int room_id);
  void OnEntranceSelected(int entrance_id);

  // Object placement callback
  void HandleObjectPlaced(const zelda3::RoomObject& obj);

  // Data
  Rom* rom_;
  std::array<zelda3::Room, 0x128> rooms_;
  std::array<zelda3::RoomEntrance, 0x8C> entrances_;

  // Current selection state
  int current_entrance_id_ = 0;

  // Active room tabs and card tracking for jump-to
  ImVector<int> active_rooms_;
  // Panels
  gui::PanelWindow room_properties_card_{"Room Properties", ICON_MD_TUNE};
  gui::PanelWindow object_tool_card_{"Object Tools", ICON_MD_BUILD};
  int current_room_id_ = 0;

  bool control_panel_minimized_ = false;

  // Palette management
  gfx::SnesPalette current_palette_;
  gfx::PaletteGroup current_palette_group_;
  uint64_t current_palette_id_ = 0;
  uint64_t current_palette_group_id_ = 0;

  // Components - these do all the work
  DungeonRoomLoader room_loader_;
  DungeonRoomSelector room_selector_;
  DungeonCanvasViewer canvas_viewer_;

  gui::PaletteEditorWidget palette_editor_;
  std::unique_ptr<ObjectEditorCard>
      object_editor_card_;  // Unified object editor
  std::unique_ptr<zelda3::DungeonEditorSystem> dungeon_editor_system_;
  std::unique_ptr<emu::render::EmulatorRenderService> render_service_;

  bool is_loaded_ = false;

  // Docking class for room windows to dock together
  ImGuiWindowClass room_window_class_;

  // Dynamic room cards - created per open room
  std::unordered_map<int, std::shared_ptr<gui::PanelWindow>> room_cards_;

  // Undo/Redo history: store snapshots of room objects
  std::unordered_map<int, std::vector<std::vector<zelda3::RoomObject>>>
      undo_history_;
  std::unordered_map<int, std::vector<std::vector<zelda3::RoomObject>>>
      redo_history_;

  void PushUndoSnapshot(int room_id);
  absl::Status RestoreFromSnapshot(int room_id,
                                   std::vector<zelda3::RoomObject> snapshot);
  void ClearRedo(int room_id);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H
