#include "app/editor/screen_editor.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/asm/script.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "gui/canvas.h"
#include "gui/input.h"

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

void ScreenEditor::DrawWorldGrid(int world, int h, int w) {
  const float time = (float)ImGui::GetTime();

  int i = 0;
  if (world == 1) {
    i = 64;
  } else if (world == 2) {
    i = 128;
  }
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      if (x > 0) ImGui::SameLine();
      ImGui::PushID(y * 4 + x);
      std::string label = absl::StrCat(" #", absl::StrFormat("%x", i));
      if (ImGui::Selectable(label.c_str(), mosaic_tiles_[i] != 0, 0,
                            ImVec2(35, 25))) {
        mosaic_tiles_[i] ^= 1;
      }
      ImGui::PopID();
      i++;
    }
}

void ScreenEditor::DrawMosaicEditor() {
  TAB_ITEM("Mosaic Transitions")

  if (ImGui::BeginTable("Worlds", 3, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Light World");
    ImGui::TableSetupColumn("Dark World");
    ImGui::TableSetupColumn("Special World");
    ImGui::TableHeadersRow();

    ImGui::TableNextColumn();
    DrawWorldGrid(0);

    ImGui::TableNextColumn();
    DrawWorldGrid(1);

    ImGui::TableNextColumn();
    DrawWorldGrid(2, 4);

    ImGui::EndTable();
  }

  gui::InputHex("Routine Location", &overworldCustomMosaicASM);

  if (ImGui::Button("Generate Mosaic Assembly")) {
    auto mosaic = mosaic_script_.GenerateMosaicChangeAssembly(
        rom_, mosaic_tiles_, overworldCustomMosaicASM);
    if (!mosaic.ok()) {
      std::cout << mosaic;
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
  screen_canvas_.DrawContextMenu();

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