
#include "app/editor/system/settings_editor.h"

#include "absl/status/status.h"
#include "app/gui/feature_flags_menu.h"
#include "app/gfx/performance_profiler.h"
#include "app/gui/style.h"
#include "app/gui/icons.h"
#include "app/gui/theme_manager.h"
#include "imgui/imgui.h"
#include "util/log.h"

#include <set>
#include <filesystem>

namespace yaze {
namespace editor {

using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::EndTable;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableSetupColumn;

void SettingsEditor::Initialize() {}

absl::Status SettingsEditor::Load() { 
  gfx::ScopedTimer timer("SettingsEditor::Load");
  return absl::OkStatus(); 
}

absl::Status SettingsEditor::Update() {
  if (BeginTabBar("Settings", ImGuiTabBarFlags_None)) {
    if (BeginTabItem(ICON_MD_SETTINGS " General")) {
      DrawGeneralSettings();
      EndTabItem();
    }
    if (BeginTabItem(ICON_MD_FONT_DOWNLOAD " Font Manager")) {
      gui::DrawFontManager();
      EndTabItem();
    }
    if (BeginTabItem(ICON_MD_KEYBOARD " Keyboard Shortcuts")) {
      DrawKeyboardShortcuts();
      EndTabItem();
    }
    if (BeginTabItem(ICON_MD_PALETTE " Themes")) {
      DrawThemeSettings();
      EndTabItem();
    }
    if (BeginTabItem(ICON_MD_TUNE " Editor Behavior")) {
      DrawEditorBehavior();
      EndTabItem();
    }
    if (BeginTabItem(ICON_MD_SPEED " Performance")) {
      DrawPerformanceSettings();
      EndTabItem();
    }
    if (BeginTabItem(ICON_MD_SMART_TOY " AI Agent")) {
      DrawAIAgentSettings();
      EndTabItem();
    }
    EndTabBar();
  }

  return absl::OkStatus();
}

void SettingsEditor::DrawGeneralSettings() {
  static gui::FlagsMenu flags;

  if (BeginTable("##SettingsTable", 4,
                 ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
                     ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
    TableSetupColumn("System Flags", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Overworld Flags", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Dungeon Flags", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Resource Flags", ImGuiTableColumnFlags_WidthStretch,
                     0.0f);

    TableHeadersRow();

    TableNextColumn();
    flags.DrawSystemFlags();

    TableNextColumn();
    flags.DrawOverworldFlags();

    TableNextColumn();
    flags.DrawDungeonFlags();

    TableNextColumn();
    flags.DrawResourceFlags();

    EndTable();
  }
}

void SettingsEditor::DrawKeyboardShortcuts() {
  ImGui::Text("Keyboard shortcut customization coming soon...");
  ImGui::Separator();
  
  // TODO: Implement keyboard shortcut editor with:
  // - Visual shortcut conflict detection
  // - Import/export shortcut profiles
  // - Search and filter shortcuts
  // - Live editing and rebinding
}

void SettingsEditor::DrawThemeSettings() {
  using namespace ImGui;
  
  auto& theme_manager = gui::ThemeManager::Get();
  
  Text("%s Theme Management", ICON_MD_PALETTE);
  Separator();
  
  // Current theme selection
  Text("Current Theme:");
  SameLine();
  auto current = theme_manager.GetCurrentThemeName();
  TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", current.c_str());
  
  Spacing();
  
  // Available themes grid
  Text("Available Themes:");
  for (const auto& theme_name : theme_manager.GetAvailableThemes()) {
    PushID(theme_name.c_str());
    bool is_current = (theme_name == current);
    
    if (is_current) {
      PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
    }
    
    if (Button(theme_name.c_str(), ImVec2(180, 0))) {
      theme_manager.LoadTheme(theme_name);
    }
    
    if (is_current) {
      PopStyleColor();
      SameLine();
      Text(ICON_MD_CHECK);
    }
    
    PopID();
  }
  
  Separator();
  Spacing();
  
  // Theme operations
  if (CollapsingHeader(ICON_MD_EDIT " Theme Operations")) {
    TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), 
                "Theme import/export coming soon");
  }
}

void SettingsEditor::DrawEditorBehavior() {
  using namespace ImGui;
  
  Text("%s Editor Behavior Settings", ICON_MD_TUNE);
  Separator();
  
  // Autosave settings
  if (CollapsingHeader(ICON_MD_SAVE " Auto-Save", ImGuiTreeNodeFlags_DefaultOpen)) {
    static bool autosave_enabled = true;
    Checkbox("Enable Auto-Save", &autosave_enabled);
    
    if (autosave_enabled) {
      static int autosave_interval = 300;
      SliderInt("Interval (seconds)", &autosave_interval, 60, 600);
      
      static bool backup_before_save = true;
      Checkbox("Create Backup Before Save", &backup_before_save);
    }
  }
  
  // Recent files
  if (CollapsingHeader(ICON_MD_HISTORY " Recent Files")) {
    static int recent_files_limit = 10;
    SliderInt("Recent Files Limit", &recent_files_limit, 5, 50);
  }
  
  // Editor defaults
  if (CollapsingHeader(ICON_MD_EDIT " Default Editor")) {
    Text("Editor to open on ROM load:");
    static int default_editor = 0;
    const char* editors[] = { "None", "Overworld", "Dungeon", "Graphics" };
    Combo("##DefaultEditor", &default_editor, editors, IM_ARRAYSIZE(editors));
  }
}

void SettingsEditor::DrawPerformanceSettings() {
  using namespace ImGui;
  
  Text("%s Performance Settings", ICON_MD_SPEED);
  Separator();
  
  // Graphics settings
  if (CollapsingHeader(ICON_MD_IMAGE " Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
    static bool vsync = true;
    Checkbox("V-Sync", &vsync);
    
    static int target_fps = 60;
    SliderInt("Target FPS", &target_fps, 30, 144);
  }
  
  // Memory settings
  if (CollapsingHeader(ICON_MD_MEMORY " Memory")) {
    static int cache_size = 512;
    SliderInt("Cache Size (MB)", &cache_size, 128, 2048);
    
    static int undo_size = 50;
    SliderInt("Undo History", &undo_size, 10, 200);
  }
  
  Separator();
  Text("Current FPS: %.1f", ImGui::GetIO().Framerate);
  Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
}

void SettingsEditor::DrawAIAgentSettings() {
  using namespace ImGui;
  
  Text("%s AI Agent Configuration", ICON_MD_SMART_TOY);
  Separator();
  
  // Provider selection
  if (CollapsingHeader(ICON_MD_CLOUD " AI Provider", ImGuiTreeNodeFlags_DefaultOpen)) {
    static int provider = 0;
    const char* providers[] = { "Ollama (Local)", "Gemini (Cloud)", "Mock (Testing)" };
    Combo("Provider", &provider, providers, IM_ARRAYSIZE(providers));
    
    Spacing();
    
    if (provider == 0) {  // Ollama
      static char ollama_url[256] = "http://localhost:11434";
      InputText("URL", ollama_url, IM_ARRAYSIZE(ollama_url));
    } else if (provider == 1) {  // Gemini
      static char api_key[128] = "";
      InputText("API Key", api_key, IM_ARRAYSIZE(api_key), ImGuiInputTextFlags_Password);
    }
  }
  
  // Model parameters
  if (CollapsingHeader(ICON_MD_TUNE " Model Parameters")) {
    static float temperature = 0.7f;
    SliderFloat("Temperature", &temperature, 0.0f, 2.0f);
    TextDisabled("Higher = more creative");
    
    static int max_tokens = 2048;
    SliderInt("Max Tokens", &max_tokens, 256, 8192);
  }
  
  // Agent behavior
  if (CollapsingHeader(ICON_MD_PSYCHOLOGY " Behavior")) {
    static bool proactive = true;
    Checkbox("Proactive Suggestions", &proactive);
    
    static bool auto_learn = true;
    Checkbox("Auto-Learn Preferences", &auto_learn);
    
    static bool multimodal = true;
    Checkbox("Enable Vision/Multimodal", &multimodal);
  }
  
  // z3ed CLI logging settings
  if (CollapsingHeader(ICON_MD_TERMINAL " CLI Logging", ImGuiTreeNodeFlags_DefaultOpen)) {
    Text("Configure z3ed command-line logging behavior");
    Spacing();
    
    // Declare all static variables first
    static int log_level = 1;  // 0=Debug, 1=Info, 2=Warning, 3=Error, 4=Fatal
    static bool log_to_file = false;
    static char log_file_path[512] = "";
    static bool log_ai_requests = true;
    static bool log_rom_operations = true;
    static bool log_gui_automation = true;
    static bool log_proposals = true;
    
    // Log level selection
    const char* log_levels[] = { "Debug (Verbose)", "Info (Normal)", "Warning (Quiet)", "Error (Critical)", "Fatal Only" };
    if (Combo("Log Level", &log_level, log_levels, IM_ARRAYSIZE(log_levels))) {
      // Apply log level immediately using existing LogManager
      util::LogLevel level;
      switch (log_level) {
        case 0: level = util::LogLevel::YAZE_DEBUG; break;
        case 1: level = util::LogLevel::INFO; break;
        case 2: level = util::LogLevel::WARNING; break;
        case 3: level = util::LogLevel::ERROR; break;
        case 4: level = util::LogLevel::FATAL; break;
        default: level = util::LogLevel::INFO; break;
      }
      
      // Get current categories
      std::set<std::string> categories;
      if (log_ai_requests) categories.insert("AI");
      if (log_rom_operations) categories.insert("ROM");
      if (log_gui_automation) categories.insert("GUI");
      if (log_proposals) categories.insert("Proposals");
      
      // Reconfigure with new level
      util::LogManager::instance().configure(level, std::string(log_file_path), categories);
      Text("✓ Log level applied");
    }
    TextDisabled("Controls verbosity of YAZE and z3ed output");
    
    Spacing();
    
    // Logging targets
    
    if (Checkbox("Log to File", &log_to_file)) {
      if (log_to_file) {
        // Set default path if empty
        if (strlen(log_file_path) == 0) {
          const char* home = std::getenv("HOME");
          if (home) {
            snprintf(log_file_path, sizeof(log_file_path), "%s/.yaze/logs/yaze.log", home);
          }
        }
        
        // Enable file logging
        std::set<std::string> categories;
        util::LogLevel level = static_cast<util::LogLevel>(log_level);
        util::LogManager::instance().configure(level, std::string(log_file_path), categories);
      } else {
        // Disable file logging
        std::set<std::string> categories;
        util::LogLevel level = static_cast<util::LogLevel>(log_level);
        util::LogManager::instance().configure(level, "", categories);
      }
    }
    
    if (log_to_file) {
      Indent();
      if (InputText("Log File", log_file_path, IM_ARRAYSIZE(log_file_path))) {
        // Update log file path
        std::set<std::string> categories;
        util::LogLevel level = static_cast<util::LogLevel>(log_level);
        util::LogManager::instance().configure(level, std::string(log_file_path), categories);
      }
      
      TextDisabled("Log file path (supports ~ for home directory)");
      Unindent();
    }
    
    Spacing();
    
    // Log filtering
    Text(ICON_MD_FILTER_ALT " Category Filtering");
    Separator();
    TextDisabled("Enable/disable specific log categories");
    Spacing();
    
    bool categories_changed = false;
    
    categories_changed |= Checkbox("AI API Requests", &log_ai_requests);
    categories_changed |= Checkbox("ROM Operations", &log_rom_operations);
    categories_changed |= Checkbox("GUI Automation", &log_gui_automation);
    categories_changed |= Checkbox("Proposal Generation", &log_proposals);
    
    if (categories_changed) {
      // Rebuild category set
      std::set<std::string> categories;
      if (log_ai_requests) categories.insert("AI");
      if (log_rom_operations) categories.insert("ROM");
      if (log_gui_automation) categories.insert("GUI");
      if (log_proposals) categories.insert("Proposals");
      
      // Reconfigure LogManager
      util::LogLevel level = static_cast<util::LogLevel>(log_level);
      util::LogManager::instance().configure(level, log_to_file ? std::string(log_file_path) : "", categories);
    }
    
    Spacing();
    
    // Quick actions
    if (Button(ICON_MD_DELETE " Clear Logs")) {
      if (log_to_file && strlen(log_file_path) > 0) {
        std::filesystem::path path(log_file_path);
        if (std::filesystem::exists(path)) {
          std::filesystem::remove(path);
          LOG_DEBUG("Settings", "Log file cleared: %s", log_file_path);
        }
      }
    }
    SameLine();
    if (Button(ICON_MD_FOLDER_OPEN " Open Log Directory")) {
      if (log_to_file && strlen(log_file_path) > 0) {
        std::filesystem::path path(log_file_path);
        std::filesystem::path dir = path.parent_path();
        
        // Platform-specific command to open directory
#ifdef _WIN32
        std::string cmd = "explorer " + dir.string();
#elif __APPLE__
        std::string cmd = "open " + dir.string();
#else
        std::string cmd = "xdg-open " + dir.string();
#endif
        system(cmd.c_str());
      }
    }
    
    Spacing();
    Separator();
    
    // Log test buttons
    Text(ICON_MD_BUG_REPORT " Test Logging");
    if (Button("Test Debug")) {
      LOG_DEBUG("Settings", "This is a debug message");
    }
    SameLine();
    if (Button("Test Info")) {
      LOG_INFO("Settings", "This is an info message");
    }
    SameLine();
    if (Button("Test Warning")) {
      LOG_WARN("Settings", "This is a warning message");
    }
    SameLine();
    if (Button("Test Error")) {
      LOG_ERROR("Settings", "This is an error message");
    }
  }
}

}  // namespace editor
}  // namespace yaze
