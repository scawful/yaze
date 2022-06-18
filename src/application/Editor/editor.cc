#include "editor.h"

#include "Graphics/palette.h"
#include "Graphics/tile.h"

namespace yaze {
namespace application {
namespace Editor {

using namespace Core;

Editor::Editor() {
  for (auto &k : Core::Constants::kKeywords)
    language_65816_.mKeywords.emplace(k);

  for (auto &k : Core::Constants::kIdentifiers) {
    TextEditor::Identifier id;
    id.mDeclaration = "Built-in function";
    language_65816_.mIdentifiers.insert(std::make_pair(std::string(k), id));
  }

  language_65816_.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[ \\t]*#[ \\t]*[a-zA-Z_]+", TextEditor::PaletteIndex::Preprocessor));
  language_65816_.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "L?\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
  language_65816_.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "\\'\\\\?[^\\']\\'", TextEditor::PaletteIndex::CharLiteral));
  language_65816_.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?",
          TextEditor::PaletteIndex::Number));
  language_65816_.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?[0-9]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
  language_65816_.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "0[0-7]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
  language_65816_.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?",
          TextEditor::PaletteIndex::Number));
  language_65816_.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));
  language_65816_.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/"
          "\\;\\,\\.]",
          TextEditor::PaletteIndex::Punctuation));

  language_65816_.mCommentStart = "/*";
  language_65816_.mCommentEnd = "*/";
  language_65816_.mSingleLineComment = ";";

  language_65816_.mCaseSensitive = false;
  language_65816_.mAutoIndentation = true;

  language_65816_.mName = "65816";
  asm_editor_.SetLanguageDefinition(language_65816_);
  asm_editor_.SetPalette(TextEditor::GetDarkPalette());

  current_set_.bits_per_pixel_ = 4;
  current_set_.pc_tiles_location_ = 0x80000;
  current_set_.SNESTilesLocation = 0x0000;
  current_set_.length_ = 28672;
  current_set_.pc_palette_location_ = 906022;
  current_set_.SNESPaletteLocation = 0;
}

void Editor::SetupScreen(std::shared_ptr<SDL_Renderer> renderer) {
  sdl_renderer_ = renderer;
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
  DrawScreenEditor();
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
      rom_data_ = (void *)rom_.GetRawData();
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
    if (ImGui::BeginMenu("Open Recent")) {
      ImGui::MenuItem("alttp.sfc");
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
    mem_edit.DrawWindow("Memory Editor", rom_data_, rom_.getSize());
  }

  if (show_imgui_demo) {
    ImGui::ShowDemoWindow();
  }

  if (show_asm_editor) {
    static bool asm_is_loaded = false;
    auto cpos = asm_editor_.GetCursorPosition();
    static const char *fileToEdit = "assets/bunnyhood.asm";
    if (!asm_is_loaded) {
      std::ifstream t(fileToEdit);
      if (t.good()) {
        std::string str((std::istreambuf_iterator<char>(t)),
                        std::istreambuf_iterator<char>());
        asm_editor_.SetText(str);
      }
      asm_is_loaded = true;
    }

    ImGui::Begin("ASM Editor", &show_asm_editor);
    ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1,
                cpos.mColumn + 1, asm_editor_.GetTotalLines(),
                asm_editor_.IsOverwrite() ? "Ovr" : "Ins",
                asm_editor_.CanUndo() ? "*" : " ",
                asm_editor_.GetLanguageDefinition().mName.c_str(), fileToEdit);

    asm_editor_.Render(fileToEdit);
    ImGui::End();
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
    }
    ImGui::EndMenu();
  }
}

void Editor::DrawSurface() {
  arranged_tiles_ =
      Graphics::TilesPattern::transform(current_set_.tilesPattern, tiles_);
  for (unsigned int j = 0; j < arranged_tiles_.size(); j++) {
    for (unsigned int i = 0; i < arranged_tiles_[0].size(); i++) {
      tile8 tile = arranged_tiles_[j][i];
      // SDL_PIXELFORMAT_RGB888 ?
      SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
          0, 8, 8, SDL_BITSPERPIXEL(4), SDL_PIXELFORMAT_RGB444);
      if (surface == nullptr) {
        SDL_Log("SDL_CreateRGBSurfaceWithFormat() failed: %s", SDL_GetError());
        exit(1);
      }
      SDL_PixelFormat *format = surface->format;
      format->palette = current_palette_.GetSDL_Palette();
      uchar *ptr = (uchar *)surface->pixels;

      for (int k = 0; k < 8; k++) {
        for (int l = 0; l < 8; l++) {
          ptr[k * 8 + l] = tile.data[k * 8 + l];
        }
      }

      SDL_Texture *texture =
          SDL_CreateTextureFromSurface(sdl_renderer_.get(), surface);
      if (texture == nullptr) {
        std::cout << "Error: " << SDL_GetError() << std::endl;
      }
      imagesCache[tile.id] = texture;
    }
  }
}

