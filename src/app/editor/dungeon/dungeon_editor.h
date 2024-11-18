#ifndef YAZE_APP_EDITOR_DUNGEONEDITOR_H
#define YAZE_APP_EDITOR_DUNGEONEDITOR_H

#include "app/core/common.h"
#include "absl/container/flat_hash_map.h"
#include "app/editor/graphics/gfx_group_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/editor.h"
#include "app/gui/canvas.h"
#include "app/rom.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace app {
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
 * This class is currently a work in progress and is used for editing dungeons.
 * It provides various functions for updating, cutting, copying, pasting,
 * undoing, and redoing. It also includes methods for drawing the toolset, room
 * selector, entrance selector, dungeon tab view, dungeon canvas, room graphics,
 * tile selector, and object renderer. Additionally, it handles loading room
 * entrances, calculating usage statistics, and rendering set usage.
 */
class DungeonEditor : public Editor,
                      public SharedRom,
                      public core::ExperimentFlags {
 public:
  DungeonEditor() { type_ = EditorType::kDungeon; }

  absl::Status Update() override;
  absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }

  void add_room(int i) { active_rooms_.push_back(i); }

 private:
  absl::Status Initialize();
  absl::Status RefreshGraphics();

  void LoadDungeonRoomSize();

  absl::Status UpdateDungeonRoomView();

  void DrawToolset();
  void DrawRoomSelector();
  void DrawEntranceSelector();

  void DrawDungeonTabView();
  void DrawDungeonCanvas(int room_id);

  void DrawRoomGraphics();
  void DrawTileSelector();
  void DrawObjectRenderer();

  void CalculateUsageStats();
  void DrawUsageStats();
  void DrawUsageGrid();
  void RenderSetUsage(const absl::flat_hash_map<uint16_t, int>& usage_map,
                      uint16_t& selected_set, int spriteset_offset = 0x00);

  enum BackgroundType {
    kNoBackground,
    kBackground1,
    kBackground2,
    kBackground3,
    kBackgroundAny,
  };
  enum PlacementType { kNoType, kSprite, kItem, kDoor, kBlock };

  int background_type_ = kNoBackground;
  int placement_type_ = kNoType;
  int current_object_ = 0;

  bool is_loaded_ = false;
  bool object_loaded_ = false;
  bool palette_showing_ = false;
  bool refresh_graphics_ = false;

  uint16_t current_entrance_id_ = 0;
  uint16_t current_room_id_ = 0;
  uint64_t current_palette_id_ = 0;
  uint64_t current_palette_group_id_ = 0;

  ImVector<int> active_rooms_;

  GfxGroupEditor gfx_group_editor_;
  PaletteEditor palette_editor_;
  gfx::SnesPalette current_palette_;
  gfx::SnesPalette full_palette_;
  gfx::PaletteGroup current_palette_group_;

  gui::Canvas canvas_;
  gui::Canvas room_gfx_canvas_;
  gui::Canvas object_canvas_;

  gfx::Bitmap room_gfx_bmp_;
  std::array<gfx::Bitmap, kNumGfxSheets> graphics_bin_;

  std::vector<gfx::Bitmap*> room_gfx_sheets_;
  std::vector<zelda3::dungeon::Room> rooms_;
  std::vector<zelda3::dungeon::RoomEntrance> entrances_;
  zelda3::dungeon::DungeonObjectRenderer object_renderer_;

  absl::flat_hash_map<uint16_t, int> spriteset_usage_;
  absl::flat_hash_map<uint16_t, int> blockset_usage_;
  absl::flat_hash_map<uint16_t, int> palette_usage_;

  std::vector<int64_t> room_size_pointers_;

  uint16_t selected_blockset_ = 0xFFFF;  // 0xFFFF indicates no selection
  uint16_t selected_spriteset_ = 0xFFFF;
  uint16_t selected_palette_ = 0xFFFF;

  uint64_t total_room_size_ = 0;

  std::unordered_map<int, int> room_size_addresses_;
  std::unordered_map<int, ImVec4> room_palette_;

  absl::Status status_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif
