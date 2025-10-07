#ifndef YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H
#define YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/gfx/snes_palette.h"
#include "app/rom.h"
#include "dungeon_room_selector.h"
#include "dungeon_canvas_viewer.h"
#include "dungeon_object_selector.h"
#include "dungeon_room_loader.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/room_entrance.h"
#include "app/gui/editor_layout.h"
#include "app/gui/widgets/dungeon_object_emulator_preview.h"
#include "imgui/imgui.h"

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
        canvas_viewer_(rom),
        object_selector_(rom),
        object_emulator_preview_() {
    type_ = EditorType::kDungeon;
  }

  // Editor interface
  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  absl::Status Save() override;

  // ROM management
  void set_rom(Rom* rom) {
    rom_ = rom;
    room_loader_ = DungeonRoomLoader(rom);
    room_selector_.set_rom(rom);
    canvas_viewer_.SetRom(rom);
    object_selector_.SetRom(rom);
    object_emulator_preview_.Initialize(rom);
  }
  Rom* rom() const { return rom_; }

  // Room management
  void add_room(int room_id) { active_rooms_.push_back(room_id); }

  // ROM state
  bool IsRomLoaded() const override { return rom_ && rom_->is_loaded(); }
  std::string GetRomStatus() const override {
    if (!rom_) return "No ROM loaded";
    if (!rom_->is_loaded()) return "ROM failed to load";
    return absl::StrFormat("ROM loaded: %s", rom_->title());
  }

 private:
  // Simple UI layout
  void DrawLayout();
  void DrawRoomTab(int room_id);
  void DrawToolset();
  
  // Room selection callback
  void OnRoomSelected(int room_id);

  // Data
  Rom* rom_;
  std::array<zelda3::Room, 0x128> rooms_;
  std::array<zelda3::RoomEntrance, 0x8C> entrances_;
  
  // Active room tabs
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
  DungeonCanvasViewer canvas_viewer_;
  DungeonObjectSelector object_selector_;
  gui::DungeonObjectEmulatorPreview object_emulator_preview_;
  
  bool is_loaded_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_EDITOR_V2_H

