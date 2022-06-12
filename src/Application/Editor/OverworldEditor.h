#ifndef YAZE_APPLICATION_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APPLICATION_EDITOR_OVERWORLDEDITOR_H

#include <imgui/imgui.h>

#include "Core/Icons.h"
#include "Data/Overworld.h"
#include "Graphics/Palette.h"
#include "Graphics/Scene.h"
#include "Graphics/Tile.h"
#include "Utils/Compression.h"

namespace yaze {
namespace Application {
namespace Editor {

using byte = unsigned char;

static constexpr unsigned int k4BPP = 4;

class OverworldEditor {
 public:
  void Update();

  void SetRom(Utils::ROM &rom) { rom_ = rom; }

 private:
  void DrawToolset();
  void DrawOverworldMapSettings();
  void DrawOverworldCanvas();
  void DrawTileSelector();

  void DrawChangelist();

  bool show_changelist_ = false;

  Utils::ROM rom_;
  Data::Overworld overworld;
  Graphics::Scene current_scene_;
  Utils::ALTTPCompression alttp_compressor_;
  Graphics::Bitmap allgfxBitmap;
  Graphics::SNESPalette palette_;
  Graphics::TilePreset current_set_;

  SDL_Texture *gfx_texture = nullptr;

  int allgfx_width = 0;
  int allgfx_height = 0;
  GLuint *allgfx_texture = nullptr;

  byte *allGfx16Ptr = new byte[(128 * 7136) / 2];

  GLuint *overworld_texture;

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
}  // namespace Application
}  // namespace yaze

#endif