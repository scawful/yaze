#include "app/gui/core/style.h"

#include <algorithm>

#include "app/gui/core/background_renderer.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "app/platform/font_loader.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/file_util.h"
#include "util/log.h"

namespace yaze {
namespace gui {

namespace {
Color ParseColor(const std::string& color) {
  Color result;
  if (color.size() == 7 && color[0] == '#') {
    result.red = std::stoi(color.substr(1, 2), nullptr, 16) / 255.0f;
    result.green = std::stoi(color.substr(3, 2), nullptr, 16) / 255.0f;
    result.blue = std::stoi(color.substr(5, 2), nullptr, 16) / 255.0f;
  } else {
    throw std::invalid_argument("Invalid color format: " + color);
  }
  return result;
}
}  // namespace

void ColorsYaze() {
  ImGuiStyle* style = &ImGui::GetStyle();
  ImVec4* colors = style->Colors;

  style->WindowPadding = ImVec2(10.f, 10.f);
  style->FramePadding = ImVec2(10.f, 2.f);
  style->CellPadding = ImVec2(4.f, 5.f);
  style->ItemSpacing = ImVec2(10.f, 5.f);
  style->ItemInnerSpacing = ImVec2(5.f, 5.f);
  style->TouchExtraPadding = ImVec2(0.f, 0.f);
  style->IndentSpacing = 20.f;
  style->ScrollbarSize = 14.f;
  style->GrabMinSize = 15.f;

  style->WindowBorderSize = 0.f;
  style->ChildBorderSize = 1.f;
  style->PopupBorderSize = 1.f;
  style->FrameBorderSize = 0.f;
  style->TabBorderSize = 0.f;

  style->WindowRounding = 0.f;
  style->ChildRounding = 0.f;
  style->FrameRounding = 5.f;
  style->PopupRounding = 0.f;
  style->ScrollbarRounding = 5.f;

  auto alttpDarkGreen = ImVec4(0.18f, 0.26f, 0.18f, 1.0f);
  auto alttpMidGreen = ImVec4(0.28f, 0.36f, 0.28f, 1.0f);
  auto allttpLightGreen = ImVec4(0.36f, 0.45f, 0.36f, 1.0f);
  auto allttpLightestGreen = ImVec4(0.49f, 0.57f, 0.49f, 1.0f);

  colors[ImGuiCol_MenuBarBg] = alttpDarkGreen;
  colors[ImGuiCol_TitleBg] = alttpMidGreen;

  colors[ImGuiCol_Header] = alttpDarkGreen;
  colors[ImGuiCol_HeaderHovered] = allttpLightGreen;
  colors[ImGuiCol_HeaderActive] = alttpMidGreen;

  colors[ImGuiCol_TitleBgActive] = alttpDarkGreen;
  colors[ImGuiCol_TitleBgCollapsed] = alttpMidGreen;

  colors[ImGuiCol_Tab] = alttpDarkGreen;
  colors[ImGuiCol_TabHovered] = alttpMidGreen;
  colors[ImGuiCol_TabActive] = ImVec4(0.347f, 0.466f, 0.347f, 1.000f);

  colors[ImGuiCol_Button] = alttpMidGreen;
  colors[ImGuiCol_ButtonHovered] = allttpLightestGreen;
  colors[ImGuiCol_ButtonActive] = allttpLightGreen;

  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.36f, 0.45f, 0.36f, 0.60f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.36f, 0.45f, 0.36f, 0.30f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.45f, 0.36f, 0.40f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.36f, 0.45f, 0.36f, 0.60f);

  colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.85f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.11f, 0.14f, 0.92f);
  colors[ImGuiCol_Border] = allttpLightGreen;
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  colors[ImGuiCol_FrameBg] = ImVec4(0.43f, 0.43f, 0.43f, 0.39f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.36f, 0.28f, 0.40f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.36f, 0.28f, 0.69f);

  colors[ImGuiCol_CheckMark] =
      ImVec4(0.26f, 0.59f, 0.98f, 1.00f);  // Solid blue checkmark
  colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.36f, 0.45f, 0.36f, 0.60f);

  colors[ImGuiCol_Separator] = ImVec4(0.50f, 0.50f, 0.50f, 0.60f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.70f, 0.70f, 0.90f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.78f, 0.82f, 1.00f, 0.60f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.78f, 0.82f, 1.00f, 0.90f);

  colors[ImGuiCol_TabUnfocused] =
      ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
  colors[ImGuiCol_TabUnfocusedActive] =
      ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);
  colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
  colors[ImGuiCol_TableHeaderBg] = alttpDarkGreen;
  colors[ImGuiCol_TableBorderStrong] = alttpMidGreen;
  colors[ImGuiCol_TableBorderLight] =
      ImVec4(0.26f, 0.26f, 0.28f, 1.00f);  // Prefer using Alpha=1.0 here
  colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);
  colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
  colors[ImGuiCol_NavHighlight] = colors[ImGuiCol_HeaderHovered];
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
}

void DrawBitmapViewer(const std::vector<gfx::Bitmap>& bitmaps, float scale,
                      int& current_bitmap_id) {
  if (bitmaps.empty()) {
    ImGui::Text("No bitmaps available.");
    return;
  }

  // Display the current bitmap index and total count.
  ImGui::Text("Viewing Bitmap %d / %zu", current_bitmap_id + 1, bitmaps.size());

  // Buttons to navigate through bitmaps.
  if (ImGui::Button("<- Prev")) {
    if (current_bitmap_id > 0) {
      --current_bitmap_id;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Next ->")) {
    if (current_bitmap_id < bitmaps.size() - 1) {
      ++current_bitmap_id;
    }
  }

  // Display the current bitmap.
  const gfx::Bitmap& current_bitmap = bitmaps[current_bitmap_id];
  // Assuming Bitmap has a function to get its texture ID, and width and
  // height.
  ImTextureID tex_id = (ImTextureID)(intptr_t)current_bitmap.texture();
  ImVec2 size(current_bitmap.width() * scale, current_bitmap.height() * scale);
  // ImGui::Image(tex_id, size);

  // Scroll if the image is larger than the display area.
  if (ImGui::BeginChild("BitmapScrollArea", ImVec2(0, 0), false,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
    ImGui::Image(tex_id, size);
    ImGui::EndChild();
  }
}

static const char* const kKeywords[] = {
    "ADC", "AND", "ASL", "BCC", "BCS", "BEQ", "BIT",   "BMI",  "BNE", "BPL",
    "BRA", "BRL", "BVC", "BVS", "CLC", "CLD", "CLI",   "CLV",  "CMP", "CPX",
    "CPY", "DEC", "DEX", "DEY", "EOR", "INC", "INX",   "INY",  "JMP", "JSR",
    "JSL", "LDA", "LDX", "LDY", "LSR", "MVN", "NOP",   "ORA",  "PEA", "PER",
    "PHA", "PHB", "PHD", "PHP", "PHX", "PHY", "PLA",   "PLB",  "PLD", "PLP",
    "PLX", "PLY", "REP", "ROL", "ROR", "RTI", "RTL",   "RTS",  "SBC", "SEC",
    "SEI", "SEP", "STA", "STP", "STX", "STY", "STZ",   "TAX",  "TAY", "TCD",
    "TCS", "TDC", "TRB", "TSB", "TSC", "TSX", "TXA",   "TXS",  "TXY", "TYA",
    "TYX", "WAI", "WDM", "XBA", "XCE", "ORG", "LOROM", "HIROM"};

static const char* const kIdentifiers[] = {
    "abort",   "abs",     "acos",    "asin",     "atan",    "atexit",
    "atof",    "atoi",    "atol",    "ceil",     "clock",   "cosh",
    "ctime",   "div",     "exit",    "fabs",     "floor",   "fmod",
    "getchar", "getenv",  "isalnum", "isalpha",  "isdigit", "isgraph",
    "ispunct", "isspace", "isupper", "kbhit",    "log10",   "log2",
    "log",     "memcmp",  "modf",    "pow",      "putchar", "putenv",
    "puts",    "rand",    "remove",  "rename",   "sinh",    "sqrt",
    "srand",   "strcat",  "strcmp",  "strerror", "time",    "tolower",
    "toupper"};

TextEditor::LanguageDefinition GetAssemblyLanguageDef() {
  TextEditor::LanguageDefinition language_65816;
  for (auto& k : kKeywords)
    language_65816.mKeywords.emplace(k);

  for (auto& k : kIdentifiers) {
    TextEditor::Identifier id;
    id.mDeclaration = "Built-in function";
    language_65816.mIdentifiers.insert(std::make_pair(std::string(k), id));
  }

  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[ \\t]*#[ \\t]*[a-zA-Z_]+", TextEditor::PaletteIndex::Preprocessor));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "L?\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "\\'\\\\?[^\\']\\'", TextEditor::PaletteIndex::CharLiteral));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?",
          TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?[0-9]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "0[0-7]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?",
          TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/"
          "\\;\\,\\.]",
          TextEditor::PaletteIndex::Punctuation));

  language_65816.mCommentStart = "/*";
  language_65816.mCommentEnd = "*/";
  language_65816.mSingleLineComment = ";";

  language_65816.mCaseSensitive = false;
  language_65816.mAutoIndentation = true;

  language_65816.mName = "65816";

  return language_65816;
}

// TODO: Add more display settings to popup windows.
void BeginWindowWithDisplaySettings(const char* id, bool* active,
                                    const ImVec2& size,
                                    ImGuiWindowFlags flags) {
  ImGuiStyle* ref = &ImGui::GetStyle();
  static float childBgOpacity = 0.75f;
  auto color = ref->Colors[ImGuiCol_WindowBg];

  ImGui::PushStyleColor(ImGuiCol_WindowBg, color);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, color);
  ImGui::PushStyleColor(ImGuiCol_Border, color);

  ImGui::Begin(id, active, flags | ImGuiWindowFlags_MenuBar);
  ImGui::BeginMenuBar();
  if (ImGui::BeginMenu("Display Settings")) {
    ImGui::SliderFloat("Child Background Opacity", &childBgOpacity, 0.0f, 1.0f);
    ImGui::EndMenu();
  }
  ImGui::EndMenuBar();
}

void EndWindowWithDisplaySettings() {
  ImGui::End();
  ImGui::PopStyleColor(3);
}

void BeginPadding(int i) {
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(i, i));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(i, i));
}
void EndPadding() {
  EndNoPadding();
}

void BeginNoPadding() {
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
}
void EndNoPadding() {
  ImGui::PopStyleVar(2);
}

void BeginChildWithScrollbar(const char* str_id) {
  // Get available region but ensure minimum size for proper scrolling
  ImVec2 available = ImGui::GetContentRegionAvail();
  if (available.x < 64.0f)
    available.x = 64.0f;
  if (available.y < 64.0f)
    available.y = 64.0f;

  ImGui::BeginChild(str_id, available, true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
}

void BeginChildWithScrollbar(const char* str_id, ImVec2 content_size) {
  // Set content size before beginning child to enable proper scrolling
  if (content_size.x > 0 && content_size.y > 0) {
    ImGui::SetNextWindowContentSize(content_size);
  }

  // Get available region but ensure minimum size for proper scrolling
  ImVec2 available = ImGui::GetContentRegionAvail();
  if (available.x < 64.0f)
    available.x = 64.0f;
  if (available.y < 64.0f)
    available.y = 64.0f;

  ImGui::BeginChild(str_id, available, true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
}

void BeginChildBothScrollbars(int id) {
  ImGuiID child_id = ImGui::GetID((void*)(intptr_t)id);
  ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar |
                        ImGuiWindowFlags_AlwaysHorizontalScrollbar);
}

// Helper functions for table canvas management
void BeginTableCanvas(const char* table_id, int columns, ImVec2 canvas_size) {
  // Use proper sizing for tables containing canvas elements
  ImGuiTableFlags flags =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;

  // If canvas size is specified, use it as minimum size
  ImVec2 outer_size = ImVec2(0, 0);
  if (canvas_size.x > 0 || canvas_size.y > 0) {
    outer_size = canvas_size;
    flags |= ImGuiTableFlags_NoHostExtendY;  // Prevent auto-extending past
                                             // canvas size
  }

  ImGui::BeginTable(table_id, columns, flags, outer_size);
}

void EndTableCanvas() {
  ImGui::EndTable();
}

void SetupCanvasTableColumn(const char* label, float width_ratio) {
  if (width_ratio > 0) {
    ImGui::TableSetupColumn(label, ImGuiTableColumnFlags_WidthStretch,
                            width_ratio);
  } else {
    ImGui::TableSetupColumn(label, ImGuiTableColumnFlags_WidthStretch);
  }
}

void BeginCanvasTableCell(ImVec2 min_size) {
  ImGui::TableNextColumn();

  // Ensure minimum size for canvas cells
  if (min_size.x > 0 || min_size.y > 0) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 actual_size =
        ImVec2(std::max(avail.x, min_size.x), std::max(avail.y, min_size.y));

    // Reserve space for the canvas
    ImGui::Dummy(actual_size);
    // ImGui::SetCursorPos(ImGui::GetCursorPos() - actual_size); // Reset cursor
    // for drawing
  }
}

void DrawDisplaySettings(ImGuiStyle* ref) {
  // You can pass in a reference ImGuiStyle structure to compare to, revert to
  // and save to (without a reference style pointer, we will use one compared
  // locally as a reference)
  ImGuiStyle& style = ImGui::GetStyle();
  static ImGuiStyle ref_saved_style;

  // Default to using internal storage as reference
  static bool init = true;
  if (init && ref == NULL)
    ref_saved_style = style;
  init = false;
  if (ref == NULL)
    ref = &ref_saved_style;

  ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.50f);

  // Enhanced theme management section
  if (ImGui::CollapsingHeader("Theme Management",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    auto& theme_manager = ThemeManager::Get();
    static bool show_theme_selector = false;
    static bool show_theme_editor = false;

    ImGui::Text("%s Current Theme:", ICON_MD_PALETTE);
    ImGui::SameLine();

    std::string current_theme_name = theme_manager.GetCurrentThemeName();
    bool is_classic_active = (current_theme_name == "Classic YAZE");

    // Current theme display with color preview
    if (is_classic_active) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s",
                         current_theme_name.c_str());
    } else {
      ImGui::Text("%s", current_theme_name.c_str());
    }

    // Theme color preview
    auto current_theme = theme_manager.GetCurrentTheme();
    ImGui::SameLine();
    ImGui::ColorButton("##primary_preview",
                       gui::ConvertColorToImVec4(current_theme.primary),
                       ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
    ImGui::SameLine();
    ImGui::ColorButton("##secondary_preview",
                       gui::ConvertColorToImVec4(current_theme.secondary),
                       ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
    ImGui::SameLine();
    ImGui::ColorButton("##accent_preview",
                       gui::ConvertColorToImVec4(current_theme.accent),
                       ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));

    ImGui::Spacing();

    // Theme selection table for better organization
    if (ImGui::BeginTable(
            "ThemeSelectionTable", 3,
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoHostExtendY,
            ImVec2(0, 80))) {
      ImGui::TableSetupColumn("Built-in", ImGuiTableColumnFlags_WidthStretch,
                              0.3f);
      ImGui::TableSetupColumn("File Themes", ImGuiTableColumnFlags_WidthStretch,
                              0.4f);
      ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch,
                              0.3f);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();

      // Built-in themes column
      ImGui::TableNextColumn();
      if (is_classic_active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
      }

      if (ImGui::Button("Classic YAZE", ImVec2(-1, 30))) {
        theme_manager.ApplyClassicYazeTheme();
        ref_saved_style = style;
      }

      if (is_classic_active) {
        ImGui::PopStyleColor();
      }

      if (ImGui::Button("Reset ColorsYaze", ImVec2(-1, 30))) {
        gui::ColorsYaze();
        ref_saved_style = style;
      }

      // File themes column
      ImGui::TableNextColumn();
      auto available_themes = theme_manager.GetAvailableThemes();
      const char* current_file_theme = "";

      // Find current file theme for display
      for (const auto& theme_name : available_themes) {
        if (theme_name == current_theme_name) {
          current_file_theme = theme_name.c_str();
          break;
        }
      }

      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##FileThemes", current_file_theme)) {
        for (const auto& theme_name : available_themes) {
          bool is_selected = (theme_name == current_theme_name);
          if (ImGui::Selectable(theme_name.c_str(), is_selected)) {
            theme_manager.LoadTheme(theme_name);
            ref_saved_style = style;
          }
        }
        ImGui::EndCombo();
      }

      if (ImGui::Button("Refresh Themes", ImVec2(-1, 30))) {
        theme_manager.RefreshAvailableThemes();
      }

      // Actions column
      ImGui::TableNextColumn();
      if (ImGui::Button("Theme Selector", ImVec2(-1, 30))) {
        show_theme_selector = true;
      }

      if (ImGui::Button("Theme Editor", ImVec2(-1, 30))) {
        show_theme_editor = true;
      }

      ImGui::EndTable();
    }

    // Show theme dialogs
    if (show_theme_selector) {
      theme_manager.ShowThemeSelector(&show_theme_selector);
    }

    if (show_theme_editor) {
      theme_manager.ShowSimpleThemeEditor(&show_theme_editor);
    }
  }

  ImGui::Separator();

  // Background effects settings
  auto& bg_renderer = gui::BackgroundRenderer::Get();
  bg_renderer.DrawSettingsUI();

  ImGui::Separator();

  if (ImGui::ShowStyleSelector("Colors##Selector"))
    ref_saved_style = style;
  ImGui::ShowFontSelector("Fonts##Selector");

  // Simplified Settings (expose floating-pointer border sizes as boolean
  // representing 0.0f or 1.0f)
  if (ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f,
                         "%.0f"))
    style.GrabRounding = style.FrameRounding;  // Make GrabRounding always the
                                               // same value as FrameRounding
  {
    bool border = (style.WindowBorderSize > 0.0f);
    if (ImGui::Checkbox("WindowBorder", &border)) {
      style.WindowBorderSize = border ? 1.0f : 0.0f;
    }
  }
  ImGui::SameLine();
  {
    bool border = (style.FrameBorderSize > 0.0f);
    if (ImGui::Checkbox("FrameBorder", &border)) {
      style.FrameBorderSize = border ? 1.0f : 0.0f;
    }
  }
  ImGui::SameLine();
  {
    bool border = (style.PopupBorderSize > 0.0f);
    if (ImGui::Checkbox("PopupBorder", &border)) {
      style.PopupBorderSize = border ? 1.0f : 0.0f;
    }
  }

  // Save/Revert button
  if (ImGui::Button("Save Ref"))
    *ref = ref_saved_style = style;
  ImGui::SameLine();
  if (ImGui::Button("Revert Ref"))
    style = *ref;
  ImGui::SameLine();

  ImGui::Separator();

  if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
    if (ImGui::BeginTabItem("Sizes")) {
      ImGui::SeparatorText("Main");
      ImGui::SliderFloat2("WindowPadding", (float*)&style.WindowPadding, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderFloat2("FramePadding", (float*)&style.FramePadding, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderFloat2("ItemSpacing", (float*)&style.ItemSpacing, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderFloat2("ItemInnerSpacing", (float*)&style.ItemInnerSpacing,
                          0.0f, 20.0f, "%.0f");
      ImGui::SliderFloat2("TouchExtraPadding", (float*)&style.TouchExtraPadding,
                          0.0f, 10.0f, "%.0f");
      ImGui::SliderFloat("IndentSpacing", &style.IndentSpacing, 0.0f, 30.0f,
                         "%.0f");
      ImGui::SliderFloat("ScrollbarSize", &style.ScrollbarSize, 1.0f, 20.0f,
                         "%.0f");
      ImGui::SliderFloat("GrabMinSize", &style.GrabMinSize, 1.0f, 20.0f,
                         "%.0f");

      ImGui::SeparatorText("Borders");
      ImGui::SliderFloat("WindowBorderSize", &style.WindowBorderSize, 0.0f,
                         1.0f, "%.0f");
      ImGui::SliderFloat("ChildBorderSize", &style.ChildBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("PopupBorderSize", &style.PopupBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("FrameBorderSize", &style.FrameBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("TabBorderSize", &style.TabBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("TabBarBorderSize", &style.TabBarBorderSize, 0.0f,
                         2.0f, "%.0f");

      ImGui::SeparatorText("Rounding");
      ImGui::SliderFloat("WindowRounding", &style.WindowRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("ChildRounding", &style.ChildRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("PopupRounding", &style.PopupRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("ScrollbarRounding", &style.ScrollbarRounding, 0.0f,
                         12.0f, "%.0f");
      ImGui::SliderFloat("GrabRounding", &style.GrabRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("TabRounding", &style.TabRounding, 0.0f, 12.0f,
                         "%.0f");

      ImGui::SeparatorText("Tables");
      ImGui::SliderFloat2("CellPadding", (float*)&style.CellPadding, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderAngle("TableAngledHeadersAngle",
                         &style.TableAngledHeadersAngle, -50.0f, +50.0f);

      ImGui::SeparatorText("Widgets");
      ImGui::SliderFloat2("WindowTitleAlign", (float*)&style.WindowTitleAlign,
                          0.0f, 1.0f, "%.2f");
      ImGui::Combo("ColorButtonPosition", (int*)&style.ColorButtonPosition,
                   "Left\0Right\0");
      ImGui::SliderFloat2("ButtonTextAlign", (float*)&style.ButtonTextAlign,
                          0.0f, 1.0f, "%.2f");
      ImGui::SameLine();

      ImGui::SliderFloat2("SelectableTextAlign",
                          (float*)&style.SelectableTextAlign, 0.0f, 1.0f,
                          "%.2f");
      ImGui::SameLine();

      ImGui::SliderFloat("SeparatorTextBorderSize",
                         &style.SeparatorTextBorderSize, 0.0f, 10.0f, "%.0f");
      ImGui::SliderFloat2("SeparatorTextAlign",
                          (float*)&style.SeparatorTextAlign, 0.0f, 1.0f,
                          "%.2f");
      ImGui::SliderFloat2("SeparatorTextPadding",
                          (float*)&style.SeparatorTextPadding, 0.0f, 40.0f,
                          "%.0f");
      ImGui::SliderFloat("LogSliderDeadzone", &style.LogSliderDeadzone, 0.0f,
                         12.0f, "%.0f");

      ImGui::SeparatorText("Tooltips");
      for (int n = 0; n < 2; n++)
        if (ImGui::TreeNodeEx(n == 0 ? "HoverFlagsForTooltipMouse"
                                     : "HoverFlagsForTooltipNav")) {
          ImGuiHoveredFlags* p = (n == 0) ? &style.HoverFlagsForTooltipMouse
                                          : &style.HoverFlagsForTooltipNav;
          ImGui::CheckboxFlags("ImGuiHoveredFlags_DelayNone", p,
                               ImGuiHoveredFlags_DelayNone);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_DelayShort", p,
                               ImGuiHoveredFlags_DelayShort);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_DelayNormal", p,
                               ImGuiHoveredFlags_DelayNormal);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_Stationary", p,
                               ImGuiHoveredFlags_Stationary);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_NoSharedDelay", p,
                               ImGuiHoveredFlags_NoSharedDelay);
          ImGui::TreePop();
        }

      ImGui::SeparatorText("Misc");
      ImGui::SliderFloat2("DisplaySafeAreaPadding",
                          (float*)&style.DisplaySafeAreaPadding, 0.0f, 30.0f,
                          "%.0f");
      ImGui::SameLine();

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Colors")) {
      static int output_dest = 0;
      static bool output_only_modified = true;
      if (ImGui::Button("Export")) {
        if (output_dest == 0)
          ImGui::LogToClipboard();
        else
          ImGui::LogToTTY();
        ImGui::LogText("ImVec4* colors = ImGui::GetStyle().Colors;" IM_NEWLINE);
        for (int i = 0; i < ImGuiCol_COUNT; i++) {
          const ImVec4& col = style.Colors[i];
          const char* name = ImGui::GetStyleColorName(i);
          if (!output_only_modified ||
              memcmp(&col, &ref->Colors[i], sizeof(ImVec4)) != 0)
            ImGui::LogText(
                "colors[ImGuiCol_%s]%*s= ImVec4(%.2ff, %.2ff, %.2ff, "
                "%.2ff);" IM_NEWLINE,
                name, 23 - (int)strlen(name), "", col.x, col.y, col.z, col.w);
        }
        ImGui::LogFinish();
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120);
      ImGui::Combo("##output_type", &output_dest, "To Clipboard\0To TTY\0");
      ImGui::SameLine();
      ImGui::Checkbox("Only Modified Colors", &output_only_modified);

      static ImGuiTextFilter filter;
      filter.Draw("Filter colors", ImGui::GetFontSize() * 16);

      static ImGuiColorEditFlags alpha_flags = 0;
      if (ImGui::RadioButton("Opaque",
                             alpha_flags == ImGuiColorEditFlags_None)) {
        alpha_flags = ImGuiColorEditFlags_None;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Alpha",
                             alpha_flags == ImGuiColorEditFlags_AlphaPreview)) {
        alpha_flags = ImGuiColorEditFlags_AlphaPreview;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton(
              "Both", alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf)) {
        alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf;
      }
      ImGui::SameLine();

      ImGui::SetNextWindowSizeConstraints(
          ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 10),
          ImVec2(FLT_MAX, FLT_MAX));
      ImGui::BeginChild("##colors", ImVec2(0, 0), ImGuiChildFlags_Border,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                            ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                            ImGuiWindowFlags_NavFlattened);
      ImGui::PushItemWidth(ImGui::GetFontSize() * -12);
      for (int i = 0; i < ImGuiCol_COUNT; i++) {
        const char* name = ImGui::GetStyleColorName(i);
        if (!filter.PassFilter(name))
          continue;
        ImGui::PushID(i);
        ImGui::ColorEdit4("##color", (float*)&style.Colors[i],
                          ImGuiColorEditFlags_AlphaBar | alpha_flags);
        if (memcmp(&style.Colors[i], &ref->Colors[i], sizeof(ImVec4)) != 0) {
          // Tips: in a real user application, you may want to merge and use
          // an icon font into the main font, so instead of "Save"/"Revert"
          // you'd use icons! Read the FAQ and docs/FONTS.md about using icon
          // fonts. It's really easy and super convenient!
          ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
          if (ImGui::Button("Save")) {
            ref->Colors[i] = style.Colors[i];
          }
          ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
          if (ImGui::Button("Revert")) {
            style.Colors[i] = ref->Colors[i];
          }
        }
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::TextUnformatted(name);
        ImGui::PopID();
      }
      ImGui::PopItemWidth();
      ImGui::EndChild();

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Fonts")) {
      ImGuiIO& io = ImGui::GetIO();
      ImFontAtlas* atlas = io.Fonts;
      ImGui::ShowFontAtlas(atlas);

      // Post-baking font scaling. Note that this is NOT the nice way of
      // scaling fonts, read below. (we enforce hard clamping manually as by
      // default DragFloat/SliderFloat allows CTRL+Click text to get out of
      // bounds).
      const float MIN_SCALE = 0.3f;
      const float MAX_SCALE = 2.0f;

      static float window_scale = 1.0f;
      ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
      if (ImGui::DragFloat(
              "window scale", &window_scale, 0.005f, MIN_SCALE, MAX_SCALE,
              "%.2f",
              ImGuiSliderFlags_AlwaysClamp))  // Scale only this window
        ImGui::SetWindowFontScale(window_scale);
      // Global scale is handled by the caller (PopupManager or
      // SettingsPanel) to enable persistence via UserSettings.
      ImGui::PopItemWidth();

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Rendering")) {
      ImGui::Checkbox("Anti-aliased lines", &style.AntiAliasedLines);
      ImGui::SameLine();

      ImGui::Checkbox("Anti-aliased lines use texture",
                      &style.AntiAliasedLinesUseTex);
      ImGui::SameLine();

      ImGui::Checkbox("Anti-aliased fill", &style.AntiAliasedFill);
      ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
      ImGui::DragFloat("Curve Tessellation Tolerance",
                       &style.CurveTessellationTol, 0.02f, 0.10f, 10.0f,
                       "%.2f");
      if (style.CurveTessellationTol < 0.10f)
        style.CurveTessellationTol = 0.10f;

      // When editing the "Circle Segment Max Error" value, draw a preview of
      // its effect on auto-tessellated circles.
      ImGui::DragFloat("Circle Tessellation Max Error",
                       &style.CircleTessellationMaxError, 0.005f, 0.10f, 5.0f,
                       "%.2f", ImGuiSliderFlags_AlwaysClamp);
      const bool show_samples = ImGui::IsItemActive();
      if (show_samples)
        ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
      if (show_samples && ImGui::BeginTooltip()) {
        ImGui::TextUnformatted("(R = radius, N = number of segments)");
        ImGui::Spacing();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const float min_widget_width = ImGui::CalcTextSize("N: MMM\nR: MMM").x;
        for (int n = 0; n < 8; n++) {
          const float RAD_MIN = 5.0f;
          const float RAD_MAX = 70.0f;
          const float rad =
              RAD_MIN + (RAD_MAX - RAD_MIN) * (float)n / (8.0f - 1.0f);

          ImGui::BeginGroup();

          ImGui::Text("R: %.f\nN: %d", rad,
                      draw_list->_CalcCircleAutoSegmentCount(rad));

          const float canvas_width = std::max(min_widget_width, rad * 2.0f);
          const float offset_x = floorf(canvas_width * 0.5f);
          const float offset_y = floorf(RAD_MAX);

          const ImVec2 p1 = ImGui::GetCursorScreenPos();
          draw_list->AddCircle(ImVec2(p1.x + offset_x, p1.y + offset_y), rad,
                               ImGui::GetColorU32(ImGuiCol_Text));
          ImGui::Dummy(ImVec2(canvas_width, RAD_MAX * 2));

          /*
          const ImVec2 p2 = ImGui::GetCursorScreenPos();
          draw_list->AddCircleFilled(ImVec2(p2.x + offset_x, p2.y + offset_y),
          rad, ImGui::GetColorU32(ImGuiCol_Text));
          ImGui::Dummy(ImVec2(canvas_width, RAD_MAX * 2));
          */

          ImGui::EndGroup();
          ImGui::SameLine();
        }
        ImGui::EndTooltip();
      }
      ImGui::SameLine();

      ImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f,
                       "%.2f");  // Not exposing zero here so user doesn't
                                 // "lose" the UI (zero alpha clips all
                                 // widgets). But application code could have a
                                 // toggle to switch between zero and non-zero.
      ImGui::DragFloat("Disabled Alpha", &style.DisabledAlpha, 0.005f, 0.0f,
                       1.0f, "%.2f");
      ImGui::SameLine();

      ImGui::PopItemWidth();

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::PopItemWidth();
}

void DrawDisplaySettingsForPopup(ImGuiStyle* ref) {
  // Popup-safe version of DrawDisplaySettings without problematic tables
  ImGuiStyle& style = ImGui::GetStyle();
  static ImGuiStyle ref_saved_style;

  // Default to using internal storage as reference
  static bool init = true;
  if (init && ref == NULL)
    ref_saved_style = style;
  init = false;
  if (ref == NULL)
    ref = &ref_saved_style;

  ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.50f);

  // Enhanced theme management section (simplified for popup)
  if (ImGui::CollapsingHeader("Theme Management",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    auto& theme_manager = ThemeManager::Get();

    ImGui::Text("%s Current Theme:", ICON_MD_PALETTE);
    ImGui::SameLine();

    std::string current_theme_name = theme_manager.GetCurrentThemeName();
    bool is_classic_active = (current_theme_name == "Classic YAZE");

    // Current theme display with color preview
    if (is_classic_active) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s",
                         current_theme_name.c_str());
    } else {
      ImGui::Text("%s", current_theme_name.c_str());
    }

    // Theme color preview
    auto current_theme = theme_manager.GetCurrentTheme();
    ImGui::SameLine();
    ImGui::ColorButton("##primary_preview",
                       gui::ConvertColorToImVec4(current_theme.primary),
                       ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
    ImGui::SameLine();
    ImGui::ColorButton("##secondary_preview",
                       gui::ConvertColorToImVec4(current_theme.secondary),
                       ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
    ImGui::SameLine();
    ImGui::ColorButton("##accent_preview",
                       gui::ConvertColorToImVec4(current_theme.accent),
                       ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));

    ImGui::Spacing();

    // Simplified theme selection (no table to avoid popup conflicts)
    if (is_classic_active) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    }

    if (ImGui::Button("Classic YAZE")) {
      theme_manager.ApplyClassicYazeTheme();
      ref_saved_style = style;
    }

    if (is_classic_active) {
      ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset ColorsYaze")) {
      gui::ColorsYaze();
      ref_saved_style = style;
    }

    // File themes dropdown
    auto available_themes = theme_manager.GetAvailableThemes();
    const char* current_file_theme = "";

    // Find current file theme for display
    for (const auto& theme_name : available_themes) {
      if (theme_name == current_theme_name) {
        current_file_theme = theme_name.c_str();
        break;
      }
    }

    ImGui::Text("File Themes:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##FileThemes", current_file_theme)) {
      for (const auto& theme_name : available_themes) {
        bool is_selected = (theme_name == current_theme_name);
        if (ImGui::Selectable(theme_name.c_str(), is_selected)) {
          theme_manager.LoadTheme(theme_name);
          ref_saved_style = style;
        }
      }
      ImGui::EndCombo();
    }

    if (ImGui::Button("Refresh Themes")) {
      theme_manager.RefreshAvailableThemes();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open Theme Editor")) {
      static bool show_theme_editor = true;
      theme_manager.ShowSimpleThemeEditor(&show_theme_editor);
    }
  }

  ImGui::Separator();

  // Background effects settings
  auto& bg_renderer = gui::BackgroundRenderer::Get();
  bg_renderer.DrawSettingsUI();

  ImGui::Separator();

  if (ImGui::ShowStyleSelector("Colors##Selector"))
    ref_saved_style = style;
  ImGui::ShowFontSelector("Fonts##Selector");

  // Quick style controls before the tabbed section
  if (ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f,
                         "%.0f"))
    style.GrabRounding = style.FrameRounding;

  // Border checkboxes (simplified layout)
  bool window_border = (style.WindowBorderSize > 0.0f);
  if (ImGui::Checkbox("WindowBorder", &window_border)) {
    style.WindowBorderSize = window_border ? 1.0f : 0.0f;
  }
  ImGui::SameLine();

  bool frame_border = (style.FrameBorderSize > 0.0f);
  if (ImGui::Checkbox("FrameBorder", &frame_border)) {
    style.FrameBorderSize = frame_border ? 1.0f : 0.0f;
  }
  ImGui::SameLine();

  bool popup_border = (style.PopupBorderSize > 0.0f);
  if (ImGui::Checkbox("PopupBorder", &popup_border)) {
    style.PopupBorderSize = popup_border ? 1.0f : 0.0f;
  }

  // Save/Revert buttons
  if (ImGui::Button("Save Ref"))
    *ref = ref_saved_style = style;
  ImGui::SameLine();
  if (ImGui::Button("Revert Ref"))
    style = *ref;

  ImGui::Separator();

  // Add the comprehensive tabbed settings from the original DrawDisplaySettings
  if (ImGui::BeginTabBar("DisplaySettingsTabs", ImGuiTabBarFlags_None)) {
    if (ImGui::BeginTabItem("Sizes")) {
      ImGui::SeparatorText("Main");
      ImGui::SliderFloat2("WindowPadding", (float*)&style.WindowPadding, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderFloat2("FramePadding", (float*)&style.FramePadding, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderFloat2("ItemSpacing", (float*)&style.ItemSpacing, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderFloat2("ItemInnerSpacing", (float*)&style.ItemInnerSpacing,
                          0.0f, 20.0f, "%.0f");
      ImGui::SliderFloat2("TouchExtraPadding", (float*)&style.TouchExtraPadding,
                          0.0f, 10.0f, "%.0f");
      ImGui::SliderFloat("IndentSpacing", &style.IndentSpacing, 0.0f, 30.0f,
                         "%.0f");
      ImGui::SliderFloat("ScrollbarSize", &style.ScrollbarSize, 1.0f, 20.0f,
                         "%.0f");
      ImGui::SliderFloat("GrabMinSize", &style.GrabMinSize, 1.0f, 20.0f,
                         "%.0f");

      ImGui::SeparatorText("Borders");
      ImGui::SliderFloat("WindowBorderSize", &style.WindowBorderSize, 0.0f,
                         1.0f, "%.0f");
      ImGui::SliderFloat("ChildBorderSize", &style.ChildBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("PopupBorderSize", &style.PopupBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("FrameBorderSize", &style.FrameBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("TabBorderSize", &style.TabBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("TabBarBorderSize", &style.TabBarBorderSize, 0.0f,
                         2.0f, "%.0f");

      ImGui::SeparatorText("Rounding");
      ImGui::SliderFloat("WindowRounding", &style.WindowRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("ChildRounding", &style.ChildRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("PopupRounding", &style.PopupRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("ScrollbarRounding", &style.ScrollbarRounding, 0.0f,
                         12.0f, "%.0f");
      ImGui::SliderFloat("GrabRounding", &style.GrabRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("TabRounding", &style.TabRounding, 0.0f, 12.0f,
                         "%.0f");

      ImGui::SeparatorText("Tables");
      ImGui::SliderFloat2("CellPadding", (float*)&style.CellPadding, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderAngle("TableAngledHeadersAngle",
                         &style.TableAngledHeadersAngle, -50.0f, +50.0f);

      ImGui::SeparatorText("Widgets");
      ImGui::SliderFloat2("WindowTitleAlign", (float*)&style.WindowTitleAlign,
                          0.0f, 1.0f, "%.2f");
      ImGui::Combo("ColorButtonPosition", (int*)&style.ColorButtonPosition,
                   "Left\0Right\0");
      ImGui::SliderFloat2("ButtonTextAlign", (float*)&style.ButtonTextAlign,
                          0.0f, 1.0f, "%.2f");
      ImGui::SameLine();

      ImGui::SliderFloat2("SelectableTextAlign",
                          (float*)&style.SelectableTextAlign, 0.0f, 1.0f,
                          "%.2f");
      ImGui::SameLine();

      ImGui::SliderFloat("SeparatorTextBorderSize",
                         &style.SeparatorTextBorderSize, 0.0f, 10.0f, "%.0f");
      ImGui::SliderFloat2("SeparatorTextAlign",
                          (float*)&style.SeparatorTextAlign, 0.0f, 1.0f,
                          "%.2f");
      ImGui::SliderFloat2("SeparatorTextPadding",
                          (float*)&style.SeparatorTextPadding, 0.0f, 40.0f,
                          "%.0f");
      ImGui::SliderFloat("LogSliderDeadzone", &style.LogSliderDeadzone, 0.0f,
                         12.0f, "%.0f");

      ImGui::SeparatorText("Tooltips");
      for (int n = 0; n < 2; n++)
        if (ImGui::TreeNodeEx(n == 0 ? "HoverFlagsForTooltipMouse"
                                     : "HoverFlagsForTooltipNav")) {
          ImGuiHoveredFlags* p = (n == 0) ? &style.HoverFlagsForTooltipMouse
                                          : &style.HoverFlagsForTooltipNav;
          ImGui::CheckboxFlags("ImGuiHoveredFlags_DelayNone", p,
                               ImGuiHoveredFlags_DelayNone);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_DelayShort", p,
                               ImGuiHoveredFlags_DelayShort);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_DelayNormal", p,
                               ImGuiHoveredFlags_DelayNormal);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_Stationary", p,
                               ImGuiHoveredFlags_Stationary);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_NoSharedDelay", p,
                               ImGuiHoveredFlags_NoSharedDelay);
          ImGui::TreePop();
        }

      ImGui::SeparatorText("Misc");
      ImGui::SliderFloat2("DisplaySafeAreaPadding",
                          (float*)&style.DisplaySafeAreaPadding, 0.0f, 30.0f,
                          "%.0f");
      ImGui::SameLine();

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Colors")) {
      static int output_dest = 0;
      static bool output_only_modified = true;
      if (ImGui::Button("Export")) {
        if (output_dest == 0)
          ImGui::LogToClipboard();
        else
          ImGui::LogToTTY();
        ImGui::LogText("ImVec4* colors = ImGui::GetStyle().Colors;" IM_NEWLINE);
        for (int i = 0; i < ImGuiCol_COUNT; i++) {
          const ImVec4& col = style.Colors[i];
          const char* name = ImGui::GetStyleColorName(i);
          if (!output_only_modified ||
              memcmp(&col, &ref->Colors[i], sizeof(ImVec4)) != 0)
            ImGui::LogText(
                "colors[ImGuiCol_%s]%*s= ImVec4(%.2ff, %.2ff, %.2ff, "
                "%.2ff);" IM_NEWLINE,
                name, 23 - (int)strlen(name), "", col.x, col.y, col.z, col.w);
        }
        ImGui::LogFinish();
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120);
      ImGui::Combo("##output_type", &output_dest, "To Clipboard\0To TTY\0");
      ImGui::SameLine();
      ImGui::Checkbox("Only Modified Colors", &output_only_modified);

      static ImGuiTextFilter filter;
      filter.Draw("Filter colors", ImGui::GetFontSize() * 16);

      static ImGuiColorEditFlags alpha_flags = 0;
      if (ImGui::RadioButton("Opaque",
                             alpha_flags == ImGuiColorEditFlags_None)) {
        alpha_flags = ImGuiColorEditFlags_None;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Alpha",
                             alpha_flags == ImGuiColorEditFlags_AlphaPreview)) {
        alpha_flags = ImGuiColorEditFlags_AlphaPreview;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton(
              "Both", alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf)) {
        alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf;
      }
      ImGui::SameLine();

      ImGui::SetNextWindowSizeConstraints(
          ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 10),
          ImVec2(FLT_MAX, FLT_MAX));
      ImGui::BeginChild("##colors", ImVec2(0, 0), ImGuiChildFlags_Border,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                            ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                            ImGuiWindowFlags_NavFlattened);
      ImGui::PushItemWidth(ImGui::GetFontSize() * -12);
      for (int i = 0; i < ImGuiCol_COUNT; i++) {
        const char* name = ImGui::GetStyleColorName(i);
        if (!filter.PassFilter(name))
          continue;
        ImGui::PushID(i);
        ImGui::ColorEdit4("##color", (float*)&style.Colors[i],
                          ImGuiColorEditFlags_AlphaBar | alpha_flags);
        if (memcmp(&style.Colors[i], &ref->Colors[i], sizeof(ImVec4)) != 0) {
          // Tips: in a real user application, you may want to merge and use
          // an icon font into the main font, so instead of "Save"/"Revert"
          // you'd use icons! Read the FAQ and docs/FONTS.md about using icon
          // fonts. It's really easy and super convenient!
          ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
          if (ImGui::Button("Save")) {
            ref->Colors[i] = style.Colors[i];
          }
          ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
          if (ImGui::Button("Revert")) {
            style.Colors[i] = ref->Colors[i];
          }
        }
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::TextUnformatted(name);
        ImGui::PopID();
      }
      ImGui::PopItemWidth();
      ImGui::EndChild();

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Fonts")) {
      ImGuiIO& io = ImGui::GetIO();
      ImFontAtlas* atlas = io.Fonts;
      ImGui::ShowFontAtlas(atlas);

      // Post-baking font scaling. Note that this is NOT the nice way of
      // scaling fonts, read below. (we enforce hard clamping manually as by
      // default DragFloat/SliderFloat allows CTRL+Click text to get out of
      // bounds).
      const float MIN_SCALE = 0.3f;
      const float MAX_SCALE = 2.0f;

      static float window_scale = 1.0f;
      ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
      if (ImGui::DragFloat(
              "window scale", &window_scale, 0.005f, MIN_SCALE, MAX_SCALE,
              "%.2f",
              ImGuiSliderFlags_AlwaysClamp))  // Scale only this window
        ImGui::SetWindowFontScale(window_scale);
      // Global scale is handled by the caller (PopupManager or
      // SettingsPanel) to enable persistence via UserSettings.
      ImGui::PopItemWidth();

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Rendering")) {
      ImGui::Checkbox("Anti-aliased lines", &style.AntiAliasedLines);
      ImGui::SameLine();

      ImGui::Checkbox("Anti-aliased lines use texture",
                      &style.AntiAliasedLinesUseTex);
      ImGui::SameLine();

      ImGui::Checkbox("Anti-aliased fill", &style.AntiAliasedFill);
      ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
      ImGui::DragFloat("Curve Tessellation Tolerance",
                       &style.CurveTessellationTol, 0.02f, 0.10f, 10.0f,
                       "%.2f");
      if (style.CurveTessellationTol < 0.10f)
        style.CurveTessellationTol = 0.10f;

      // When editing the "Circle Segment Max Error" value, draw a preview of
      // its effect on auto-tessellated circles.
      ImGui::DragFloat("Circle Tessellation Max Error",
                       &style.CircleTessellationMaxError, 0.005f, 0.10f, 5.0f,
                       "%.2f", ImGuiSliderFlags_AlwaysClamp);
      const bool show_samples = ImGui::IsItemActive();
      if (show_samples)
        ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
      if (show_samples && ImGui::BeginTooltip()) {
        ImGui::TextUnformatted("(R = radius, N = number of segments)");
        ImGui::Spacing();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const float min_widget_width = ImGui::CalcTextSize("N: MMM\nR: MMM").x;
        for (int n = 0; n < 8; n++) {
          const float RAD_MIN = 5.0f;
          const float RAD_MAX = 70.0f;
          const float rad =
              RAD_MIN + (RAD_MAX - RAD_MIN) * (float)n / (8.0f - 1.0f);

          ImGui::BeginGroup();

          ImGui::Text("R: %.f\nN: %d", rad,
                      draw_list->_CalcCircleAutoSegmentCount(rad));

          const float canvas_width = std::max(min_widget_width, rad * 2.0f);
          const float offset_x = floorf(canvas_width * 0.5f);
          const float offset_y = floorf(RAD_MAX);

          const ImVec2 p1 = ImGui::GetCursorScreenPos();
          draw_list->AddCircle(ImVec2(p1.x + offset_x, p1.y + offset_y), rad,
                               ImGui::GetColorU32(ImGuiCol_Text));
          ImGui::Dummy(ImVec2(canvas_width, RAD_MAX * 2));

          ImGui::EndGroup();
          ImGui::SameLine();
        }
        ImGui::EndTooltip();
      }
      ImGui::SameLine();

      ImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f,
                       "%.2f");  // Not exposing zero here so user doesn't
                                 // "lose" the UI (zero alpha clips all
                                 // widgets). But application code could have a
                                 // toggle to switch between zero and non-zero.
      ImGui::DragFloat("Disabled Alpha", &style.DisabledAlpha, 0.005f, 0.0f,
                       1.0f, "%.2f");
      ImGui::SameLine();

      ImGui::PopItemWidth();

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::PopItemWidth();
}

void TextWithSeparators(const absl::string_view& text) {
  ImGui::Separator();
  ImGui::Text("%s", text.data());
  ImGui::Separator();
}

void DrawFontManager() {
  ImGuiIO& io = ImGui::GetIO();
  ImFontAtlas* atlas = io.Fonts;

  static ImFont* current_font = atlas->Fonts[0];
  static int current_font_index = 0;
  static int font_size = 16;
  static bool font_selected = false;

  ImGui::Text("Loaded fonts");
  for (const auto& loaded_font : font_registry.fonts) {
    ImGui::Text("%s", loaded_font.font_path);
  }
  ImGui::Separator();

  ImGui::Text("Current Font: %s", current_font->GetDebugName());
  ImGui::Text("Font Size: %d", font_size);

  if (ImGui::BeginCombo("Fonts", current_font->GetDebugName())) {
    for (int i = 0; i < atlas->Fonts.Size; i++) {
      bool is_selected = (current_font == atlas->Fonts[i]);
      if (ImGui::Selectable(atlas->Fonts[i]->GetDebugName(), is_selected)) {
        current_font = atlas->Fonts[i];
        current_font_index = i;
        font_selected = true;
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::Separator();
  if (ImGui::SliderInt("Font Size", &font_size, 8, 32)) {
    current_font->Scale = font_size / 16.0f;
  }
}

}  // namespace gui
}  // namespace yaze
