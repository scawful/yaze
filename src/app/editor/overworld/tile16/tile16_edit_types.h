#ifndef YAZE_APP_EDITOR_OVERWORLD_TILE16_TILE16_EDIT_TYPES_H_
#define YAZE_APP_EDITOR_OVERWORLD_TILE16_TILE16_EDIT_TYPES_H_

#include <array>
#include <string>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze {
namespace editor {

constexpr int kTile16Size = 16;
constexpr int kTile8Size = 8;
constexpr int kTilesheetEditorWidth = 0x100;
constexpr int kTilesheetEditorHeight = 0x4000;
constexpr int kTile16CanvasSize = 0x20;
constexpr int kTile8CanvasHeight = 0x175;
constexpr int kNumScratchSlots = 4;
constexpr int kNumTile16Palettes = 8;
constexpr int kTile8PixelCount = 64;
constexpr int kTile16PixelCount = 256;

enum class Tile16EditMode {
  kPaint = 0,
  kPick = 1,
  kUsageProbe = 2,
};

struct Tile16ClipboardData {
  gfx::Tile16 tile_data;
  gfx::Bitmap bitmap;
  bool has_data = false;
};

struct Tile16ScratchData {
  gfx::Tile16 tile_data;
  gfx::Bitmap bitmap;
  bool has_data = false;
};

struct Tile16Commit {
  int tile_id = -1;
  gfx::Tile16 tile_data;
};

/// Logical pixel position within a Tile16 (origin top-left, units = source
/// pixels). Replaces ImVec2 in domain APIs so sessions stay ImGui-free.
struct Tile16LocalPos {
  float x = 0.0f;
  float y = 0.0f;
};

struct Tile16LayoutScratch {
  std::array<std::array<int, 8>, 8> tile_layout{};
  bool in_use = false;
  std::string name = "Empty";
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_TILE16_TILE16_EDIT_TYPES_H_
