#include "app/editor/ui/settings_panel.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <vector>

#include "app/editor/system/panel_manager.h"
#include "app/editor/system/shortcut_manager.h"
#include "app/gui/app/feature_flags_menu.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "rom/rom.h"
#include "core/patch/asm_patch.h"
#include "core/patch/patch_manager.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/log.h"
#include "util/platform_paths.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

void SettingsPanel::Draw() {
  if (!user_settings_) {
    ImGui::TextDisabled("Settings not available");
    return;
  }

  // Use collapsing headers for sections
  // Default open the General Settings
  if (ImGui::CollapsingHeader(ICON_MD_SETTINGS " General Settings",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Indent();
    DrawGeneralSettings();
    ImGui::Unindent();
    ImGui::Spacing();
  }

  // Add Project Settings section
  if (ImGui::CollapsingHeader(ICON_MD_FOLDER " Project Configuration")) {
    ImGui::Indent();
    DrawProjectSettings();
    ImGui::Unindent();
    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_MD_PALETTE " Appearance")) {
    ImGui::Indent();
    DrawAppearanceSettings();
    ImGui::Unindent();
    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_MD_TUNE " Editor Behavior")) {
    ImGui::Indent();
    DrawEditorBehavior();
    ImGui::Unindent();
    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_MD_SPEED " Performance")) {
    ImGui::Indent();
    DrawPerformanceSettings();
    ImGui::Unindent();
    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_MD_SMART_TOY " AI Agent")) {
    ImGui::Indent();
    DrawAIAgentSettings();
    ImGui::Unindent();
    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_MD_KEYBOARD " Keyboard Shortcuts")) {
    ImGui::Indent();
    DrawKeyboardShortcuts();
    ImGui::Unindent();
    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_MD_EXTENSION " ASM Patches")) {
    ImGui::Indent();
    DrawPatchSettings();
    ImGui::Unindent();
  }
}

void SettingsPanel::DrawGeneralSettings() {
  // Refactored from table to vertical list for sidebar
  static gui::FlagsMenu flags;

  ImGui::TextDisabled("Feature Flags configuration");
  ImGui::Spacing();

  if (ImGui::TreeNode(ICON_MD_FLAG " System Flags")) {
    flags.DrawSystemFlags();
    ImGui::TreePop();
  }

  if (ImGui::TreeNode(ICON_MD_MAP " Overworld Flags")) {
    flags.DrawOverworldFlags();
    ImGui::TreePop();
  }

  if (ImGui::TreeNode(ICON_MD_EXTENSION " ZSCustomOverworld Enable Flags")) {
    flags.DrawZSCustomOverworldFlags(rom_);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode(ICON_MD_CASTLE " Dungeon Flags")) {
    flags.DrawDungeonFlags();
    ImGui::TreePop();
  }

  if (ImGui::TreeNode(ICON_MD_FOLDER_SPECIAL " Resource Flags")) {
    flags.DrawResourceFlags();
    ImGui::TreePop();
  }
}

void SettingsPanel::DrawProjectSettings() {
  if (!project_) {
    ImGui::TextDisabled("No active project.");
    return;
  }

  ImGui::Text("%s Project Info", ICON_MD_INFO);
  ImGui::Separator();
  
  ImGui::Text("Name: %s", project_->name.c_str());
  ImGui::Text("Path: %s", project_->filepath.c_str());
  
  ImGui::Spacing();
  ImGui::Text("%s Paths", ICON_MD_FOLDER_OPEN);
  ImGui::Separator();

  // Output Folder
  std::string output_folder = project_->output_folder;
  if (ImGui::InputText("Output Folder", &output_folder)) {
    project_->output_folder = output_folder;
    project_->Save();
  }

  // Git Repository
  std::string git_repo = project_->git_repository;
  if (ImGui::InputText("Git Repository", &git_repo)) {
    project_->git_repository = git_repo;
    project_->Save();
  }

  ImGui::Spacing();
  ImGui::Text("%s Build", ICON_MD_BUILD);
  ImGui::Separator();

  // Build Target
  std::string build_target = project_->build_target;
  if (ImGui::InputText("Build Target (ROM)", &build_target)) {
    project_->build_target = build_target;
    project_->Save();
  }

  // Symbols File
  std::string symbols_file = project_->symbols_filename;
  if (ImGui::InputText("Symbols File", &symbols_file)) {
    project_->symbols_filename = symbols_file;
    project_->Save();
  }
}

