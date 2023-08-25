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
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"

namespace yaze {
namespace app {
namespace editor {

ScreenEditor::ScreenEditor() { screen_canvas_.SetCanvasSize(ImVec2(512, 512)); }

void ScreenEditor::Update() {
  TAB_BAR("##TabBar")
  DrawInventoryMenuEditor();
  DrawTitleScreenEditor();
  DrawNamingScreenEditor();
  DrawOverworldMapEditor();
  DrawDungeonMapsEditor();
  END_TAB_BAR()
}

void ScreenEditor::DrawInventoryMenuEditor() {
  TAB_ITEM("Inventory Menu")

  static bool create = false;
  if (!create && rom()->isLoaded()) {
    inventory_.Create();
    palette_ = inventory_.Palette();
    create = true;
  }

  DrawInventoryToolset();

  if (ImGui::BeginTable("InventoryScreen", 3, ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("Canvas");
    ImGui::TableSetupColumn("Tiles");
    ImGui::TableSetupColumn("Palette");
    ImGui::TableHeadersRow();

    ImGui::TableNextColumn();
    screen_canvas_.DrawBackground();
    screen_canvas_.DrawContextMenu();
    screen_canvas_.DrawBitmap(inventory_.Bitmap(), 2, create);
    screen_canvas_.DrawGrid(32.0f);
    screen_canvas_.DrawOverlay();

    ImGui::TableNextColumn();
    tilesheet_canvas_.DrawBackground(ImVec2(128 * 2 + 2, (192 * 2) + 4));
    tilesheet_canvas_.DrawContextMenu();
    tilesheet_canvas_.DrawBitmap(inventory_.Tilesheet(), 2, create);
    tilesheet_canvas_.DrawGrid(16.0f);
    tilesheet_canvas_.DrawOverlay();

    ImGui::TableNextColumn();
    gui::DisplayPalette(palette_, create);

    ImGui::EndTable();
  }
  ImGui::Separator();
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

void ScreenEditor::DrawInventoryToolset() {
  if (ImGui::BeginTable("InventoryToolset", 8, ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0, 0))) {
    ImGui::TableSetupColumn("#drawTool");
    ImGui::TableSetupColumn("#sep1");
    ImGui::TableSetupColumn("#zoomOut");
    ImGui::TableSetupColumn("#zoomIN");
    ImGui::TableSetupColumn("#sep2");
    ImGui::TableSetupColumn("#bg2Tool");
    ImGui::TableSetupColumn("#bg3Tool");
    ImGui::TableSetupColumn("#itemTool");

    BUTTON_COLUMN(ICON_MD_UNDO)
    BUTTON_COLUMN(ICON_MD_REDO)
    TEXT_COLUMN(ICON_MD_MORE_VERT)
    BUTTON_COLUMN(ICON_MD_ZOOM_OUT)
    BUTTON_COLUMN(ICON_MD_ZOOM_IN)
    TEXT_COLUMN(ICON_MD_MORE_VERT)
    BUTTON_COLUMN(ICON_MD_DRAW)
    BUTTON_COLUMN(ICON_MD_BUILD)

    ImGui::EndTable();
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze