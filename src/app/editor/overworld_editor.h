#ifndef YAZE_APP_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APP_EDITOR_OVERWORLDEDITOR_H

#include <imgui/imgui.h>

#include <cmath>
#include <unordered_map>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/zelda3/overworld.h"
#include "gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

static constexpr unsigned int k4BPP = 4;

class OverworldEditor {
 public:
  void SetupROM(ROM &rom);
  void Update();

 private:
  void DrawToolset();
  void DrawOverworldMapSettings();
  void DrawOverworldCanvas();
  void DrawTileSelector();
  void DrawTile16Selector() const;
  void DrawTile8Selector() const;

  void LoadBlockset();
  void LoadGraphics();

  ROM rom_;

  zelda3::Overworld overworld_;
  gfx::SNESPalette palette_;

  // pointer size 1048576
  gfx::Bitmap tile16_blockset_bmp_;

  // pointer size 32768
  gfx::Bitmap current_gfx_bmp_;

  // pointer size 456704
  gfx::Bitmap allgfxBitmap;

  gfx::Bitmap mapblockset16Bitmap;

  std::unordered_map<unsigned int, SDL_Texture *> all_texture_sheet_;

  int current_world_ = 0;
  char map_gfx_[3] = "";
  char map_palette_[3] = "";
  char spr_gfx_[3] = "";
  char spr_palette_[3] = "";
  char message_id_[5] = "";
  char staticgfx[16];
  bool isLoaded = false;
  bool doneLoaded = false;
  bool opt_enable_grid = true;
  bool all_gfx_loaded_ = false;
  bool map_blockset_loaded_ = false;

  constexpr static int kByteSize = 3;
  constexpr static int kMessageIdSize = 5;
  constexpr static int kNumSheetsToLoad = 100;
  constexpr static int kTile8DisplayHeight = 64;
  constexpr static float kInputFieldSize = 30.f;

  ImVec4 current_palette_[8];

  ImGuiTableFlags toolset_table_flags = ImGuiTableFlags_SizingFixedFit;
  ImGuiTableFlags ow_map_flags = ImGuiTableFlags_Borders;
  ImGuiTableFlags ow_edit_flags = ImGuiTableFlags_Reorderable |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchSame;
};
}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif