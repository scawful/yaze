#include "Editor.h"

namespace yaze {
namespace Application {
namespace Editor {

Editor::Editor() {
  static bool inited = false;
  if (!inited) {
    static const char *const keywords[] = {
        "ADC", "AND", "ASL", "BCC", "BCS", "BEQ",   "BIT",   "BMI",       "BNE",
        "BPL", "BRA", "BRL", "BVC", "BVS", "CLC",   "CLD",   "CLI",       "CLV",
        "CMP", "CPX", "CPY", "DEC", "DEX", "DEY",   "EOR",   "INC",       "INX",
        "INY", "JMP", "JSR", "JSL", "LDA", "LDX",   "LDY",   "LSR",       "MVN",
        "NOP", "ORA", "PEA", "PER", "PHA", "PHB",   "PHD",   "PHP",       "PHX",
        "PHY", "PLA", "PLB", "PLD", "PLP", "PLX",   "PLY",   "REP",       "ROL",
        "ROR", "RTI", "RTL", "RTS", "SBC", "SEC",   "SEI",   "SEP",       "STA",
        "STP", "STX", "STY", "STZ", "TAX", "TAY",   "TCD",   "TCS",       "TDC",
        "TRB", "TSB", "TSC", "TSX", "TXA", "TXS",   "TXY",   "TYA",       "TYX",
        "WAI", "WDM", "XBA", "XCE", "ORG", "LOROM", "HIROM", "NAMESPACE", "DB"};
    for (auto &k : keywords) language65816Def.mKeywords.emplace(k);

    static const char *const identifiers[] = {
        "abort",   "abs",     "acos",    "asin",     "atan",    "atexit",
        "atof",    "atoi",    "atol",    "ceil",     "clock",   "cosh",
        "ctime",   "div",     "exit",    "fabs",     "floor",   "fmod",
        "getchar", "getenv",  "isalnum", "isalpha",  "isdigit", "isgraph",
        "ispunct", "isspace", "isupper", "kbhit",    "log10",   "log2",
        "log",     "memcmp",  "modf",    "pow",      "putchar", "putenv",
        "puts",    "rand",    "remove",  "rename",   "sinh",    "sqrt",
        "srand",   "strcat",  "strcmp",  "strerror", "time",    "tolower",
        "toupper"};
    for (auto &k : identifiers) {
      TextEditor::Identifier id;
      id.mDeclaration = "Built-in function";
      language65816Def.mIdentifiers.insert(std::make_pair(std::string(k), id));
    }

    language65816Def.mTokenRegexStrings.push_back(
        std::make_pair<std::string, TextEditor::PaletteIndex>(
            "[ \\t]*#[ \\t]*[a-zA-Z_]+",
            TextEditor::PaletteIndex::Preprocessor));
    language65816Def.mTokenRegexStrings.push_back(
        std::make_pair<std::string, TextEditor::PaletteIndex>(
            "L?\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
    language65816Def.mTokenRegexStrings.push_back(
        std::make_pair<std::string, TextEditor::PaletteIndex>(
            "\\'\\\\?[^\\']\\'", TextEditor::PaletteIndex::CharLiteral));
    language65816Def.mTokenRegexStrings.push_back(
        std::make_pair<std::string, TextEditor::PaletteIndex>(
            "[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?",
            TextEditor::PaletteIndex::Number));
    language65816Def.mTokenRegexStrings.push_back(
        std::make_pair<std::string, TextEditor::PaletteIndex>(
            "[+-]?[0-9]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
    language65816Def.mTokenRegexStrings.push_back(
        std::make_pair<std::string, TextEditor::PaletteIndex>(
            "0[0-7]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
    language65816Def.mTokenRegexStrings.push_back(
        std::make_pair<std::string, TextEditor::PaletteIndex>(
            "0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?",
            TextEditor::PaletteIndex::Number));
    language65816Def.mTokenRegexStrings.push_back(
        std::make_pair<std::string, TextEditor::PaletteIndex>(
            "[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));
    language65816Def.mTokenRegexStrings.push_back(
        std::make_pair<std::string, TextEditor::PaletteIndex>(
            "[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/"
            "\\;\\,\\.]",
            TextEditor::PaletteIndex::Punctuation));

    language65816Def.mCommentStart = "/*";
    language65816Def.mCommentEnd = "*/";
    language65816Def.mSingleLineComment = ";";

    language65816Def.mCaseSensitive = false;
    language65816Def.mAutoIndentation = true;

    language65816Def.mName = "65816";

    inited = true;
  }
  asm_editor_.SetLanguageDefinition(language65816Def);
  asm_editor_.SetPalette(TextEditor::GetDarkPalette());
}

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

  if (!ImGui::Begin("##YazeMain", nullptr, flags)) {
    ImGui::End();
    return;
  }

  DrawYazeMenu();

  if (ImGui::BeginTabBar("##TabBar")) {
    DrawProjectEditor();
    DrawOverworldEditor();
    DrawDungeonEditor();
    DrawGraphicsEditor();
    DrawSpriteEditor();
    DrawScreenEditor();
    ImGui::EndTabBar();
  }

  ImGui::End();
}

void Editor::DrawYazeMenu() {
  if (ImGui::BeginMenuBar()) {
    DrawFileMenu();
    DrawEditMenu();
    DrawViewMenu();
    DrawHelpMenu();

    ImGui::EndMenuBar();
  }

  // display
  if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
    // action if OK
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      title_ = ImGuiFileDialog::Instance()->GetCurrentFileName();
      rom.LoadFromFile(filePathName);
      overworld_editor_.SetRom(rom);
      rom_data_ = (void *)rom.GetRawData();
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
    mem_edit.DrawWindow("Memory Editor", rom_data_, rom.getSize());
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
    ImGui::MenuItem("HEX Editor", nullptr, &MemoryEditor::Open);
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

void Editor::DrawProjectEditor() {
  if (ImGui::BeginTabItem("Project")) {
    if (rom.isLoaded()) {
      ImGui::Text("Title: %s", rom.getTitle());
      ImGui::Text("Version: %d", rom.getVersion());
      ImGui::Text("ROM Size: %ld", rom.getSize());
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
}  // namespace Application
}  // namespace yaze