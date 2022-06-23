#ifndef YAZE_APP_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APP_EDITOR_OVERWORLDEDITOR_H

#include <imgui.h>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/tile.h"
#include "app/zelda3/overworld.h"
#include "gui/icons.h"

namespace yaze {
namespace gui {
namespace editor {

static constexpr unsigned int k4BPP = 4;

class OverworldEditor {
 public:
  void SetupROM(app::rom::ROM &rom);
  void Update();

 private:
  void DrawToolset();
  void DrawOverworldMapSettings();
  void DrawOverworldCanvas();
  void DrawTileSelector();
  void DrawTile16Selector();
  void DrawTile8Selector();

  void DrawChangelist();

  void LoadBlockset();
  void LoadGraphics();

  app::zelda3::Overworld overworld_;
  app::gfx::SNESPalette palette_;
  app::gfx::TilePreset current_set_;

  app::rom::ROM rom_;

  app::gfx::Bitmap tile16_blockset_bmp_;
  uchar *tile16_blockset_ptr_ = new uchar[1048576];

  app::gfx::Bitmap current_gfx_bmp_;
  uchar *current_gfx_ptr_ = new uchar[(128 * 512) / 2];

  app::gfx::Bitmap allgfxBitmap;
  uchar *allGfx16Ptr = new uchar[(128 * 7136) / 2];

  app::gfx::Bitmap mapblockset16Bitmap;

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
  bool show_changelist_ = false;
  bool all_gfx_loaded_ = false;
  bool map_blockset_loaded_ = false;

  constexpr static int kByteSize = 3;
  constexpr static int kMessageIdSize = 5;
  constexpr static int kNumSheetsToLoad = 100;
  constexpr static int kTile8DisplayHeight = 64;
  constexpr static float kInputFieldSize = 30.f;

  ImGuiTableFlags toolset_table_flags = ImGuiTableFlags_SizingFixedFit;
  ImGuiTableFlags ow_map_flags = ImGuiTableFlags_Borders;
  ImGuiTableFlags ow_edit_flags = ImGuiTableFlags_Reorderable |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchSame;
};
}  // namespace editor
}  // namespace gui
}  // namespace yaze

#endif