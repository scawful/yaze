#ifndef YAZE_APP_EDITOR_DUNGEONEDITOR_H
#define YAZE_APP_EDITOR_DUNGEONEDITOR_H

#include <imgui/imgui.h>

#include "app/core/common.h"
#include "app/core/editor.h"
#include "app/editor/modules/palette_editor.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "zelda3/dungeon/room.h"
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

class DungeonEditor : public Editor,
                      public SharedROM,
                      public core::ExperimentFlags {
 public:
  absl::Status Update() override;
  absl::Status Cut() override { return absl::OkStatus(); }
  absl::Status Copy() override { return absl::OkStatus(); }
  absl::Status Paste() override { return absl::OkStatus(); }
  absl::Status Undo() override { return absl::OkStatus(); }
  absl::Status Redo() override { return absl::OkStatus(); }

 private:
  void DrawToolset();
  void DrawRoomSelector();

  void DrawDungeonTabView();
  void DrawDungeonCanvas(int room_id);

  void DrawRoomGraphics();
  void DrawTileSelector();
  void DrawObjectRenderer();

  bool is_loaded_ = false;
  bool show_object_render_ = false;
  bool object_loaded_ = false;
  bool palette_showing_ = false;

  bool refresh_graphics_ = false;
  int current_object_ = 0;
  uint64_t current_palette_id_ = 0;
  uint64_t current_palette_group_id_ = 0;

  uint16_t current_room_id_ = 0;

  gfx::Bitmap room_gfx_bmp_;
  gfx::SNESPalette current_palette_;

  gfx::SNESPalette full_palette_;

  gfx::PaletteGroup current_palette_group_;

  gui::Canvas canvas_;
  gui::Canvas room_gfx_canvas_;
  gui::Canvas object_canvas_;

  gfx::BitmapTable graphics_bin_;

  ImVector<int> active_rooms_;
  std::vector<zelda3::dungeon::Room> rooms_;
  std::vector<gfx::BitmapManager> room_graphics_;
  zelda3::dungeon::DungeonObjectRenderer object_renderer_;

  PaletteEditor palette_editor_;

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

  ImGuiTableFlags toolset_table_flags_ =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable |
      ImGuiTableFlags_Hideable | ImGuiTableFlags_Resizable;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif
