#include "editor.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/imgui_memory_editor.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "app/core/constants.h"
#include "app/editor/assembly_editor.h"
#include "app/editor/dungeon_editor.h"
#include "app/editor/overworld_editor.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "gui/canvas.h"
#include "gui/icons.h"
#include "gui/input.h"
#include "gui/widgets.h"

namespace yaze {
namespace app {
namespace editor {

Editor::Editor() {
  for (int i = 0; i < 8; i++) {
    current_palette_[i].x = (i * 0.21f);
    current_palette_[i].y = (i * 0.21f);
    current_palette_[i].z = (i * 0.21f);
    current_palette_[i].w = 1.f;
  }
}

Editor::~Editor() {
  for (auto &each : image_cache_) {
    SDL_DestroyTexture(each.second);
  }
  rom_.Close();
}

void Editor::SetupScreen(std::shared_ptr<SDL_Renderer> renderer) {
  sdl_renderer_ = renderer;
  rom_.SetupRenderer(renderer);
}

void Editor::UpdateScreen() {
  const ImGuiIO &io = ImGui::GetIO();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  ImGui::SetNextWindowSize(dimensions, ImGuiCond_Always);

  if (!ImGui::Begin("##YazeMain", nullptr, main_editor_flags_)) {
    ImGui::End();
    return;
  }

  DrawYazeMenu();

  TAB_BAR("##TabBar")
  DrawProjectEditor();
  DrawOverworldEditor();
  DrawDungeonEditor();
  DrawGraphicsEditor();
  DrawSpriteEditor();
  END_TAB_BAR()

  ImGui::End();
}

void Editor::DrawYazeMenu() {
  MENU_BAR()
  DrawFileMenu();
  DrawEditMenu();
  DrawViewMenu();
  DrawHelpMenu();
  END_MENU_BAR()

  if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      rom_.LoadFromFile(filePathName);
      overworld_editor_.SetupROM(rom_);
    }
    ImGuiFileDialog::Instance()->Close();
  }
}

void Editor::DrawFileMenu() const {
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Open", "Ctrl+O")) {
      ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Open ROM",
                                              ".sfc,.smc", ".");
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
      for (int i = 0; i < 10; i++) ImGui::Text("Scrolling Text %d", i);
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
    if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
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

void Editor::DrawViewMenu() {
  static bool show_imgui_metrics = false;
  static bool show_imgui_style_editor = false;
  static bool show_memory_editor = false;
  static bool show_asm_editor = false;
  static bool show_imgui_demo = false;

  if (show_imgui_metrics) {
    ImGui::ShowMetricsWindow(&show_imgui_metrics);
  }

  if (show_memory_editor) {
    static MemoryEditor mem_edit;
    mem_edit.DrawWindow("Memory Editor", (void *)rom_.data(), rom_.getSize());
  }

  if (show_imgui_demo) {
    ImGui::ShowDemoWindow();
  }

  if (show_asm_editor) {
    assembly_editor_.Update();
  }

  if (show_imgui_style_editor) {
    ImGui::Begin("Style Editor (ImGui)", &show_imgui_style_editor);
    ImGui::ShowStyleEditor();
    ImGui::End();
  }

  if (ImGui::BeginMenu("View")) {
    ImGui::MenuItem("HEX Editor", nullptr, &show_memory_editor);
    ImGui::MenuItem("ASM Editor", nullptr, &show_asm_editor);
    ImGui::MenuItem("ImGui Demo", nullptr, &show_imgui_demo);

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

void Editor::DrawHelpMenu() const {
  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem("About")) {
      // insert the about window here
    }
    ImGui::EndMenu();
  }
}

void Editor::DrawGraphicsSheet(int offset) {
  SDL_Surface *surface =
      SDL_CreateRGBSurfaceWithFormat(0, 128, 32, 8, SDL_PIXELFORMAT_INDEX8);
  std::cout << "Drawing surface" << std::endl;
  uchar *sheet_buffer = nullptr;
  for (int i = 0; i < 8; i++) {
    std::cout << "Red value: " << current_palette_[i].x << std::endl;
    std::cout << "Green value: " << current_palette_[i].y << std::endl;
    std::cout << "Blue value: " << current_palette_[i].z << std::endl;
    surface->format->palette->colors[i].r = current_palette_[i].x * 255;
    surface->format->palette->colors[i].g = current_palette_[i].y * 255;
    surface->format->palette->colors[i].b = current_palette_[i].z * 255;
  }

  unsigned int snes_addr = 0;
  unsigned int pc_addr = 0;
  snes_addr = (unsigned int)((((rom_.data()[0x4F80 + offset]) << 16) |
                              ((rom_.data()[0x505F + offset]) << 8) |
                              ((rom_.data()[0x513E + offset]))));
  pc_addr = core::SnesToPc(snes_addr);
  std::cout << "Decompressing..." << std::endl;
  auto decomp = rom_.Decompress(pc_addr);
  std::cout << "Converting to 8bpp sheet..." << std::endl;
  sheet_buffer = rom_.SNES3bppTo8bppSheet(decomp);
  std::cout << "Assigning pixel data..." << std::endl;
  surface->pixels = sheet_buffer;
  std::cout << "Creating texture from surface..." << std::endl;
  SDL_Texture *sheet_texture = nullptr;
  sheet_texture = SDL_CreateTextureFromSurface(sdl_renderer_.get(), surface);
  image_cache_[offset] = sheet_texture;
  if (sheet_texture == nullptr) {
    std::cout << "Error: " << SDL_GetError() << std::endl;
  }
}

void Editor::DrawProjectEditor() {
  if (ImGui::BeginTabItem("Project")) {
    if (ImGui::BeginTable(
            "##projectTable", 2,
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable)) {
      ImGui::TableSetupColumn("##inputs", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("##outputs");

      ImGui::TableNextColumn();
      ImGui::Text("Title: %s", rom_.getTitle());
      ImGui::Text("ROM Size: %ld", rom_.getSize());
      ImGui::Separator();

      // ----------------------------------------------------------------------

      static bool loaded_image = false;
      static int tilesheet_offset = 0;
      ImGui::Text("Palette:");
      for (int i = 0; i < 8; i++) {
        std::string id = "##PaletteColor" + std::to_string(i);
        ImGui::SameLine();
        ImGui::ColorEdit4(id.c_str(), &current_palette_[i].x,
                          ImGuiColorEditFlags_NoInputs |
                              ImGuiColorEditFlags_DisplayRGB |
                              ImGuiColorEditFlags_DisplayHex);
      }
      ImGui::SetNextItemWidth(100.f);
      ImGui::InputInt("Tilesheet Offset", &tilesheet_offset);
      BASIC_BUTTON("Retrieve gfx") {
        if (rom_.isLoaded()) {
          DrawGraphicsSheet(tilesheet_offset);
          loaded_image = true;
        }
      }

      // ----------------------------------------------------------------------

      ImGui::TableNextColumn();

      static ImVector<ImVec2> points;
      static ImVec2 scrolling(0.0f, 0.0f);
      static bool opt_enable_context_menu = true;
      ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
      auto canvas_sz = ImVec2(
          512 + 1, ImGui::GetContentRegionAvail().y + (tilesheet_offset * 256));
      auto canvas_p1 =
          ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

      // Draw border and background color
      const ImGuiIO &io = ImGui::GetIO();
      ImDrawList *draw_list = ImGui::GetWindowDrawList();
      draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(32, 32, 32, 255));
      draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

      // This will catch our interactions
      ImGui::InvisibleButton(
          "canvas", canvas_sz,
          ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
      const bool is_active = ImGui::IsItemActive();  // Held
      const ImVec2 origin(canvas_p0.x + scrolling.x,
                          canvas_p0.y + scrolling.y);  // Lock scrolled origin
      const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                       io.MousePos.y - origin.y);

      // Pan (we use a zero mouse threshold when there's no context menu)
      const float mouse_threshold_for_pan =
          opt_enable_context_menu ? -1.0f : 0.0f;
      if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right,
                                              mouse_threshold_for_pan)) {
        scrolling.x += io.MouseDelta.x;
        scrolling.y += io.MouseDelta.y;
      }

      // Context menu (under default mouse threshold)
      ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
      if (opt_enable_context_menu && drag_delta.x == 0.0f &&
          drag_delta.y == 0.0f)
        ImGui::OpenPopupOnItemClick("context",
                                    ImGuiPopupFlags_MouseButtonRight);
      if (ImGui::BeginPopup("context")) {
        ImGui::MenuItem("Placeholder");
        ImGui::EndPopup();
      }

      // Draw grid around the canvas
      draw_list->PushClipRect(canvas_p0, canvas_p1, true);

      // Draw the tilesheets loaded from the ROM
      if (loaded_image) {
        for (const auto &[key, value] : image_cache_) {
          int offset = 128 * (key + 1);
          int top_left_y = canvas_p0.y + 2;
          if (key >= 1) {
            top_left_y = canvas_p0.y + 128 * key;
          }
          draw_list->AddImage((void *)value,
                              ImVec2(canvas_p0.x + 2, top_left_y),
                              ImVec2(canvas_p0.x + 512, canvas_p0.y + offset));
        }
      }

      draw_list->PopClipRect();

      ImGui::EndTable();
    }
    ImGui::EndTabItem();
  }
}

void Editor::DrawOverworldEditor() {
  TAB_ITEM("Overworld")
  overworld_editor_.Update();
  END_TAB_ITEM()
}

void Editor::DrawDungeonEditor() {
  TAB_ITEM("Dungeon")
  dungeon_editor_.Update();
  END_TAB_ITEM()
}

void Editor::DrawGraphicsEditor() {
  TAB_ITEM("Graphics")
  END_TAB_ITEM()
}

void Editor::DrawSpriteEditor() {
  TAB_ITEM("Sprites")
  END_TAB_ITEM()
}

}  // namespace editor
}  // namespace app
}  // namespace yaze