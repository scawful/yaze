#include "app/editor/ui/rom_load_options_dialog.h"

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "rom/rom.h"
#include "core/features.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

// Preset definitions
const char* RomLoadOptionsDialog::kPresetNames[kNumPresets] = {
    "Vanilla ROM Hack",
    "ZSCustomOverworld v2",
    "ZSCustomOverworld v3 (Recommended)",
    "Randomizer Compatible"
};

const char* RomLoadOptionsDialog::kPresetDescriptions[kNumPresets] = {
    "Standard ROM editing without custom ASM. Limited to vanilla features.",
    "Basic overworld expansion: custom BG colors, main palettes, parent system.",
    "Full overworld expansion: wide/tall areas, animated GFX, overlays, all features.",
    "Compatible with ALttP Randomizer. Minimal custom features."
};

RomLoadOptionsDialog::RomLoadOptionsDialog() {
  // Initialize with safe defaults
  options_.save_overworld_maps = true;
  options_.save_overworld_entrances = true;
  options_.save_overworld_exits = true;
  options_.save_overworld_items = true;
}

void RomLoadOptionsDialog::Open(Rom* rom) {
  if (rom) {
    Open(rom, rom->filename());
  }
}

void RomLoadOptionsDialog::Draw(bool* p_open) {
  Show(p_open);
}

void RomLoadOptionsDialog::Open(Rom* rom, const std::string& rom_filename) {
  rom_ = rom;
  rom_filename_ = rom_filename;
  is_open_ = true;
  confirmed_ = false;
  
  // Detect ROM version
  if (rom_ && rom_->is_loaded()) {
    detected_version_ = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  } else {
    detected_version_ = zelda3::OverworldVersion::kVanilla;
  }
  
  // Set default project name from ROM filename
  size_t last_slash = rom_filename_.find_last_of("/\\");
  size_t last_dot = rom_filename_.find_last_of('.');
  std::string base_name;
  if (last_slash != std::string::npos) {
    base_name = rom_filename_.substr(last_slash + 1);
  } else {
    base_name = rom_filename_;
  }
  if (last_dot != std::string::npos && last_dot > last_slash) {
    base_name = base_name.substr(0, base_name.find_last_of('.'));
  }
  
  snprintf(project_name_buffer_, sizeof(project_name_buffer_), "%s_project",
           base_name.c_str());
  
  // Auto-select preset based on detected version
  switch (detected_version_) {
    case zelda3::OverworldVersion::kVanilla:
      selected_preset_index_ = 2;  // Recommend v3 upgrade for vanilla
      options_.upgrade_to_zscustom = true;
      options_.target_zso_version = 3;
      break;
    case zelda3::OverworldVersion::kZSCustomV1:
    case zelda3::OverworldVersion::kZSCustomV2:
      selected_preset_index_ = 1;  // Keep v2 features
      break;
    case zelda3::OverworldVersion::kZSCustomV3:
      selected_preset_index_ = 2;  // Full v3 features
      break;
  }
  
  ApplyPreset(kPresetNames[selected_preset_index_]);
}

bool RomLoadOptionsDialog::Show(bool* p_open) {
  if (!is_open_) return false;
  
  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
  
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
  
  bool result = false;
  if (ImGui::Begin(
          absl::StrFormat("%s ROM Load Options", ICON_MD_SETTINGS).c_str(),
          &is_open_, flags)) {
    DrawVersionInfo();
    ImGui::Separator();
    
    DrawUpgradeOptions();
    ImGui::Separator();
    
    DrawFeatureFlagPresets();
    
    if (show_advanced_) {
      ImGui::Separator();
      DrawFeatureFlagDetails();
    }
    
    ImGui::Separator();
    DrawProjectOptions();
    
    ImGui::Separator();
    DrawActionButtons();
    
    result = confirmed_;
  }
  ImGui::End();
  
  if (p_open) {
    *p_open = is_open_;
  }
  
  return result;
}

void RomLoadOptionsDialog::DrawVersionInfo() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  
  ImGui::Text("%s ROM Information", ICON_MD_INFO);
  ImGui::Spacing();
  
  // ROM filename
  ImGui::TextColored(gui::ConvertColorToImVec4(theme.text_secondary),
                     "File: %s", rom_filename_.c_str());
  
  // Detected version with color coding
  const char* version_name =
      zelda3::OverworldVersionHelper::GetVersionName(detected_version_);
  
  ImVec4 version_color;
  switch (detected_version_) {
    case zelda3::OverworldVersion::kVanilla:
      version_color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);  // Yellow - needs upgrade
      break;
    case zelda3::OverworldVersion::kZSCustomV1:
    case zelda3::OverworldVersion::kZSCustomV2:
      version_color = ImVec4(0.2f, 0.6f, 0.8f, 1.0f);  // Blue - partial features
      break;
    case zelda3::OverworldVersion::kZSCustomV3:
      version_color = ImVec4(0.2f, 0.8f, 0.4f, 1.0f);  // Green - full features
      break;
    default:
      version_color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
  }
  
  ImGui::TextColored(version_color, "%s Detected: %s", ICON_MD_VERIFIED,
                     version_name);
  
  // Show feature availability
  if (detected_version_ == zelda3::OverworldVersion::kVanilla) {
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                       "%s This ROM can be upgraded for expanded features",
                       ICON_MD_UPGRADE);
  }
}

