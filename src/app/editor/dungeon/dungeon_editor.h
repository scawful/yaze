#ifndef YAZE_APP_EDITOR_DUNGEONEDITOR_H
#define YAZE_APP_EDITOR_DUNGEONEDITOR_H

#include "absl/container/flat_hash_map.h"
#include "app/editor/editor.h"
#include "app/editor/graphics/gfx_group_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/gui/canvas.h"
#include "app/rom.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/object_renderer.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_object.h"
#include "dungeon_room_selector.h"
#include "dungeon_canvas_viewer.h"
#include "dungeon_object_selector.h"
#include "dungeon_toolset.h"
#include "dungeon_object_interaction.h"
#include "dungeon_renderer.h"
#include "dungeon_room_loader.h"
#include "dungeon_usage_tracker.h"

namespace yaze {
namespace editor {

constexpr ImGuiTabItemFlags kDungeonTabFlags =
    ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip;

constexpr ImGuiTabBarFlags kDungeonTabBarFlags =
    ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable |
    ImGuiTabBarFlags_FittingPolicyResizeDown |
    ImGuiTabBarFlags_TabListPopupButton;

constexpr ImGuiTableFlags kDungeonTableFlags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
    ImGuiTableFlags_BordersV;

/**
 * @brief DungeonEditor class for editing dungeons.
 *
 * This class provides a comprehensive dungeon editing interface that integrates
 * with the new unified dungeon editing system. It includes object editing with
 * scroll wheel support, sprite management, item placement, entrance/exit editing,
 * and advanced dungeon features.
 */
class DungeonEditor : public Editor {
 public:
  explicit DungeonEditor(Rom* rom = nullptr)
      : rom_(rom), object_renderer_(rom), preview_object_(0, 0, 0, 0, 0),
        room_selector_(rom), canvas_viewer_(rom), object_selector_(rom),
        object_interaction_(&canvas_), renderer_(&canvas_, rom), room_loader_(rom) {
    type_ = EditorType::kDungeon;
    // Initialize the new dungeon editor system
    if (rom) {
      dungeon_editor_system_ = std::make_unique<zelda3::DungeonEditorSystem>(rom);
      object_editor_ = std::make_unique<zelda3::DungeonObjectEditor>(rom);
    }
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  absl::Status Save() override;

  void add_room(int i) { active_rooms_.push_back(i); }

  void set_rom(Rom* rom) { 
    rom_ = rom; 
    // Update the new UI components with the new ROM
    room_selector_.set_rom(rom_);
    canvas_viewer_.SetRom(rom_);
    object_selector_.SetRom(rom_);
  }
  Rom* rom() const { return rom_; }

  // ROM state methods (from Editor base class)
  bool IsRomLoaded() const override { return rom_ && rom_->is_loaded(); }
  std::string GetRomStatus() const override {
    if (!rom_) return "No ROM loaded";
    if (!rom_->is_loaded()) return "ROM failed to load";
    return absl::StrFormat("ROM loaded: %s", rom_->title());
  }

 private:
  absl::Status RefreshGraphics();

  void LoadDungeonRoomSize();

  absl::Status UpdateDungeonRoomView();

  void DrawDungeonTabView();
  void DrawDungeonCanvas(int room_id);
  
  // Enhanced UI methods
  void DrawCanvasAndPropertiesPanel();
  void DrawRoomPropertiesDebugPopup();
  
  // Phase 5: Integrated editor panels
  void DrawObjectEditorPanels();
  void RenderRoomWithObjects(int room_id);
  void UpdateObjectEditor();
  
  // Room selection management
  void OnRoomSelected(int room_id);

  void DrawRoomGraphics();
  void DrawTileSelector();
  void DrawObjectRenderer();
  
  // Legacy methods (delegated to components)
  absl::Status LoadAndRenderRoomGraphics(int room_id);
  absl::Status ReloadAllRoomGraphics();
  absl::Status UpdateRoomBackgroundLayers(int room_id);

  // Object preview system
  zelda3::RoomObject preview_object_;
  gfx::SnesPalette preview_palette_;

  bool is_loaded_ = false;
  bool object_loaded_ = false;
  bool palette_showing_ = false;
  bool refresh_graphics_ = false;
  
  // Phase 5: Integrated object editor system
  std::unique_ptr<zelda3::DungeonObjectEditor> object_editor_;
  bool show_object_property_panel_ = true;
  bool show_layer_controls_ = true;
  bool enable_selection_highlight_ = true;
  bool enable_layer_visualization_ = true;
  
  // Legacy editor system (deprecated)
  std::unique_ptr<zelda3::DungeonEditorSystem> dungeon_editor_system_;
  bool show_sprite_editor_ = false;
  bool show_item_editor_ = false;
  bool show_entrance_editor_ = false;
  bool show_door_editor_ = false;
  bool show_chest_editor_ = false;
  bool show_properties_editor_ = false;

  uint16_t current_entrance_id_ = 0;
  uint16_t current_room_id_ = 0;
  uint64_t current_palette_id_ = 0;
  uint64_t current_palette_group_id_ = 0;

  ImVector<int> active_rooms_;
  int current_active_room_tab_ = 0; // Track which room tab is currently active

  GfxGroupEditor gfx_group_editor_;
  PaletteEditor palette_editor_;
  gfx::SnesPalette current_palette_;
  gfx::SnesPalette full_palette_;
  gfx::PaletteGroup current_palette_group_;

  gui::Canvas canvas_{"##DungeonCanvas", ImVec2(0x200, 0x200)};
  gui::Canvas room_gfx_canvas_{"##RoomGfxCanvas",
                               ImVec2(0x100 + 1, 0x10 * 0x40 + 1)};
  gui::Canvas object_canvas_;

  std::array<gfx::Bitmap, kNumGfxSheets> graphics_bin_;

  std::array<zelda3::Room, 0x128> rooms_ = {};
  std::array<zelda3::RoomEntrance, 0x8C> entrances_ = {};
  zelda3::ObjectRenderer object_renderer_;

  // UI components
  DungeonRoomSelector room_selector_;
  DungeonCanvasViewer canvas_viewer_;
  DungeonObjectSelector object_selector_;
  
  // Refactored components
  DungeonToolset toolset_;
  DungeonObjectInteraction object_interaction_;
  DungeonRenderer renderer_;
  DungeonRoomLoader room_loader_;
  DungeonUsageTracker usage_tracker_;

  absl::Status status_;

  Rom* rom_;
};

}  // namespace editor
}  // namespace yaze

#endif
