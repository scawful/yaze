#include "app/editor/ui/settings_panel.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <vector>

#include "app/editor/system/editor_card_registry.h"
#include "app/gui/app/feature_flags_menu.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/log.h"
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
  }
}

void SettingsPanel::DrawGeneralSettings() {
  // Refactored from table to vertical list for sidebar
  static gui::FlagsMenu flags;

  ImGui::TextDisabled("Feature Flags configuration");
  ImGui::Spacing();

  if (ImGui::TreeNode("System Flags")) {
    flags.DrawSystemFlags();
    ImGui::TreePop();
  }
  
  if (ImGui::TreeNode("Overworld Flags")) {
    flags.DrawOverworldFlags();
    ImGui::TreePop();
  }
  
  if (ImGui::TreeNode("Dungeon Flags")) {
    flags.DrawDungeonFlags();
    ImGui::TreePop();
  }
  
  if (ImGui::TreeNode("Resource Flags")) {
    flags.DrawResourceFlags();
    ImGui::TreePop();
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
  if (ImGui::TreeNodeEx("Card Shortcuts", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawCardShortcuts();
    ImGui::TreePop();
  }
}

void SettingsPanel::DrawCardShortcuts() {
  if (!card_registry_ || !user_settings_) {
    ImGui::TextDisabled("Registry not available");
    return;
  }

  // Simplified shortcut editor for sidebar
  auto categories = card_registry_->GetAllCategories();

  for (const auto& category : categories) {
    if (ImGui::TreeNode(category.c_str())) {
      auto cards = card_registry_->GetCardsInCategory(0, category);

      for (const auto& card : cards) {
        ImGui::PushID(card.card_id.c_str());
        
        ImGui::Text("%s %s", card.icon.c_str(), card.display_name.c_str());
        
        std::string current_shortcut;
        auto it = user_settings_->prefs().card_shortcuts.find(card.card_id);
        if (it != user_settings_->prefs().card_shortcuts.end()) {
          current_shortcut = it->second;
        } else if (!card.shortcut_hint.empty()) {
          current_shortcut = card.shortcut_hint;
        } else {
          current_shortcut = "None";
        }

        if (is_editing_shortcut_ && editing_card_id_ == card.card_id) {
            ImGui::SetNextItemWidth(120);
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##Edit", shortcut_edit_buffer_, 
                                 sizeof(shortcut_edit_buffer_), 
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
              if (strlen(shortcut_edit_buffer_) > 0) {
                user_settings_->prefs().card_shortcuts[card.card_id] = shortcut_edit_buffer_;
              } else {
                user_settings_->prefs().card_shortcuts.erase(card.card_id);
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
            if (ImGui::Button(current_shortcut.c_str(), ImVec2(120, 0))) {
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

}  // namespace editor
}  // namespace yaze