void RomLoadOptionsDialog::DrawUpgradeOptions() {
  bool is_vanilla = (detected_version_ == zelda3::OverworldVersion::kVanilla);
  
  ImGui::Text("%s ZSCustomOverworld Options", ICON_MD_AUTO_FIX_HIGH);
  ImGui::Spacing();
  
  if (is_vanilla) {
    ImGui::Checkbox("Upgrade ROM to ZSCustomOverworld",
                    &options_.upgrade_to_zscustom);
    
    if (options_.upgrade_to_zscustom) {
      ImGui::Indent();
      
      ImGui::Text("Target Version:");
      ImGui::SameLine();
      
      if (ImGui::RadioButton("v2 (Basic)", options_.target_zso_version == 2)) {
        options_.target_zso_version = 2;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("v3 (Full)", options_.target_zso_version == 3)) {
        options_.target_zso_version = 3;
      }
      
      // Version comparison
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                         "v2: BG colors, main palettes, parent system");
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                         "v3: + wide/tall areas, animated GFX, overlays");
      
      // Tail expansion option (only for v3)
      if (options_.target_zso_version == 3) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Experimental:");
        ImGui::Checkbox("Enable special world tail (0xA0-0xBF)",
                        &options_.enable_tail_expansion);
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "Enables access to unused special world map slots.\n"
              "REQUIRES additional ASM patch for pointer table expansion.\n"
              "Without the patch, maps will show blank tiles (safe).");
        }
      }
      
      ImGui::Unindent();
    }
  } else {
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f),
                       "%s ROM already has ZSCustomOverworld %s",
                       ICON_MD_CHECK_CIRCLE,
                       zelda3::OverworldVersionHelper::GetVersionName(
                           detected_version_));
  }
  
  // Enable custom overworld features toggle
  ImGui::Spacing();
  ImGui::Checkbox("Enable custom overworld features in editor",
                  &options_.enable_custom_overworld);
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Enables ZSCustomOverworld-specific UI elements.\n"
        "Auto-enabled if ASM is detected in ROM.");
  }
}

void RomLoadOptionsDialog::DrawFeatureFlagPresets() {
  ImGui::Text("%s Feature Presets", ICON_MD_TUNE);
  ImGui::Spacing();
  
  // Preset selection combo
  if (ImGui::BeginCombo("##PresetCombo", kPresetNames[selected_preset_index_])) {
    for (int i = 0; i < kNumPresets; i++) {
      bool is_selected = (selected_preset_index_ == i);
      if (ImGui::Selectable(kPresetNames[i], is_selected)) {
        selected_preset_index_ = i;
        ApplyPreset(kPresetNames[i]);
      }
      
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", kPresetDescriptions[i]);
      }
      
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  
  // Show preset description
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
                     kPresetDescriptions[selected_preset_index_]);
  
  // Advanced toggle
  ImGui::Spacing();
  if (ImGui::Button(show_advanced_ ? "Hide Advanced Options"
                                   : "Show Advanced Options")) {
    show_advanced_ = !show_advanced_;
  }
}

void RomLoadOptionsDialog::DrawFeatureFlagDetails() {
  ImGui::Text("%s Feature Flags", ICON_MD_FLAG);
  ImGui::Spacing();
  
  // Overworld flags
  if (ImGui::TreeNodeEx("Overworld", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Save overworld maps", &options_.save_overworld_maps);
    ImGui::Checkbox("Save entrances", &options_.save_overworld_entrances);
    ImGui::Checkbox("Save exits", &options_.save_overworld_exits);
    ImGui::Checkbox("Save items", &options_.save_overworld_items);
    ImGui::TreePop();
  }
  
  // Dungeon flags
  if (ImGui::TreeNodeEx("Dungeon")) {
    ImGui::Checkbox("Save dungeon maps", &options_.save_dungeon_maps);
    ImGui::TreePop();
  }
  
  // Graphics flags
  if (ImGui::TreeNodeEx("Graphics")) {
    ImGui::Checkbox("Save all palettes", &options_.save_all_palettes);
    ImGui::Checkbox("Save GFX groups", &options_.save_gfx_groups);
    ImGui::TreePop();
  }
}

void RomLoadOptionsDialog::DrawProjectOptions() {
  ImGui::Text("%s Project Options", ICON_MD_FOLDER);
  ImGui::Spacing();
  
  ImGui::Checkbox("Create associated project file", &options_.create_project);
  
  if (options_.create_project) {
    ImGui::Indent();
    
    ImGui::Text("Project Name:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##ProjectName", project_name_buffer_,
                     sizeof(project_name_buffer_));
    options_.project_name = project_name_buffer_;
    
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                       "Project file stores settings, labels, and preferences.");
    
    ImGui::Unindent();
  }
}

