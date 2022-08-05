#include "app/editor/screen_editor.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "app/asm/script.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "gui/canvas.h"

namespace yaze {
namespace app {
namespace editor {

ScreenEditor::ScreenEditor() { screen_canvas_.SetCanvasSize(ImVec2(512, 512)); }

void ScreenEditor::Update() {
  TAB_BAR("##TabBar")
  DrawMosaicEditor();
  DrawTitleScreenEditor();
  DrawNamingScreenEditor();
  DrawOverworldMapEditor();
  DrawDungeonMapsEditor();
  DrawGameMenuEditor();
  DrawHUDEditor();
  END_TAB_BAR()
}

void ScreenEditor::DrawMosaicEditor() {
  TAB_ITEM("Mosaic Transitions")
  if (ImGui::Button("GenerateMosaicChangeAssembly")) {
    auto mosaic = snes_asm::GenerateMosaicChangeAssembly(mosaic_tiles_);
    if (!mosaic.ok()) {
      std::cout << "Failed to generate mosaic change assembly";
    } else {
      std::cout << "Successfully generated mosaic change assembly";
      std::cout << mosaic.value();
    }
  }
  END_TAB_ITEM()
}

void ScreenEditor::DrawTitleScreenEditor() {
  TAB_ITEM("Title Screen")
  END_TAB_ITEM()
}
void ScreenEditor::DrawNamingScreenEditor() {
  TAB_ITEM("Naming Screen")
  END_TAB_ITEM()
}
void ScreenEditor::DrawOverworldMapEditor() {
  TAB_ITEM("Overworld Map")
  END_TAB_ITEM()
}
void ScreenEditor::DrawDungeonMapsEditor() {
  TAB_ITEM("Dungeon Maps")
  END_TAB_ITEM()
}
void ScreenEditor::DrawGameMenuEditor() {
  TAB_ITEM("Game Menu")
  END_TAB_ITEM()
}
void ScreenEditor::DrawHUDEditor() {
  TAB_ITEM("Heads-up Display")
  END_TAB_ITEM()
}

void ScreenEditor::DrawCanvas() {
  screen_canvas_.DrawBackground();
  screen_canvas_.UpdateContext();

  screen_canvas_.DrawGrid();
  screen_canvas_.DrawOverlay();
}

void ScreenEditor::DrawToolset() {
  static bool show_bg1 = true;
  static bool show_bg2 = true;
  static bool show_bg3 = true;

  static bool drawing_bg1 = true;
  static bool drawing_bg2 = false;
  static bool drawing_bg3 = false;

  ImGui::Checkbox("Show BG1", &show_bg1);
  ImGui::SameLine();
  ImGui::Checkbox("Show BG2", &show_bg2);

  ImGui::Checkbox("Draw BG1", &drawing_bg1);
  ImGui::SameLine();
  ImGui::Checkbox("Draw BG2", &drawing_bg2);
  ImGui::SameLine();
  ImGui::Checkbox("Draw BG3", &drawing_bg3);
}

}  // namespace editor
}  // namespace app
}  // namespace yaze