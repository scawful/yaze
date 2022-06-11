#include "Editor.h"

namespace yaze {
namespace Application {
namespace Editor {

void Editor::UpdateScreen() {
  const ImGuiIO &io = ImGui::GetIO();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  ImGui::SetNextWindowSize(dimensions, ImGuiCond_Always);
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar;

  if (!ImGui::Begin("#yaze", nullptr, flags)) {
    ImGui::End();
    return;
  }

  DrawYazeMenu();

  if (isLoaded) {
    if (!doneLoaded) {
      overworld.Load(rom);
      overworld_texture = &overworld.owactualMapTexture;
      doneLoaded = true;
    }
    // ImGui::Image((void*)(intptr_t)overworld_texture,
    // ImVec2(overworld.overworldMapBitmap->GetWidth(),
    // overworld.overworldMapBitmap->GetHeight()));
  }

  if (ImGui::BeginTabBar("##TabBar")) {
    DrawOverworldEditor();
    DrawDungeonEditor();
    DrawScreenEditor();
    DrawROMInfo();
    ImGui::EndTabBar();
  }
  // ImGui::ShowDemoWindow();

  ImGui::End();
}

void Editor::DrawYazeMenu() {
  if (ImGui::BeginMenuBar()) {
    DrawFileMenu();
    DrawEditMenu();
    DrawViewMenu();
    ImGui::EndMenuBar();
  }

  // display
  if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
    // action if OK
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      rom.LoadFromFile(filePathName);
      isLoaded = true;
    }

    // close
    ImGuiFileDialog::Instance()->Close();
  }
}

void Editor::DrawFileMenu() const {
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Open", "Ctrl+O")) {
      // TODO: Add the ability to open ALTTP ROM
      ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Open ROM",
                                              ".sfc,.smc", ".");
    }
    if (ImGui::BeginMenu("Open Recent")) {
      ImGui::MenuItem("alttp.sfc");
      // TODO: Display recently accessed files here
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Save", "Ctrl+S")) {
      // TODO: Implement this
    }
    if (ImGui::MenuItem("Save As..")) {
      // TODO: Implement this
    }

    ImGui::Separator();

    // TODO: Make these options matter
    if (ImGui::BeginMenu("Options")) {
      static bool enabled = true;
      ImGui::MenuItem("Enabled", "", &enabled);
      ImGui::BeginChild("child", ImVec2(0, 60), true);
      for (int i = 0; i < 10; i++)
        ImGui::Text("Scrolling Text %d", i);
      ImGui::EndChild();
      static float f = 0.5f;
      static int n = 0;
      ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
      ImGui::InputFloat("Input", &f, 0.1f);
      ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }
}

void Editor::DrawEditMenu() const {
  if (ImGui::BeginMenu("Edit")) {
    if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
      // TODO: Implement this
    }
    if (ImGui::MenuItem("Undo", "Ctrl+Y")) {
      // TODO: Implement this
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Cut", "Ctrl+X")) {
      // TODO: Implement this
    }
    if (ImGui::MenuItem("Copy", "Ctrl+C")) {
      // TODO: Implement this
    }
    if (ImGui::MenuItem("Paste", "Ctrl+V")) {
      // TODO: Implement this
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Find", "Ctrl+F")) {
      // TODO: Implement this
    }
    ImGui::EndMenu();
  }
}

void Editor::DrawViewMenu() const {
  static bool show_imgui_metrics = false;
  static bool show_imgui_style_editor = false;
  if (show_imgui_metrics) {
    ImGui::ShowMetricsWindow(&show_imgui_metrics);
  }

  if (show_imgui_style_editor) {
    ImGui::Begin("Style Editor (ImGui)", &show_imgui_style_editor);
    ImGui::ShowStyleEditor();
    ImGui::End();
  }

  if (ImGui::BeginMenu("View")) {
    if (ImGui::BeginMenu("Appearance")) {
      if (ImGui::MenuItem("Fullscreen")) {
      }
      ImGui::EndMenu();
    }

    ImGui::Separator();
    if (ImGui::BeginMenu("GUI Tools")) {
      ImGui::MenuItem("Metrics (ImGui)", nullptr, &show_imgui_metrics);
      ImGui::MenuItem("Style Editor (ImGui)", nullptr,
                      &show_imgui_style_editor);
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }
}

// first step would be to decompress all graphics data from the game
// (in alttp that's easy they're all located in the same location all the
// same sheet size 128x32) have a code that convert PC address to SNES and
// vice-versa

// 1) find the gfx pointers (you could use ZS constant file)
// 2) decompress all the gfx with your lz2 decompressor
// 3) convert the 3bpp snes data into PC 4bpp (probably the hardest part)
// 4) get the tiles32 data
// 5) get the tiles16 data
// 6) get the map32 data (they must be decompressed as well with a lz2
// variant not the same as gfx compression but pretty similar) 7) get the
// gfx data of the map yeah i forgot that one and load 4bpp in a pseudo vram
// and use that to render tiles on screen 8) try to render the tiles on the
// bitmap in black & white to start 9) get the palettes data and try to find
// how they're loaded in the game that's a big puzzle to solve then 9 you'll
// have an overworld map viewer, in less than few hours if are able to
// understand the data quickly
void Editor::DrawOverworldEditor() {
  if (ImGui::BeginTabItem("Overworld")) {
    owEditor.Update();
    ImGui::EndTabItem();
  }
}

void Editor::DrawDungeonEditor() {
  if (ImGui::BeginTabItem("Dungeon")) {
    if (ImGui::BeginTable("DWToolset", 9, toolset_table_flags, ImVec2(0, 0))) {
      ImGui::TableSetupColumn("#undoTool");
      ImGui::TableSetupColumn("#redoTool");
      ImGui::TableSetupColumn("#history");
      ImGui::TableSetupColumn("#separator");
      ImGui::TableSetupColumn("#bg1Tool");
      ImGui::TableSetupColumn("#bg2Tool");
      ImGui::TableSetupColumn("#bg3Tool");
      ImGui::TableSetupColumn("#itemTool");
      ImGui::TableSetupColumn("#spriteTool");

      ImGui::TableNextColumn();
      ImGui::Button(ICON_MD_UNDO);

      ImGui::TableNextColumn();
      ImGui::Button(ICON_MD_REDO);

      ImGui::TableNextColumn();
      ImGui::Button(ICON_MD_MANAGE_HISTORY);

      ImGui::TableNextColumn();
      ImGui::Text(ICON_MD_MORE_VERT);

      ImGui::TableNextColumn();
      ImGui::Button(ICON_MD_FILTER_1);

      ImGui::TableNextColumn();
      ImGui::Button(ICON_MD_FILTER_2);

      ImGui::TableNextColumn();
      ImGui::Button(ICON_MD_FILTER_3);

      ImGui::TableNextColumn();
      ImGui::Button(ICON_MD_GRASS);

      ImGui::TableNextColumn();
      ImGui::Button(ICON_MD_PEST_CONTROL_RODENT);
      ImGui::EndTable();
    }
    ImGui::EndTabItem();
  }
}

void Editor::DrawScreenEditor() {
  if (ImGui::BeginTabItem("Screens")) {
    ImGui::EndTabItem();
  }
}

void Editor::DrawROMInfo() {
  if (ImGui::BeginTabItem("ROM Info")) {
    if (isLoaded) {
      ImGui::Text("Title: %s", rom.getTitle());
      ImGui::Text("Version: %d", rom.getVersion());
      ImGui::Text("ROM Size: %ld", rom.getSize());
    }

    ImGui::EndTabItem();
  }
}

} // namespace Editor
} // namespace Application
} // namespace yaze