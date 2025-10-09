
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
  
  if (!user_settings_) {
    Text("No user settings available");
    return;
  }
  
  Text("%s Editor Behavior Settings", ICON_MD_TUNE);
  Separator();
  
  // Autosave settings
  if (CollapsingHeader(ICON_MD_SAVE " Auto-Save", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (Checkbox("Enable Auto-Save", &user_settings_->prefs().autosave_enabled)) {
      user_settings_->Save();
    }
    
    if (user_settings_->prefs().autosave_enabled) {
      int interval = static_cast<int>(user_settings_->prefs().autosave_interval);
      if (SliderInt("Interval (seconds)", &interval, 60, 600)) {
        user_settings_->prefs().autosave_interval = static_cast<float>(interval);
        user_settings_->Save();
      }
      
      if (Checkbox("Create Backup Before Save", &user_settings_->prefs().backup_before_save)) {
        user_settings_->Save();
      }
    }
  }
  
  // Recent files
  if (CollapsingHeader(ICON_MD_HISTORY " Recent Files")) {
    if (SliderInt("Recent Files Limit", &user_settings_->prefs().recent_files_limit, 5, 50)) {
      user_settings_->Save();
    }
  }
  
  // Editor defaults
  if (CollapsingHeader(ICON_MD_EDIT " Default Editor")) {
    Text("Editor to open on ROM load:");
    const char* editors[] = { "None", "Overworld", "Dungeon", "Graphics" };
    if (Combo("##DefaultEditor", &user_settings_->prefs().default_editor, editors, IM_ARRAYSIZE(editors))) {
      user_settings_->Save();
    }
  }
}

void SettingsEditor::DrawPerformanceSettings() {
  using namespace ImGui;
  
  if (!user_settings_) {
    Text("No user settings available");
    return;
  }
  
  Text("%s Performance Settings", ICON_MD_SPEED);
  Separator();
  
  // Graphics settings
  if (CollapsingHeader(ICON_MD_IMAGE " Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (Checkbox("V-Sync", &user_settings_->prefs().vsync)) {
      user_settings_->Save();
    }
    
    if (SliderInt("Target FPS", &user_settings_->prefs().target_fps, 30, 144)) {
      user_settings_->Save();
    }
  }
  
  // Memory settings
  if (CollapsingHeader(ICON_MD_MEMORY " Memory")) {
    if (SliderInt("Cache Size (MB)", &user_settings_->prefs().cache_size_mb, 128, 2048)) {
      user_settings_->Save();
    }
    
    if (SliderInt("Undo History", &user_settings_->prefs().undo_history_size, 10, 200)) {
      user_settings_->Save();
    }
  }
  
  Separator();
  Text("Current FPS: %.1f", ImGui::GetIO().Framerate);
  Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
}

