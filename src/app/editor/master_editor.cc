#include "master_editor.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_sdlrenderer2.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_internal.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/core/platform/file_dialog.h"
#include "app/editor/dungeon_editor.h"
#include "app/editor/graphics_editor.h"
#include "app/editor/modules/assembly_editor.h"
#include "app/editor/modules/music_editor.h"
#include "app/editor/modules/palette_editor.h"
#include "app/editor/overworld_editor.h"
#include "app/editor/screen_editor.h"
#include "app/editor/sprite_editor.h"
#include "app/emu/emulator.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/pipeline.h"
#include "app/gui/style.h"
#include "app/gui/widgets.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

namespace {

constexpr ImGuiWindowFlags kMainEditorFlags =
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar;

void NewMasterFrame() {
  const ImGuiIO& io = ImGui::GetIO();
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(gui::kZeroPos);
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  ImGui::SetNextWindowSize(dimensions, ImGuiCond_Always);

  if (!ImGui::Begin("##YazeMain", nullptr, kMainEditorFlags)) {
    ImGui::End();
    return;
  }
}

bool BeginCentered(const char* name) {
  ImGuiIO const& io = ImGui::GetIO();
  ImVec2 pos(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
  return ImGui::Begin(name, nullptr, flags);
}

class RecentFilesManager {
 public:
  RecentFilesManager(const std::string& filename) : filename_(filename) {}

  void AddFile(const std::string& filePath) {
    // Add a file to the list, avoiding duplicates
    auto it = std::find(recentFiles_.begin(), recentFiles_.end(), filePath);
    if (it == recentFiles_.end()) {
      recentFiles_.push_back(filePath);
    }
  }

  void Save() {
    std::ofstream file(filename_);
    if (!file.is_open()) {
      return;  // Handle the error appropriately
    }

    for (const auto& filePath : recentFiles_) {
      file << filePath << std::endl;
    }
  }

  void Load() {
    std::ifstream file(filename_);
    if (!file.is_open()) {
      return;  // Handle the error appropriately
    }

    recentFiles_.clear();
    std::string line;
    while (std::getline(file, line)) {
      if (!line.empty()) {
        recentFiles_.push_back(line);
      }
    }
  }

  const std::vector<std::string>& GetRecentFiles() const {
    return recentFiles_;
  }

 private:
  std::string filename_;
  std::vector<std::string> recentFiles_;
};

}  // namespace

using ImGui::BeginMenu;
using ImGui::Checkbox;
using ImGui::MenuItem;
using ImGui::Text;

void MasterEditor::SetupScreen(std::shared_ptr<SDL_Renderer> renderer,
                               std::string filename) {
  sdl_renderer_ = renderer;
  rom()->SetupRenderer(renderer);
  if (!filename.empty()) {
    PRINT_IF_ERROR(rom()->LoadFromFile(filename));
  }
  overworld_editor_.InitializeZeml();
}

namespace {
// Function to switch the active tab in a tab bar
void SetTabBarTab(ImGuiTabBar* tab_bar, ImGuiID tab_id) {
  if (tab_bar == NULL) return;

  // Find the tab item with the specified tab_id
  ImGuiTabItem* tab_item = &tab_bar->Tabs[tab_id];
  tab_item->LastFrameVisible = -1;
  tab_item->LastFrameSelected = -1;
  tab_bar->VisibleTabId = tab_id;
  tab_bar->VisibleTabWasSubmitted = true;
  tab_bar->SelectedTabId = tab_id;
  tab_bar->NextSelectedTabId = tab_id;
  tab_bar->ReorderRequestTabId = tab_id;
  tab_bar->CurrFrameVisible = -1;
}
}  // namespace

absl::Status MasterEditor::Update() {
  NewMasterFrame();

  DrawYazeMenu();
  DrawFileDialog();
  DrawStatusPopup();
  DrawAboutPopup();
  DrawInfoPopup();

  if (rom()->is_loaded() && !rom_assets_loaded_) {
    std::cout << "Loading all graphics data from the game..." << std::endl;
    // Load all of the graphics data from the game.
    RETURN_IF_ERROR(rom()->LoadAllGraphicsData())
    // Initialize overworld graphics, maps, and palettes
    RETURN_IF_ERROR(overworld_editor_.LoadGraphics());
    std::cout << "Graphics data loaded." << std::endl;
    rom_assets_loaded_ = true;
  }

  TAB_BAR("##TabBar")
  auto current_tab_bar = ImGui::GetCurrentContext()->CurrentTabBar;

  if (overworld_editor_.jump_to_tab() == -1) {
    gui::RenderTabItem("Overworld", [&]() {
      current_editor_ = &overworld_editor_;
      status_ = overworld_editor_.Update();
    });
  }

  gui::RenderTabItem("Dungeon", [&]() {
    current_editor_ = &dungeon_editor_;
    status_ = dungeon_editor_.Update();
    if (overworld_editor_.jump_to_tab() != -1) {
      dungeon_editor_.add_room(overworld_editor_.jump_to_tab());
      overworld_editor_.jump_to_tab_ = -1;
    }
  });

  gui::RenderTabItem("Graphics",
                     [&]() { status_ = graphics_editor_.Update(); });
  gui::RenderTabItem("Sprites", [&]() { status_ = sprite_editor_.Update(); });
  gui::RenderTabItem("Palettes", [&]() { status_ = palette_editor_.Update(); });
  gui::RenderTabItem("Screens", [&]() { screen_editor_.Update(); });
  gui::RenderTabItem("Music", [&]() { music_editor_.Update(); });
  END_TAB_BAR()

  ImGui::End();

  return absl::OkStatus();
}

void MasterEditor::DrawFileDialog() {
  gui::FileDialogPipeline("ChooseFileDlgKey", ".sfc,.smc", std::nullopt, [&]() {
    std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
    status_ = rom()->LoadFromFile(filePathName);
    static RecentFilesManager manager("recent_files.txt");

    // Load existing recent files
    manager.Load();

    // Add a new file
    manager.AddFile(filePathName);

    // Save the updated list
    manager.Save();
  });
}

void MasterEditor::DrawStatusPopup() {
  if (!status_.ok()) {
    show_status_ = true;
    prev_status_ = status_;
  }

  if (show_status_ && (BeginCentered("StatusWindow"))) {
    Text("%s", prev_status_.ToString().c_str());
    ImGui::Spacing();
    ImGui::NextColumn();
    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::NewLine();
    ImGui::SameLine(270);
    if (ImGui::Button("OK", gui::kDefaultModalSize)) {
      show_status_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CONTENT_COPY, ImVec2(50, 0))) {
      ImGui::SetClipboardText(prev_status_.ToString().c_str());
    }
    ImGui::End();
  }
}

