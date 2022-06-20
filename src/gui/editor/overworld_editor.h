#ifndef YAZE_APP_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APP_EDITOR_OVERWORLDEDITOR_H

#include <imgui/imgui.h>

#include "gfx/palette.h"
#include "gfx/tile.h"
#include "gui/icons.h"
#include "zelda3/overworld.h"

namespace yaze {
namespace gui {
namespace editor {

static constexpr unsigned int k4BPP = 4;

class OverworldEditor {
 public:
  void SetupROM(app::ROM &rom);
  void Update();

 private:
  void DrawToolset();
  void DrawOverworldMapSettings();
  void DrawOverworldCanvas();
  void DrawTileSelector();
  void DrawTile8Selector();
  void DrawChangelist();

  void Loadgfx();

  app::ROM rom_;
  app::zelda3::Overworld overworld_;
  app::gfx::Bitmap allgfxBitmap;
  app::gfx::SNESPalette palette_;
  app::gfx::TilePreset current_set_;
  std::unordered_map<unsigned int, SDL_Texture *> all_texture_sheet_;

  SDL_Texture *gfx_texture = nullptr;

  int allgfx_width = 0;
  int allgfx_height = 0;

  uchar *allGfx16Ptr = new uchar[(128 * 7136) / 2];

  float canvas_table_ratio = 30.f;

  char map_gfx_[3] = "";
  char map_palette_[3] = "";
  char spr_gfx_[3] = "";
  char spr_palette_[3] = "";
  char message_id_[5] = "";

  int current_world_ = 0;

  bool isLoaded = false;
  bool doneLoaded = false;
  bool opt_enable_grid = true;
  bool show_changelist_ = false;
  bool all_gfx_loaded_ = false;

  constexpr static int kByteSize = 3;
  constexpr static int kMessageIdSize = 5;
  constexpr static float kInputFieldSize = 30.f;
  constexpr static int kNumSheetsToLoad = 100;
  constexpr static int kTile8DisplayHeight = 64;

  ImGuiTableFlags toolset_table_flags = ImGuiTableFlags_SizingFixedFit;
  ImGuiTableFlags ow_map_settings_flags = ImGuiTableFlags_Borders;
  ImGuiTableFlags ow_edit_flags = ImGuiTableFlags_Reorderable |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchSame;
};
}  // namespace editor
}  // namespace gui
}  // namespace yaze

#endif