void SettingsEditor::DrawAIAgentSettings() {
  using namespace ImGui;
  
  if (!user_settings_) {
    Text("No user settings available");
    return;
  }
  
  Text("%s AI Agent Configuration", ICON_MD_SMART_TOY);
  Separator();
  
  // Provider selection
  if (CollapsingHeader(ICON_MD_CLOUD " AI Provider", ImGuiTreeNodeFlags_DefaultOpen)) {
    const char* providers[] = { "Ollama (Local)", "Gemini (Cloud)", "Mock (Testing)" };
    if (Combo("Provider", &user_settings_->prefs().ai_provider, providers, IM_ARRAYSIZE(providers))) {
      user_settings_->Save();
    }
    
    Spacing();
    
    if (user_settings_->prefs().ai_provider == 0) {  // Ollama
      char url_buffer[256];
      strncpy(url_buffer, user_settings_->prefs().ollama_url.c_str(), sizeof(url_buffer) - 1);
      url_buffer[sizeof(url_buffer) - 1] = '\0';
      if (InputText("URL", url_buffer, IM_ARRAYSIZE(url_buffer))) {
        user_settings_->prefs().ollama_url = url_buffer;
        user_settings_->Save();
      }
    } else if (user_settings_->prefs().ai_provider == 1) {  // Gemini
      char key_buffer[128];
      strncpy(key_buffer, user_settings_->prefs().gemini_api_key.c_str(), sizeof(key_buffer) - 1);
      key_buffer[sizeof(key_buffer) - 1] = '\0';
      if (InputText("API Key", key_buffer, IM_ARRAYSIZE(key_buffer), ImGuiInputTextFlags_Password)) {
        user_settings_->prefs().gemini_api_key = key_buffer;
        user_settings_->Save();
      }
    }
  }
  
  // Model parameters
  if (CollapsingHeader(ICON_MD_TUNE " Model Parameters")) {
    if (SliderFloat("Temperature", &user_settings_->prefs().ai_temperature, 0.0f, 2.0f)) {
      user_settings_->Save();
    }
    TextDisabled("Higher = more creative");
    
    if (SliderInt("Max Tokens", &user_settings_->prefs().ai_max_tokens, 256, 8192)) {
      user_settings_->Save();
    }
  }
  
  // Agent behavior
  if (CollapsingHeader(ICON_MD_PSYCHOLOGY " Behavior")) {
    if (Checkbox("Proactive Suggestions", &user_settings_->prefs().ai_proactive)) {
      user_settings_->Save();
    }
    
    if (Checkbox("Auto-Learn Preferences", &user_settings_->prefs().ai_auto_learn)) {
      user_settings_->Save();
    }
    
    if (Checkbox("Enable Vision/Multimodal", &user_settings_->prefs().ai_multimodal)) {
      user_settings_->Save();
    }
  }
  
  // z3ed CLI logging settings
  if (CollapsingHeader(ICON_MD_TERMINAL " CLI Logging", ImGuiTreeNodeFlags_DefaultOpen)) {
    Text("Configure z3ed command-line logging behavior");
    Spacing();
    
    // Log level selection
    const char* log_levels[] = { "Debug (Verbose)", "Info (Normal)", "Warning (Quiet)", "Error (Critical)", "Fatal Only" };
    if (Combo("Log Level", &user_settings_->prefs().log_level, log_levels, IM_ARRAYSIZE(log_levels))) {
      // Apply log level immediately using existing LogManager
      util::LogLevel level;
      switch (user_settings_->prefs().log_level) {
        case 0: level = util::LogLevel::YAZE_DEBUG; break;
        case 1: level = util::LogLevel::INFO; break;
        case 2: level = util::LogLevel::WARNING; break;
        case 3: level = util::LogLevel::ERROR; break;
        case 4: level = util::LogLevel::FATAL; break;
        default: level = util::LogLevel::INFO; break;
      }
      
      // Get current categories
      std::set<std::string> categories;
      if (user_settings_->prefs().log_ai_requests) categories.insert("AI");
      if (user_settings_->prefs().log_rom_operations) categories.insert("ROM");
      if (user_settings_->prefs().log_gui_automation) categories.insert("GUI");
      if (user_settings_->prefs().log_proposals) categories.insert("Proposals");
      
      // Reconfigure with new level
      util::LogManager::instance().configure(level, user_settings_->prefs().log_file_path, categories);
      user_settings_->Save();
      Text("✓ Log level applied");
    }
    TextDisabled("Controls verbosity of YAZE and z3ed output");
    
    Spacing();
    
    // Logging targets
    if (Checkbox("Log to File", &user_settings_->prefs().log_to_file)) {
      if (user_settings_->prefs().log_to_file) {
        // Set default path if empty
        if (user_settings_->prefs().log_file_path.empty()) {
          const char* home = std::getenv("HOME");
          if (home) {
            user_settings_->prefs().log_file_path = std::string(home) + "/.yaze/logs/yaze.log";
          }
        }
        
        // Enable file logging
        std::set<std::string> categories;
        util::LogLevel level = static_cast<util::LogLevel>(user_settings_->prefs().log_level);
        util::LogManager::instance().configure(level, user_settings_->prefs().log_file_path, categories);
      } else {
        // Disable file logging
        std::set<std::string> categories;
        util::LogLevel level = static_cast<util::LogLevel>(user_settings_->prefs().log_level);
        util::LogManager::instance().configure(level, "", categories);
      }
      user_settings_->Save();
    }
    
    if (user_settings_->prefs().log_to_file) {
      Indent();
      char path_buffer[512];
      strncpy(path_buffer, user_settings_->prefs().log_file_path.c_str(), sizeof(path_buffer) - 1);
      path_buffer[sizeof(path_buffer) - 1] = '\0';
      if (InputText("Log File", path_buffer, IM_ARRAYSIZE(path_buffer))) {
        // Update log file path
        user_settings_->prefs().log_file_path = path_buffer;
        std::set<std::string> categories;
        util::LogLevel level = static_cast<util::LogLevel>(user_settings_->prefs().log_level);
        util::LogManager::instance().configure(level, user_settings_->prefs().log_file_path, categories);
        user_settings_->Save();
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
    
    categories_changed |= Checkbox("AI API Requests", &user_settings_->prefs().log_ai_requests);
    categories_changed |= Checkbox("ROM Operations", &user_settings_->prefs().log_rom_operations);
    categories_changed |= Checkbox("GUI Automation", &user_settings_->prefs().log_gui_automation);
    categories_changed |= Checkbox("Proposal Generation", &user_settings_->prefs().log_proposals);
    
    if (categories_changed) {
      // Rebuild category set
      std::set<std::string> categories;
      if (user_settings_->prefs().log_ai_requests) categories.insert("AI");
      if (user_settings_->prefs().log_rom_operations) categories.insert("ROM");
      if (user_settings_->prefs().log_gui_automation) categories.insert("GUI");
      if (user_settings_->prefs().log_proposals) categories.insert("Proposals");
      
      // Reconfigure LogManager
      util::LogLevel level = static_cast<util::LogLevel>(user_settings_->prefs().log_level);
      util::LogManager::instance().configure(level, 
          user_settings_->prefs().log_to_file ? user_settings_->prefs().log_file_path : "", 
          categories);
      user_settings_->Save();
    }
    
    Spacing();
    
    // Quick actions
    if (Button(ICON_MD_DELETE " Clear Logs")) {
      if (user_settings_->prefs().log_to_file && !user_settings_->prefs().log_file_path.empty()) {
        std::filesystem::path path(user_settings_->prefs().log_file_path);
        if (std::filesystem::exists(path)) {
          std::filesystem::remove(path);
          LOG_DEBUG("Settings", "Log file cleared: %s", user_settings_->prefs().log_file_path.c_str());
        }
      }
    }
    SameLine();
    if (Button(ICON_MD_FOLDER_OPEN " Open Log Directory")) {
      if (user_settings_->prefs().log_to_file && !user_settings_->prefs().log_file_path.empty()) {
        std::filesystem::path path(user_settings_->prefs().log_file_path);
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