void MasterEditor::DrawAboutPopup() {
  if (about_) ImGui::OpenPopup("About");
  if (ImGui::BeginPopupModal("About", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Yet Another Zelda3 Editor - v%.2f", core::kYazeVersion);
    Text("Written by: scawful");
    ImGui::Spacing();
    Text("Special Thanks: Zarby89, JaredBrian");
    ImGui::Separator();

    if (ImGui::Button("Close", gui::kDefaultModalSize)) {
      about_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void MasterEditor::DrawInfoPopup() {
  if (rom_info_) ImGui::OpenPopup("ROM Information");
  if (ImGui::BeginPopupModal("ROM Information", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Title: %s", rom()->title());
    Text("ROM Size: %ld", rom()->size());

    if (ImGui::Button("Close", gui::kDefaultModalSize)) {
      rom_info_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void MasterEditor::DrawYazeMenu() {
  static bool show_display_settings = false;
  static bool show_command_line_interface = false;

  MENU_BAR()
  DrawFileMenu();
  DrawEditMenu();
  DrawViewMenu();
  DrawHelpMenu();

  ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x -
                  ImGui::CalcTextSize(ICON_MD_DISPLAY_SETTINGS).x - 150);
  // Modify the style of the button to have no background color
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  if (ImGui::Button(ICON_MD_DISPLAY_SETTINGS)) {
    show_display_settings = !show_display_settings;
  }

  if (ImGui::Button(ICON_MD_TERMINAL)) {
    show_command_line_interface = !show_command_line_interface;
  }
  ImGui::PopStyleColor();

  Text("%s", absl::StrCat("yaze v", core::kYazeVersion).c_str());

  END_MENU_BAR()

  if (show_display_settings) {
    ImGui::Begin("Display Settings", &show_display_settings,
                 ImGuiWindowFlags_None);
    gui::DrawDisplaySettings();
    ImGui::End();
  }

  if (show_command_line_interface) {
    ImGui::Begin("Command Line Interface", &show_command_line_interface,
                 ImGuiWindowFlags_None);
    Text("Enter a command:");
    ImGui::End();
  }
}

void MasterEditor::DrawFileMenu() {
  static bool save_as_menu = false;

  if (BeginMenu("File")) {
    if (MenuItem("Open", "Ctrl+O")) {
      LoadRom();
    }

    if (BeginMenu("Open Recent")) {
      static RecentFilesManager manager("recent_files.txt");
      manager.Load();
      if (manager.GetRecentFiles().empty()) {
        MenuItem("No Recent Files", nullptr, false, false);
      } else {
        for (const auto& filePath : manager.GetRecentFiles()) {
          if (MenuItem(filePath.c_str())) {
            status_ = rom()->LoadFromFile(filePath);
          }
        }
      }
      ImGui::EndMenu();
    }

    MENU_ITEM2("Save", "Ctrl+S") {
      if (rom()->is_loaded()) {
        SaveRom();
      }
    }
    MENU_ITEM("Save As..") { save_as_menu = true; }

    if (rom()->is_loaded()) {
      MENU_ITEM("Reload") { status_ = rom()->Reload(); }
      MENU_ITEM("Close") {
        status_ = rom()->Close();
        rom_assets_loaded_ = false;
      }
    }

    ImGui::Separator();

    if (BeginMenu("Options")) {
      MenuItem("Backup ROM", "", &backup_rom_);
      MenuItem("Save New Auto", "", &save_new_auto_);
      ImGui::Separator();
      if (BeginMenu("Experiment Flags")) {
        if (BeginMenu("Overworld Flags")) {
          Checkbox("Enable Overworld Sprites",
                   &mutable_flags()->overworld.kDrawOverworldSprites);
          ImGui::Separator();
          Checkbox("Save Overworld Maps",
                   &mutable_flags()->overworld.kSaveOverworldMaps);
          Checkbox("Save Overworld Entrances",
                   &mutable_flags()->overworld.kSaveOverworldEntrances);
          Checkbox("Save Overworld Exits",
                   &mutable_flags()->overworld.kSaveOverworldExits);
          Checkbox("Save Overworld Items",
                   &mutable_flags()->overworld.kSaveOverworldItems);
          Checkbox("Save Overworld Properties",
                   &mutable_flags()->overworld.kSaveOverworldProperties);
          ImGui::EndMenu();
        }

        if (BeginMenu("Dungeon Flags")) {
          Checkbox("Draw Dungeon Room Graphics",
                   &mutable_flags()->kDrawDungeonRoomGraphics);
          ImGui::Separator();
          Checkbox("Save Dungeon Maps", &mutable_flags()->kSaveDungeonMaps);
          ImGui::EndMenu();
        }

        if (BeginMenu("Emulator Flags")) {
          Checkbox("Load Audio Device", &mutable_flags()->kLoadAudioDevice);
          ImGui::EndMenu();
        }

        Checkbox("Use built-in file dialog",
                 &mutable_flags()->kNewFileDialogWrapper);
        Checkbox("Enable Console Logging", &mutable_flags()->kLogToConsole);
        Checkbox("Enable Texture Streaming",
                 &mutable_flags()->kLoadTexturesAsStreaming);
        Checkbox("Use Bitmap Manager", &mutable_flags()->kUseBitmapManager);
        Checkbox("Log Instructions to Debugger",
                 &mutable_flags()->kLogInstructions);
        Checkbox("Save All Palettes", &mutable_flags()->kSaveAllPalettes);
        Checkbox("Save Gfx Groups", &mutable_flags()->kSaveGfxGroups);
        Checkbox("Use New ImGui Input", &mutable_flags()->kUseNewImGuiInput);
        ImGui::EndMenu();
      }

      ImGui::EndMenu();
    }

    ImGui::Separator();

    if (MenuItem("Quit", "Ctrl+Q")) {
      quit_ = true;
    }

    ImGui::EndMenu();
  }

  if (save_as_menu) {
    static std::string save_as_filename = "";
    ImGui::Begin("Save As..", &save_as_menu, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::InputText("Filename", &save_as_filename);
    if (ImGui::Button("Save", gui::kDefaultModalSize)) {
      SaveRom();
      save_as_menu = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", gui::kDefaultModalSize)) {
      save_as_menu = false;
    }
    ImGui::End();
  }
}

void MasterEditor::DrawEditMenu() {
  if (BeginMenu("Edit")) {
    MENU_ITEM2("Undo", "Ctrl+Z") { status_ = current_editor_->Undo(); }
    MENU_ITEM2("Redo", "Ctrl+Y") { status_ = current_editor_->Redo(); }
    ImGui::Separator();
    MENU_ITEM2("Cut", "Ctrl+X") { status_ = current_editor_->Cut(); }
    MENU_ITEM2("Copy", "Ctrl+C") { status_ = current_editor_->Copy(); }
    MENU_ITEM2("Paste", "Ctrl+V") { status_ = current_editor_->Paste(); }
    ImGui::Separator();
    MENU_ITEM2("Find", "Ctrl+F") {}
    ImGui::Separator();
    MENU_ITEM("ROM Information") rom_info_ = true;
    ImGui::EndMenu();
  }
}

void MasterEditor::DrawViewMenu() {
  static bool show_imgui_metrics = false;
  static bool show_memory_editor = false;
  static bool show_asm_editor = false;
  static bool show_imgui_demo = false;
  static bool show_memory_viewer = false;
  static bool show_palette_editor = false;
  static bool show_emulator = false;
  static bool show_resource_label_manager = false;

  if (show_emulator) {
    ImGui::Begin("Emulator", &show_emulator, ImGuiWindowFlags_MenuBar);
    emulator_.Run();
    ImGui::End();
  }

  if (show_imgui_metrics) {
    ImGui::ShowMetricsWindow(&show_imgui_metrics);
  }

  if (show_memory_editor) {
    static MemoryEditor mem_edit;
    static MemoryEditor comp_edit;
    static bool show_compare_rom = false;
    static Rom comparison_rom;
    ImGui::Begin("Hex Editor", &show_memory_editor);
    if (ImGui::Button("Compare Rom")) {
      auto file_name = FileDialogWrapper::ShowOpenFileDialog();
      PRINT_IF_ERROR(comparison_rom.LoadFromFile(file_name));
      show_compare_rom = true;
    }

    static uint64_t convert_address = 0;
    gui::InputHex("SNES to PC", (int*)&convert_address, 6, 200.f);
    ImGui::SameLine();
    ImGui::Text("%x", core::SnesToPc(convert_address));

    // mem_edit.DrawWindow("Memory Editor", (void*)&(*rom()), rom()->size());
    BEGIN_TABLE("Memory Comparison", 2, ImGuiTableFlags_Resizable);
    SETUP_COLUMN("Source")
    SETUP_COLUMN("Dest")

    NEXT_COLUMN()
    ImGui::Text("%s", rom()->filename().data());
    mem_edit.DrawContents((void*)&(*rom()), rom()->size());

    NEXT_COLUMN()
    if (show_compare_rom) {
      comp_edit.SetComparisonData((void*)&(*rom()));
      ImGui::BeginGroup();
      ImGui::BeginChild("Comparison ROM");
      ImGui::Text("%s", comparison_rom.filename().data());
      comp_edit.DrawContents((void*)&(comparison_rom), comparison_rom.size());
      ImGui::EndChild();
      ImGui::EndGroup();
    }
    END_TABLE()

    ImGui::End();
  }

  if (show_imgui_demo) {
    ImGui::ShowDemoWindow();
  }

  if (show_asm_editor) {
    assembly_editor_.Update(show_asm_editor);
  }

  if (show_palette_editor) {
    ImGui::Begin("Palette Editor", &show_palette_editor);
    status_ = palette_editor_.Update();
    ImGui::End();
  }

  if (show_memory_viewer) {
    ImGui::Begin("Memory Viewer (ImGui)", &show_memory_viewer);

    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
    static ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    if (auto outer_size = ImVec2(0.0f, TEXT_BASE_HEIGHT * 5.5f);
        ImGui::BeginTable("table1", 3, flags, outer_size)) {
      for (int row = 0; row < 10; row++) {
        ImGui::TableNextRow();
        for (int column = 0; column < 3; column++) {
          ImGui::TableNextColumn();
          Text("Cell %d,%d", column, row);
        }
      }
      ImGui::EndTable();
    }

    ImGui::End();
  }

  if (show_resource_label_manager) {
    rom()->resource_label()->DisplayLabels(&show_resource_label_manager);
  }

  if (BeginMenu("View")) {
    MenuItem("Emulator", nullptr, &show_emulator);
    MenuItem("Memory Viewer", nullptr, &show_memory_viewer);
    ImGui::Separator();
    MenuItem("Resource Label Manager", nullptr, &show_resource_label_manager);
    ImGui::Separator();
    MenuItem("Hex Editor", nullptr, &show_memory_editor);
    MenuItem("Assembly Editor", nullptr, &show_asm_editor);
    MenuItem("Palette Editor", nullptr, &show_palette_editor);
    ImGui::Separator();
    MenuItem("ImGui Demo", nullptr, &show_imgui_demo);
    MenuItem("ImGui Metrics", nullptr, &show_imgui_metrics);
    ImGui::EndMenu();
  }
}

void MasterEditor::DrawHelpMenu() {
  static bool open_rom_help = false;
  static bool open_supported_features = false;
  if (BeginMenu("Help")) {
    if (MenuItem("How to open a ROM")) open_rom_help = true;
    if (MenuItem("Supported Features")) open_supported_features = true;

    if (MenuItem("About")) about_ = true;
    ImGui::EndMenu();
  }

  if (open_supported_features) ImGui::OpenPopup("Supported Features");
  if (ImGui::BeginPopupModal("Supported Features", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Overworld");
    ImGui::BulletText("LW/DW/SW Tilemap Editing");
    ImGui::BulletText("LW/DW/SW Map Properties");
    ImGui::BulletText("Create/Delete/Update Entrances");
    ImGui::BulletText("Create/Delete/Update Exits");
    ImGui::BulletText("Create/Delete/Update Sprites");
    ImGui::BulletText("Create/Delete/Update Items");

    ImGui::Text("Dungeon");
    ImGui::BulletText("View Room Header Properties");
    ImGui::BulletText("View Entrance Properties");

    ImGui::Text("Graphics");
    ImGui::BulletText("View Decompressed Graphics Sheets");
    ImGui::BulletText("View/Update Graphics Groups");

    ImGui::Text("Palettes");
    ImGui::BulletText("View Palette Groups");

    ImGui::Text("Saveable");
    ImGui::BulletText("All Listed Overworld Features");
    ImGui::BulletText("Hex Editor Changes");

    if (ImGui::Button("Close", gui::kDefaultModalSize)) {
      open_supported_features = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (open_rom_help) ImGui::OpenPopup("Open a ROM");
  if (ImGui::BeginPopupModal("Open a ROM", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("File -> Open");
    Text("Select a ROM file to open");
    Text("Supported ROMs (headered or unheadered):");
    Text("The Legend of Zelda: A Link to the Past");
    Text("US Version 1.0");
    Text("JP Version 1.0");

    if (ImGui::Button("Close", gui::kDefaultModalSize)) {
      open_rom_help = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void MasterEditor::LoadRom() {
  if (flags()->kNewFileDialogWrapper) {
    auto file_name = FileDialogWrapper::ShowOpenFileDialog();
    PRINT_IF_ERROR(rom()->LoadFromFile(file_name));
    static RecentFilesManager manager("recent_files.txt");
    manager.Load();
    manager.AddFile(file_name);
    manager.Save();
  } else {
    ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Open ROM",
                                            ".sfc,.smc", ".");
  }
}

void MasterEditor::SaveRom() {
  if (flags()->kSaveDungeonMaps) {
    status_ = screen_editor_.SaveDungeonMaps();
    RETURN_VOID_IF_ERROR(status_);
  }
  if (flags()->overworld.kSaveOverworldMaps) {
    RETURN_VOID_IF_ERROR(
        status_ = overworld_editor_.overworld()->CreateTile32Tilemap());
    status_ = overworld_editor_.overworld()->SaveMap32Tiles();
    RETURN_VOID_IF_ERROR(status_);
    status_ = overworld_editor_.overworld()->SaveMap16Tiles();
    RETURN_VOID_IF_ERROR(status_);
    status_ = overworld_editor_.overworld()->SaveOverworldMaps();
    RETURN_VOID_IF_ERROR(status_);
  }
  if (flags()->overworld.kSaveOverworldEntrances) {
    status_ = overworld_editor_.overworld()->SaveEntrances();
    RETURN_VOID_IF_ERROR(status_);
  }
  if (flags()->overworld.kSaveOverworldExits) {
    status_ = overworld_editor_.overworld()->SaveExits();
    RETURN_VOID_IF_ERROR(status_);
  }
  if (flags()->overworld.kSaveOverworldItems) {
    status_ = overworld_editor_.overworld()->SaveItems();
    RETURN_VOID_IF_ERROR(status_);
  }
  if (flags()->overworld.kSaveOverworldProperties) {
    status_ = overworld_editor_.overworld()->SaveMapProperties();
    RETURN_VOID_IF_ERROR(status_);
  }

  status_ = rom()->SaveToFile(backup_rom_, save_new_auto_);
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