void Editor::DrawProjectEditor() {
  if (ImGui::BeginTabItem("Project")) {
    if (ImGui::BeginTable("##projectTable", 2,
                          ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableSetupColumn("##inputs");
      ImGui::TableSetupColumn("##outputs");

      ImGui::TableNextColumn();
      ImGui::Text("Title: %s", rom_.getTitle());
      ImGui::Text("Version: %d", rom_.getVersion());
      ImGui::Text("ROM Size: %ld", rom_.getSize());

      ImGui::InputInt("Bits per Pixel", &current_set_.bits_per_pixel_);

      yaze::Gui::InputHex("PC Tile Location", &current_set_.pc_tiles_location_);

      yaze::Gui::InputHex("SNES Tile Location",
                          &current_set_.SNESTilesLocation);

      ImGui::InputInt("Tile Preset Length", &current_set_.length_);

      ImGui::InputInt("PC Palette Location",
                      &current_set_.pc_palette_location_);

      yaze::Gui::InputHex("SNES Palette Location",
                          &current_set_.SNESPaletteLocation);

      BASIC_BUTTON("ExtractTiles") {
        if (rom_.isLoaded()) {
          tiles_ = rom_.ExtractTiles(current_set_);
        }
      }

      BASIC_BUTTON("ExtractPalette") {
        if (rom_.isLoaded()) {
          current_palette_ = rom_.ExtractPalette(current_set_);
        }
      }

      BASIC_BUTTON("BuildSurface") {
        if (rom_.isLoaded()) {
          DrawSurface();
        }
      }

      static int i = 0;
      for (auto &[key, texture] : imagesCache) {
        ImGui::Image((void *)(SDL_Texture *)texture, ImVec2(32, 32));
        if (i != 16) {
          ImGui::SameLine();
        } else if (i == 16) {
          i = 0;
        }
        i++;
      }

      ImGui::TableNextColumn();

      static ImVector<ImVec2> points;
      static ImVec2 scrolling(0.0f, 0.0f);
      static bool opt_enable_context_menu = true;
      static bool opt_enable_grid = true;
      ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
      ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
      ImVec2 canvas_p1 =
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
      const bool is_hovered = ImGui::IsItemHovered();  // Hovered
      const bool is_active = ImGui::IsItemActive();    // Held
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

      // Draw grid + all lines in the canvas
      draw_list->PushClipRect(canvas_p0, canvas_p1, true);
      if (opt_enable_grid) {
        const float GRID_STEP = 64.0f;
        for (float x = fmodf(scrolling.x, GRID_STEP); x < canvas_sz.x;
             x += GRID_STEP)
          draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                             ImVec2(canvas_p0.x + x, canvas_p1.y),
                             IM_COL32(200, 200, 200, 40));
        for (float y = fmodf(scrolling.y, GRID_STEP); y < canvas_sz.y;
             y += GRID_STEP)
          draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                             ImVec2(canvas_p1.x, canvas_p0.y + y),
                             IM_COL32(200, 200, 200, 40));
      }

      if (current_scene_.imagesCache.size() != 0) {
        for (const auto &[key, value] : current_scene_.imagesCache) {
          const float GRID_STEP = 8.0f;
          float x = fmodf(scrolling.x, GRID_STEP);
          float y = fmodf(scrolling.y, GRID_STEP);
          draw_list->AddImage((void *)(SDL_Surface *)value,
                              ImVec2(canvas_p0.x + x, canvas_p0.y),
                              ImVec2(canvas_p0.x + x, canvas_p1.y));
          x += GRID_STEP;
          if (x == 128) {
            x = 0;
            y += GRID_STEP;
          }
        }
      }

      draw_list->PopClipRect();

      ImGui::EndTable();
    }
    ImGui::EndTabItem();
  }
}

void Editor::DrawOverworldEditor() {
  if (ImGui::BeginTabItem("Overworld")) {
    overworld_editor_.Update();
    ImGui::EndTabItem();
  }
}

void Editor::DrawDungeonEditor() {
  if (ImGui::BeginTabItem("Dungeon")) {
    if (ImGui::BeginTable("DWToolset", 9, toolset_table_flags_, ImVec2(0, 0))) {
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

void Editor::DrawGraphicsEditor() {
  if (ImGui::BeginTabItem("Graphics")) {
    ImGui::EndTabItem();
  }
}

void Editor::DrawSpriteEditor() {
  if (ImGui::BeginTabItem("Sprites")) {
    ImGui::EndTabItem();
  }
}

void Editor::DrawScreenEditor() {
  if (ImGui::BeginTabItem("Screens")) {
    ImGui::EndTabItem();
  }
}

}  // namespace Editor
}  // namespace application
}  // namespace yaze