void RomLoadOptionsDialog::DrawActionButtons() {
  const float button_width = 120.0f;
  const float spacing = 10.0f;
  float total_width = button_width * 2 + spacing;
  
  // Center buttons
  float avail = ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX((avail - total_width) * 0.5f + ImGui::GetCursorPosX());
  
  // Cancel button
  if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
    is_open_ = false;
    confirmed_ = false;
  }
  
  ImGui::SameLine(0, spacing);
  
  // Confirm button with accent color
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);
  
  ImGui::PushStyleColor(ImGuiCol_Button, accent);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(accent.x * 1.1f, accent.y * 1.1f,
                               accent.z * 1.1f, accent.w));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(accent.x * 0.9f, accent.y * 0.9f,
                               accent.z * 0.9f, accent.w));
  
  if (ImGui::Button(absl::StrFormat("%s Continue", ICON_MD_CHECK).c_str(),
                    ImVec2(button_width, 0))) {
    // Apply options
    ApplyOptionsToFeatureFlags();
    options_.selected_preset = kPresetNames[selected_preset_index_];
    
    confirmed_ = true;
    is_open_ = false;
    
    // Call upgrade callback if needed
    if (options_.upgrade_to_zscustom && upgrade_callback_) {
      upgrade_callback_(options_.target_zso_version);
    }
    
    // Call confirm callback
    if (confirm_callback_) {
      confirm_callback_(options_);
    }
  }
  
  ImGui::PopStyleColor(3);
}

void RomLoadOptionsDialog::ApplyPreset(const std::string& preset_name) {
  if (preset_name == "Vanilla ROM Hack") {
    options_.upgrade_to_zscustom = false;
    options_.enable_custom_overworld = false;
    options_.save_overworld_maps = true;
    options_.save_overworld_entrances = true;
    options_.save_overworld_exits = true;
    options_.save_overworld_items = true;
    options_.save_dungeon_maps = false;
    options_.save_all_palettes = false;
    options_.save_gfx_groups = false;
  } else if (preset_name == "ZSCustomOverworld v2") {
    options_.upgrade_to_zscustom =
        (detected_version_ == zelda3::OverworldVersion::kVanilla);
    options_.target_zso_version = 2;
    options_.enable_custom_overworld = true;
    options_.save_overworld_maps = true;
    options_.save_overworld_entrances = true;
    options_.save_overworld_exits = true;
    options_.save_overworld_items = true;
    options_.save_dungeon_maps = true;
    options_.save_all_palettes = true;
    options_.save_gfx_groups = true;
  } else if (preset_name == "ZSCustomOverworld v3 (Recommended)") {
    options_.upgrade_to_zscustom =
        (detected_version_ == zelda3::OverworldVersion::kVanilla);
    options_.target_zso_version = 3;
    options_.enable_custom_overworld = true;
    options_.save_overworld_maps = true;
    options_.save_overworld_entrances = true;
    options_.save_overworld_exits = true;
    options_.save_overworld_items = true;
    options_.save_dungeon_maps = true;
    options_.save_all_palettes = true;
    options_.save_gfx_groups = true;
  } else if (preset_name == "Randomizer Compatible") {
    options_.upgrade_to_zscustom = false;
    options_.enable_custom_overworld = false;
    options_.save_overworld_maps = false;
    options_.save_overworld_entrances = false;
    options_.save_overworld_exits = false;
    options_.save_overworld_items = false;
    options_.save_dungeon_maps = false;
    options_.save_all_palettes = false;
    options_.save_gfx_groups = false;
  }
}

void RomLoadOptionsDialog::ApplyOptionsToFeatureFlags() {
  auto& flags = core::FeatureFlags::get();
  
  flags.overworld.kSaveOverworldMaps = options_.save_overworld_maps;
  flags.overworld.kSaveOverworldEntrances = options_.save_overworld_entrances;
  flags.overworld.kSaveOverworldExits = options_.save_overworld_exits;
  flags.overworld.kSaveOverworldItems = options_.save_overworld_items;
  flags.overworld.kLoadCustomOverworld = options_.enable_custom_overworld;
  flags.overworld.kEnableSpecialWorldExpansion = options_.enable_tail_expansion;
  flags.kSaveDungeonMaps = options_.save_dungeon_maps;
  flags.kSaveAllPalettes = options_.save_all_palettes;
  flags.kSaveGfxGroups = options_.save_gfx_groups;
}

bool RomLoadOptionsDialog::ShouldPromptUpgrade() const {
  return detected_version_ == zelda3::OverworldVersion::kVanilla;
}

}  // namespace editor
}  // namespace yaze

