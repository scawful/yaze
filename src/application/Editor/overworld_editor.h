#ifndef YAZE_APPLICATION_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APPLICATION_EDITOR_OVERWORLDEDITOR_H

#include <imgui/imgui.h>

#include "Data/OW/overworld.h"
#include "Graphics/icons.h"
#include "Graphics/palette.h"
#include "Graphics/tile.h"

namespace yaze {
namespace application {
namespace Editor {

static constexpr unsigned int k4BPP = 4;

class OverworldEditor {
 public:
  void Update();

 private:
  void DrawToolset();
  void DrawOverworldMapSettings();
  void DrawOverworldCanvas();
  void DrawTileSelector();

  void DrawChangelist();

  bool show_changelist_ = false;

  Data::ROM rom_;
  Data::Overworld overworld;
  Graphics::Bitmap allgfxBitmap;
  Graphics::SNESPalette palette_;
  Graphics::TilePreset current_set_;

  SDL_Texture *gfx_texture = nullptr;

  int allgfx_width = 0;
  int allgfx_height = 0;

  uchar *allGfx16Ptr = new uchar[(128 * 7136) / 2];

  ImGuiTableFlags toolset_table_flags = ImGuiTableFlags_SizingFixedFit;
  ImGuiTableFlags ow_map_settings_flags = ImGuiTableFlags_Borders;
  ImGuiTableFlags ow_edit_flags = ImGuiTableFlags_Reorderable |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchSame;

  float canvas_table_ratio = 30.f;

  char map_gfx_[3] = "";
  char map_palette_[3] = "";
  char spr_gfx_[3] = "";
  char spr_palette_[3] = "";
  char message_id_[5] = "";

  int current_world_ = 0;

  bool isLoaded = false;
  bool doneLoaded = false;

  constexpr static int kByteSize = 3;
  constexpr static int kMessageIdSize = 5;
  constexpr static float kInputFieldSize = 30.f;
  bool opt_enable_grid = true;
};
}  // namespace Editor
}  // namespace application
}  // namespace yaze

#endif