void SettingsPanel::DrawAppearanceSettings() {
  auto& theme_manager = gui::ThemeManager::Get();

  ImGui::Text("%s Theme Management", ICON_MD_PALETTE);
  ImGui::Separator();

  // Current theme selection
  ImGui::Text("Current Theme:");
  ImGui::SameLine();
  auto current = theme_manager.GetCurrentThemeName();
  ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", current.c_str());

  ImGui::Spacing();

  // Available themes list (instead of grid for sidebar)
  ImGui::Text("Available Themes:");
  
  if (ImGui::BeginChild("ThemeList", ImVec2(0, 150), true)) {
    for (const auto& theme_name : theme_manager.GetAvailableThemes()) {
      ImGui::PushID(theme_name.c_str());
      bool is_current = (theme_name == current);

      if (ImGui::Selectable(theme_name.c_str(), is_current)) {
        theme_manager.LoadTheme(theme_name);
      }
      
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  ImGui::Spacing();
  gui::DrawFontManager();

  ImGui::Spacing();
  ImGui::Text("%s Status Bar", ICON_MD_HORIZONTAL_RULE);
  ImGui::Separator();

  bool show_status_bar = user_settings_->prefs().show_status_bar;
  if (ImGui::Checkbox("Show Status Bar", &show_status_bar)) {
    user_settings_->prefs().show_status_bar = show_status_bar;
    user_settings_->Save();
    // Immediately apply to status bar if status_bar_ is available
    if (status_bar_) {
      status_bar_->SetEnabled(show_status_bar);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Display ROM, session, cursor, and zoom info at bottom of window");
  }
}

void SettingsPanel::DrawEditorBehavior() {
  if (!user_settings_) return;

  ImGui::Text("%s Auto-Save", ICON_MD_SAVE);
  ImGui::Separator();
  
  if (ImGui::Checkbox("Enable Auto-Save",
               &user_settings_->prefs().autosave_enabled)) {
    user_settings_->Save();
  }

  if (user_settings_->prefs().autosave_enabled) {
    ImGui::Indent();
    int interval = static_cast<int>(user_settings_->prefs().autosave_interval);
    if (ImGui::SliderInt("Interval (sec)", &interval, 60, 600)) {
      user_settings_->prefs().autosave_interval = static_cast<float>(interval);
      user_settings_->Save();
    }

    if (ImGui::Checkbox("Backup Before Save",
                 &user_settings_->prefs().backup_before_save)) {
      user_settings_->Save();
    }
    ImGui::Unindent();
  }
  
  ImGui::Spacing();
  ImGui::Text("%s Recent Files", ICON_MD_HISTORY);
  ImGui::Separator();
  
  if (ImGui::SliderInt("Limit",
                &user_settings_->prefs().recent_files_limit, 5, 50)) {
    user_settings_->Save();
  }

  ImGui::Spacing();
  ImGui::Text("%s Default Editor", ICON_MD_EDIT);
  ImGui::Separator();
  
  const char* editors[] = {"None", "Overworld", "Dungeon", "Graphics"};
  if (ImGui::Combo("##DefaultEditor", &user_settings_->prefs().default_editor,
            editors, IM_ARRAYSIZE(editors))) {
    user_settings_->Save();
  }

  ImGui::Spacing();
  ImGui::Text("%s Sprite Names", ICON_MD_LABEL);
  ImGui::Separator();
  if (ImGui::Checkbox("Use HMagic sprite names (expanded)", &user_settings_->prefs().prefer_hmagic_sprite_names)) {
    yaze::zelda3::SetPreferHmagicSpriteNames(user_settings_->prefs().prefer_hmagic_sprite_names);
    user_settings_->Save();
  }
}

void SettingsPanel::DrawPerformanceSettings() {
  if (!user_settings_) return;

  ImGui::Text("%s Graphics", ICON_MD_IMAGE);
  ImGui::Separator();

  if (ImGui::Checkbox("V-Sync", &user_settings_->prefs().vsync)) {
    user_settings_->Save();
  }

  if (ImGui::SliderInt("Target FPS", &user_settings_->prefs().target_fps, 30, 144)) {
    user_settings_->Save();
  }

  ImGui::Spacing();
  ImGui::Text("%s Memory", ICON_MD_MEMORY);
  ImGui::Separator();

  if (ImGui::SliderInt("Cache Size (MB)", &user_settings_->prefs().cache_size_mb,
                128, 2048)) {
    user_settings_->Save();
  }

  if (ImGui::SliderInt("Undo History", &user_settings_->prefs().undo_history_size,
                10, 200)) {
    user_settings_->Save();
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Current FPS: %.1f", ImGui::GetIO().Framerate);
  ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
}

void SettingsPanel::DrawAIAgentSettings() {
  if (!user_settings_) return;

  // Provider selection
  ImGui::Text("%s Provider", ICON_MD_CLOUD);
  ImGui::Separator();
  
  const char* providers[] = {"Ollama (Local)", "Gemini (Cloud)", "Mock (Testing)"};
  if (ImGui::Combo("##Provider", &user_settings_->prefs().ai_provider, providers,
            IM_ARRAYSIZE(providers))) {
    user_settings_->Save();
  }

  ImGui::Spacing();

  if (user_settings_->prefs().ai_provider == 0) {  // Ollama
    char url_buffer[256];
    strncpy(url_buffer, user_settings_->prefs().ollama_url.c_str(),
            sizeof(url_buffer) - 1);
    url_buffer[sizeof(url_buffer) - 1] = '\0';
    if (ImGui::InputText("URL", url_buffer, IM_ARRAYSIZE(url_buffer))) {
      user_settings_->prefs().ollama_url = url_buffer;
      user_settings_->Save();
    }
  } else if (user_settings_->prefs().ai_provider == 1) {  // Gemini
    char key_buffer[128];
    strncpy(key_buffer, user_settings_->prefs().gemini_api_key.c_str(),
            sizeof(key_buffer) - 1);
    key_buffer[sizeof(key_buffer) - 1] = '\0';
    if (ImGui::InputText("API Key", key_buffer, IM_ARRAYSIZE(key_buffer),
                  ImGuiInputTextFlags_Password)) {
      user_settings_->prefs().gemini_api_key = key_buffer;
      user_settings_->Save();
    }
  }

  ImGui::Spacing();
  ImGui::Text("%s Parameters", ICON_MD_TUNE);
  ImGui::Separator();

  if (ImGui::SliderFloat("Temperature", &user_settings_->prefs().ai_temperature,
                  0.0f, 2.0f)) {
    user_settings_->Save();
  }
  ImGui::TextDisabled("Higher = more creative");

  if (ImGui::SliderInt("Max Tokens", &user_settings_->prefs().ai_max_tokens, 256,
                8192)) {
    user_settings_->Save();
  }

  ImGui::Spacing();
  ImGui::Text("%s Behavior", ICON_MD_PSYCHOLOGY);
  ImGui::Separator();

  if (ImGui::Checkbox("Proactive Suggestions",
               &user_settings_->prefs().ai_proactive)) {
    user_settings_->Save();
  }

  if (ImGui::Checkbox("Auto-Learn Preferences",
               &user_settings_->prefs().ai_auto_learn)) {
    user_settings_->Save();
  }

  if (ImGui::Checkbox("Enable Vision",
               &user_settings_->prefs().ai_multimodal)) {
    user_settings_->Save();
  }
  
  ImGui::Spacing();
  ImGui::Text("%s Logging", ICON_MD_TERMINAL);
  ImGui::Separator();
  
  const char* log_levels[] = {"Debug", "Info", "Warning", "Error", "Fatal"};
  if (ImGui::Combo("Log Level", &user_settings_->prefs().log_level, log_levels,
            IM_ARRAYSIZE(log_levels))) {
      // Apply log level logic here if needed
      user_settings_->Save();
  }
}

void SettingsPanel::DrawKeyboardShortcuts() {
  if (ImGui::TreeNodeEx(ICON_MD_KEYBOARD " Shortcuts", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::TreeNode("Global Shortcuts")) {
      DrawGlobalShortcuts();
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Editor Shortcuts")) {
      DrawEditorShortcuts();
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Panel Shortcuts")) {
      DrawPanelShortcuts();
      ImGui::TreePop();
    }
    ImGui::TextDisabled("Tip: Use Cmd/Opt labels on macOS or Ctrl/Alt on Windows/Linux. Function keys and symbols (/, -) are supported.");
    ImGui::TreePop();
  }
}

void SettingsPanel::DrawGlobalShortcuts() {
  if (!shortcut_manager_ || !user_settings_) {
    ImGui::TextDisabled("Not available");
    return;
  }

  auto shortcuts = shortcut_manager_->GetShortcutsByScope(Shortcut::Scope::kGlobal);
  if (shortcuts.empty()) {
    ImGui::TextDisabled("No global shortcuts registered.");
    return;
  }

  static std::unordered_map<std::string, std::string> editing;

  for (const auto& sc : shortcuts) {
    auto it = editing.find(sc.name);
    if (it == editing.end()) {
      std::string current = PrintShortcut(sc.keys);
      // Use user override if present
      auto u = user_settings_->prefs().global_shortcuts.find(sc.name);
      if (u != user_settings_->prefs().global_shortcuts.end()) {
        current = u->second;
      }
      editing[sc.name] = current;
    }

    ImGui::PushID(sc.name.c_str());
    ImGui::Text("%s", sc.name.c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    std::string& value = editing[sc.name];
    if (ImGui::InputText("##global", &value,
                         ImGuiInputTextFlags_EnterReturnsTrue |
                             ImGuiInputTextFlags_AutoSelectAll)) {
      auto parsed = ParseShortcut(value);
      if (!parsed.empty() || value.empty()) {
        // Empty string clears the shortcut
        shortcut_manager_->UpdateShortcutKeys(sc.name, parsed);
        if (value.empty()) {
          user_settings_->prefs().global_shortcuts.erase(sc.name);
        } else {
          user_settings_->prefs().global_shortcuts[sc.name] = value;
        }
        user_settings_->Save();
      }
    }
    ImGui::PopID();
  }
}

void SettingsPanel::DrawEditorShortcuts() {
  if (!shortcut_manager_ || !user_settings_) {
    ImGui::TextDisabled("Not available");
    return;
  }

  auto shortcuts = shortcut_manager_->GetShortcutsByScope(Shortcut::Scope::kEditor);
  std::map<std::string, std::vector<Shortcut>> grouped;
  static std::unordered_map<std::string, std::string> editing;
  
  for (const auto& sc : shortcuts) {
    auto pos = sc.name.find(".");
    std::string group = pos != std::string::npos ? sc.name.substr(0, pos) : "general";
    grouped[group].push_back(sc);
  }
  for (const auto& [group, list] : grouped) {
    if (ImGui::TreeNode(group.c_str())) {
      for (const auto& sc : list) {
        ImGui::PushID(sc.name.c_str());
        ImGui::Text("%s", sc.name.c_str());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180);
        std::string& value = editing[sc.name];
        if (value.empty()) {
          value = PrintShortcut(sc.keys);
          // Apply user override if present
          auto u = user_settings_->prefs().editor_shortcuts.find(sc.name);
          if (u != user_settings_->prefs().editor_shortcuts.end()) {
            value = u->second;
          }
        }
        if (ImGui::InputText("##editor", &value, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
          auto parsed = ParseShortcut(value);
          if (!parsed.empty() || value.empty()) {
            shortcut_manager_->UpdateShortcutKeys(sc.name, parsed);
            if (value.empty()) {
              user_settings_->prefs().editor_shortcuts.erase(sc.name);
            } else {
              user_settings_->prefs().editor_shortcuts[sc.name] = value;
            }
            user_settings_->Save();
          }
        }
        ImGui::PopID();
      }
      ImGui::TreePop();
    }
  }
}

void SettingsPanel::DrawPanelShortcuts() {
  if (!panel_manager_ || !user_settings_) {
    ImGui::TextDisabled("Registry not available");
    return;
  }

  // Simplified shortcut editor for sidebar
  auto categories = panel_manager_->GetAllCategories();

  for (const auto& category : categories) {
    if (ImGui::TreeNode(category.c_str())) {
      auto cards = panel_manager_->GetPanelsInCategory(0, category);

      for (const auto& card : cards) {
        ImGui::PushID(card.card_id.c_str());
        
        ImGui::Text("%s %s", card.icon.c_str(), card.display_name.c_str());
        
        std::string current_shortcut;
        auto it = user_settings_->prefs().panel_shortcuts.find(card.card_id);
        if (it != user_settings_->prefs().panel_shortcuts.end()) {
          current_shortcut = it->second;
        } else if (!card.shortcut_hint.empty()) {
          current_shortcut = card.shortcut_hint;
        } else {
          current_shortcut = "None";
        }

        // Display platform-aware label
        std::string display_shortcut = current_shortcut;
        auto parsed = ParseShortcut(current_shortcut);
        if (!parsed.empty()) {
          display_shortcut = PrintShortcut(parsed);
        }

        if (is_editing_shortcut_ && editing_card_id_ == card.card_id) {
            ImGui::SetNextItemWidth(120);
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##Edit", shortcut_edit_buffer_, 
                                 sizeof(shortcut_edit_buffer_), 
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
              if (strlen(shortcut_edit_buffer_) > 0) {
                user_settings_->prefs().panel_shortcuts[card.card_id] = shortcut_edit_buffer_;
              } else {
                user_settings_->prefs().panel_shortcuts.erase(card.card_id);
              }
              user_settings_->Save();
              is_editing_shortcut_ = false;
              editing_card_id_.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_MD_CLOSE)) {
              is_editing_shortcut_ = false;
              editing_card_id_.clear();
            }
        } else {
            if (ImGui::Button(display_shortcut.c_str(), ImVec2(120, 0))) {
                is_editing_shortcut_ = true;
                editing_card_id_ = card.card_id;
                strncpy(shortcut_edit_buffer_, current_shortcut.c_str(), sizeof(shortcut_edit_buffer_) - 1);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click to edit shortcut");
            }
        }
        
        ImGui::PopID();
      }
      
      ImGui::TreePop();
    }
  }
}

void SettingsPanel::DrawPatchSettings() {
  // Load patches on first access
  if (!patches_loaded_) {
    // Try to load from default patches location
    auto patches_dir_status = util::PlatformPaths::FindAsset("patches");
    if (patches_dir_status.ok()) {
      auto status = patch_manager_.LoadPatches(patches_dir_status->string());
      if (status.ok()) {
        patches_loaded_ = true;
        if (!patch_manager_.folders().empty()) {
          selected_folder_ = patch_manager_.folders()[0];
        }
      }
    }
  }

  ImGui::Text("%s ZScream Patch System", ICON_MD_EXTENSION);
  ImGui::Separator();

  if (!patches_loaded_) {
    ImGui::TextDisabled("No patches loaded");
    ImGui::TextDisabled("Place .asm patches in assets/patches/");

    if (ImGui::Button("Browse for Patches Folder...")) {
      // TODO: File browser for patches folder
    }
    return;
  }

  // Status line
  int enabled_count = patch_manager_.GetEnabledPatchCount();
  int total_count = static_cast<int>(patch_manager_.patches().size());
  ImGui::Text("Loaded: %d patches (%d enabled)", total_count, enabled_count);

  ImGui::Spacing();

  // Folder tabs
  if (ImGui::BeginTabBar("##PatchFolders", ImGuiTabBarFlags_FittingPolicyScroll)) {
    for (const auto& folder : patch_manager_.folders()) {
      if (ImGui::BeginTabItem(folder.c_str())) {
        selected_folder_ = folder;
        DrawPatchList(folder);
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Selected patch details
  if (selected_patch_) {
    DrawPatchDetails();
  } else {
    ImGui::TextDisabled("Select a patch to view details");
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Action buttons
  if (ImGui::Button(ICON_MD_CHECK " Apply Patches to ROM")) {
    if (rom_ && rom_->is_loaded()) {
      auto status = patch_manager_.ApplyEnabledPatches(rom_);
      if (!status.ok()) {
        LOG_ERROR("Settings", "Failed to apply patches: %s", status.message());
      } else {
        LOG_INFO("Settings", "Applied %d patches successfully", enabled_count);
      }
    } else {
      LOG_WARN("Settings", "No ROM loaded");
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Apply all enabled patches to the loaded ROM");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE " Save All")) {
    auto status = patch_manager_.SaveAllPatches();
    if (!status.ok()) {
      LOG_ERROR("Settings", "Failed to save patches: %s", status.message());
    }
  }

  if (ImGui::Button(ICON_MD_REFRESH " Reload Patches")) {
    patches_loaded_ = false;
    selected_patch_ = nullptr;
  }
}

void SettingsPanel::DrawPatchList(const std::string& folder) {
  auto patches = patch_manager_.GetPatchesInFolder(folder);

  if (patches.empty()) {
    ImGui::TextDisabled("No patches in this folder");
    return;
  }

  // Use a child region for scrolling
  float available_height = std::min(200.0f, patches.size() * 25.0f + 10.0f);
  if (ImGui::BeginChild("##PatchList", ImVec2(0, available_height), true)) {
    for (auto* patch : patches) {
      ImGui::PushID(patch->filename().c_str());

      bool enabled = patch->enabled();
      if (ImGui::Checkbox("##Enabled", &enabled)) {
        patch->set_enabled(enabled);
      }

      ImGui::SameLine();

      // Highlight selected patch
      bool is_selected = (selected_patch_ == patch);
      if (ImGui::Selectable(patch->name().c_str(), is_selected)) {
        selected_patch_ = patch;
      }

      ImGui::PopID();
    }
  }
  ImGui::EndChild();
}

void SettingsPanel::DrawPatchDetails() {
  if (!selected_patch_) return;

  ImGui::Text("%s %s", ICON_MD_INFO, selected_patch_->name().c_str());

  if (!selected_patch_->author().empty()) {
    ImGui::TextDisabled("by %s", selected_patch_->author().c_str());
  }

  if (!selected_patch_->version().empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled("v%s", selected_patch_->version().c_str());
  }

  // Description
  if (!selected_patch_->description().empty()) {
    ImGui::Spacing();
    ImGui::TextWrapped("%s", selected_patch_->description().c_str());
  }

  // Parameters
  auto& params = selected_patch_->mutable_parameters();
  if (!params.empty()) {
    ImGui::Spacing();
    ImGui::Text("%s Parameters", ICON_MD_TUNE);
    ImGui::Separator();

    for (auto& param : params) {
      DrawParameterWidget(&param);
    }
  }
}

void SettingsPanel::DrawParameterWidget(core::PatchParameter* param) {
  if (!param) return;

  ImGui::PushID(param->define_name.c_str());

  switch (param->type) {
    case core::PatchParameterType::kByte:
    case core::PatchParameterType::kWord:
    case core::PatchParameterType::kLong: {
      int value = param->value;
      const char* format = param->use_decimal ? "%d" : "$%X";

      ImGui::Text("%s", param->display_name.c_str());
      ImGui::SetNextItemWidth(100);
      if (ImGui::InputInt("##Value", &value, 1, 16)) {
        param->value = std::clamp(value, param->min_value, param->max_value);
      }

      // Show range hint
      if (param->min_value != 0 || param->max_value != 0xFF) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%d-%d)", param->min_value, param->max_value);
      }
      break;
    }

    case core::PatchParameterType::kBool: {
      bool checked = (param->value == param->checked_value);
      if (ImGui::Checkbox(param->display_name.c_str(), &checked)) {
        param->value = checked ? param->checked_value : param->unchecked_value;
      }
      break;
    }

    case core::PatchParameterType::kChoice: {
      ImGui::Text("%s", param->display_name.c_str());
      for (size_t i = 0; i < param->choices.size(); ++i) {
        bool selected = (param->value == static_cast<int>(i));
        if (ImGui::RadioButton(param->choices[i].c_str(), selected)) {
          param->value = static_cast<int>(i);
        }
      }
      break;
    }

    case core::PatchParameterType::kBitfield: {
      ImGui::Text("%s", param->display_name.c_str());
      for (size_t i = 0; i < param->choices.size(); ++i) {
        if (param->choices[i].empty() || param->choices[i] == "_EMPTY") {
          continue;
        }
        bool bit_set = (param->value & (1 << i)) != 0;
        if (ImGui::Checkbox(param->choices[i].c_str(), &bit_set)) {
          if (bit_set) {
            param->value |= (1 << i);
          } else {
            param->value &= ~(1 << i);
          }
        }
      }
      break;
    }

    case core::PatchParameterType::kItem: {
      ImGui::Text("%s", param->display_name.c_str());
      // TODO: Implement item dropdown using game item names
      ImGui::SetNextItemWidth(150);
      if (ImGui::InputInt("Item ID", &param->value)) {
        param->value = std::clamp(param->value, 0, 255);
      }
      break;
    }
  }

  ImGui::PopID();
  ImGui::Spacing();
}

}  // namespace editor
}  // namespace yaze
