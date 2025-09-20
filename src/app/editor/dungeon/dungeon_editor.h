#ifndef YAZE_APP_EDITOR_DUNGEONEDITOR_H
#define YAZE_APP_EDITOR_DUNGEONEDITOR_H

#include "absl/container/flat_hash_map.h"
#include "app/editor/editor.h"
#include "app/editor/graphics/gfx_group_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/gui/canvas.h"
#include "app/rom.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/object_renderer.h"

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
 * This class is currently a work in progress and is used for editing dungeons.
 * It provides various functions for updating, cutting, copying, pasting,
 * undoing, and redoing. It also includes methods for drawing the toolset, room
 * selector, entrance selector, dungeon tab view, dungeon canvas, room graphics,
 * tile selector, and object renderer. Additionally, it handles loading room
 * entrances, calculating usage statistics, and rendering set usage.
 */
class DungeonEditor : public Editor {
 public:
  explicit DungeonEditor(Rom* rom = nullptr) : rom_(rom), object_renderer_(rom) {
    type_ = EditorType::kDungeon;
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  absl::Status Save() override { return absl::UnimplementedError("Save"); }

  void add_room(int i) { active_rooms_.push_back(i); }

  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

 private:
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

  gui::Canvas canvas_{"##DungeonCanvas", ImVec2(0x200, 0x200)};
  gui::Canvas room_gfx_canvas_{"##RoomGfxCanvas",
                               ImVec2(0x100 + 1, 0x10 * 0x40 + 1)};
  gui::Canvas object_canvas_;

  std::array<gfx::Bitmap, kNumGfxSheets> graphics_bin_;

  std::array<zelda3::Room, 0x128> rooms_ = {};
  std::array<zelda3::RoomEntrance, 0x8C> entrances_ = {};
  zelda3::ObjectRenderer object_renderer_;

  absl::flat_hash_map<uint16_t, int> spriteset_usage_;
  absl::flat_hash_map<uint16_t, int> blockset_usage_;
  absl::flat_hash_map<uint16_t, int> palette_usage_;

  std::vector<int64_t> room_size_pointers_;
  std::vector<int64_t> room_sizes_;

  uint16_t selected_blockset_ = 0xFFFF;  // 0xFFFF indicates no selection
  uint16_t selected_spriteset_ = 0xFFFF;
  uint16_t selected_palette_ = 0xFFFF;

  uint64_t total_room_size_ = 0;

  std::unordered_map<int, int> room_size_addresses_;
  std::unordered_map<int, ImVec4> room_palette_;

  absl::Status status_;

  Rom* rom_;
};

}  // namespace editor
}  // namespace yaze

#endif
