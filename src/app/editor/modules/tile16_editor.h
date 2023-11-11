#ifndef YAZE_APP_EDITOR_TILE16EDITOR_H
#define YAZE_APP_EDITOR_TILE16EDITOR_H

#include <imgui/imgui.h>

#include <cmath>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/editor.h"
#include "app/core/pipeline.h"
#include "app/editor/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "app/zelda3/overworld.h"

class Tile16Editor {
 public:
  absl::Status Update();
};

#endif  // YAZE_APP_EDITOR_TILE16EDITOR_H