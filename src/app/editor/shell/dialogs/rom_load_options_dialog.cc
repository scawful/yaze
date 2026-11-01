#include "app/editor/shell/dialogs/rom_load_options_dialog.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <unordered_map>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "rom/rom.h"

namespace yaze {
namespace editor {

// Preset definitions
const char* RomLoadOptionsDialog::kPresetNames[kNumPresets] = {
    "Vanilla ROM Hack", "ZSCustomOverworld v2",
    "ZSCustomOverworld v3 (Recommended)", "Randomizer Compatible"};

const char* RomLoadOptionsDialog::kPresetDescriptions[kNumPresets] = {
    "Standard ROM editing without custom ASM. Limited to vanilla features.",
    "Basic overworld expansion: custom BG colors, main palettes, parent "
    "system.",
    "Full overworld expansion: wide/tall areas, animated GFX, overlays, all "
    "features.",
    "Compatible with ALttP Randomizer. Minimal custom features."};

namespace {

using LoadOptions = RomLoadOptionsDialog::LoadOptions;

constexpr int kRememberedChoiceVersion = 1;

struct RememberedFlag {
  const char* key;
  bool LoadOptions::* member;
};

// The flag-backed fields, in encoded order. Encode and decode both walk this
// table. Decode requires every key, so adding a field here makes older
// remembered choices show the dialog again instead of guessing a value.
constexpr RememberedFlag kRememberedFlags[] = {
    {"custom", &LoadOptions::enable_custom_overworld},
    {"tail", &LoadOptions::enable_tail_expansion},
    {"ow_maps", &LoadOptions::save_overworld_maps},
    {"ow_entrances", &LoadOptions::save_overworld_entrances},
    {"ow_exits", &LoadOptions::save_overworld_exits},
    {"ow_items", &LoadOptions::save_overworld_items},
    {"ow_sprites", &LoadOptions::save_overworld_sprites},
    {"dungeon_maps", &LoadOptions::save_dungeon_maps},
    {"palettes", &LoadOptions::save_all_palettes},
    {"gfx_groups", &LoadOptions::save_gfx_groups},
};

}  // namespace

RomLoadOptionsDialog::RomLoadOptionsDialog() {
  // Initialize with safe defaults
  options_.save_overworld_maps = true;
  options_.save_overworld_entrances = true;
  options_.save_overworld_exits = true;
  options_.save_overworld_items = true;
  options_.save_overworld_sprites = true;
}

void RomLoadOptionsDialog::Open(Rom* rom) {
  if (rom) {
    Open(rom, rom->filename(), false);
  }
}

void RomLoadOptionsDialog::Open(Rom* rom, bool suggest_project_creation) {
  if (rom) {
    Open(rom, rom->filename(), suggest_project_creation);
  }
}

void RomLoadOptionsDialog::Draw(bool* p_open) {
  Show(p_open);
}

void RomLoadOptionsDialog::Open(Rom* rom, const std::string& rom_filename) {
  Open(rom, rom_filename, false);
}

void RomLoadOptionsDialog::Open(Rom* rom, const std::string& rom_filename,
                                bool suggest_project_creation) {
  rom_ = rom;
  rom_filename_ = rom_filename;
  is_open_ = true;
  confirmed_ = false;
  show_advanced_ = false;
  reviewing_ = false;

  // Reset options for each open so stale state from previous ROM loads does
  // not carry over.
  options_ = LoadOptions{};
  options_.save_overworld_maps = true;
  options_.save_overworld_entrances = true;
  options_.save_overworld_exits = true;
  options_.save_overworld_items = true;
  options_.save_overworld_sprites = true;
  options_.create_project = suggest_project_creation;

  // Detect ROM version
  if (rom_ && rom_->is_loaded()) {
    detected_version_ = zelda3::OverworldVersionHelper::GetVersion(*rom_);
    zso_version_byte_ = zelda3::OverworldVersionHelper::GetAsmVersion(*rom_);
  } else {
    detected_version_ = zelda3::OverworldVersion::kVanilla;
    zso_version_byte_ = -1;
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
  options_.create_project = suggest_project_creation;
}

void RomLoadOptionsDialog::OpenForReview(
    Rom* rom, const core::FeatureFlags::Flags& active,
    const std::string& preset_name, bool remembered) {
  if (!rom) {
    return;
  }
  Open(rom, rom->filename(), false);
  reviewing_ = true;
  for (int i = 0; i < kNumPresets; ++i) {
    if (preset_name == kPresetNames[i]) {
      selected_preset_index_ = i;
      break;
    }
  }

  // Show the flags Save uses now, not the preset Open() picked for the ROM
  // version. The two can differ after a remembered choice or a menu toggle.
  const LoadOptions current = OptionsFromFeatureFlags(active);
  for (const auto& flag : kRememberedFlags) {
    options_.*flag.member = current.*flag.member;
  }
  options_.upgrade_to_zscustom = false;
  options_.create_project = false;
  options_.remember_for_rom = remembered;
  show_advanced_ = true;
}

bool RomLoadOptionsDialog::Show(bool* p_open) {
  if (!is_open_)
    return false;

  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

  bool result = false;
  if (ImGui::Begin(
          absl::StrFormat("%s ROM Load Options", ICON_MD_SETTINGS).c_str(),
          &is_open_, flags)) {
    DrawVersionInfo();
    if (reviewing_) {
      ImGui::Spacing();
      gui::StyleColorGuard note_color(ImGuiCol_Text, gui::GetInfoColor());
      ImGui::TextWrapped(
          "%s", tr("These are the options this session uses now. Apply "
                   "changes what the next save writes."));
    }
    ImGui::Separator();

    DrawUpgradeOptions();
    ImGui::Separator();

    DrawFeatureFlagPresets();

    if (show_advanced_) {
      ImGui::Separator();
      DrawFeatureFlagDetails();
    }

    // The ROM already has a session; a project comes from Save Project As.
    if (!reviewing_) {
      ImGui::Separator();
      DrawProjectOptions();
    }

    ImGui::Separator();
    DrawActionButtons();

    result = confirmed_;
  }
  ImGui::End();

  // Cancel and the title-bar close button both end here, once: a closed
  // dialog returns before drawing on later frames.
  if (!is_open_ && !confirmed_ && cancel_callback_) {
    cancel_callback_();
  }

  if (p_open) {
    *p_open = is_open_;
  }

  return result;
}

void RomLoadOptionsDialog::DrawVersionInfo() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  ImGui::Text(tr("%s ROM Information"), ICON_MD_INFO);
  ImGui::Spacing();

  // ROM filename
  ImGui::TextColored(gui::ConvertColorToImVec4(theme.text_secondary),
                     tr("File: %s"), rom_filename_.c_str());

  // Detected version with color coding
  const char* version_name =
      zelda3::OverworldVersionHelper::GetVersionName(detected_version_);

  ImVec4 version_color;
  switch (detected_version_) {
    case zelda3::OverworldVersion::kVanilla:
      version_color = gui::GetWarningColor();  // Vanilla ROM needs an upgrade.
      break;
    case zelda3::OverworldVersion::kZSCustomV1:
    case zelda3::OverworldVersion::kZSCustomV2:
      version_color = gui::GetInfoColor();  // Partial feature support.
      break;
    case zelda3::OverworldVersion::kZSCustomV3:
      version_color = gui::GetSuccessColor();  // Full feature support.
      break;
    default:
      version_color = gui::GetTextSecondaryVec4();
  }

  ImGui::TextColored(version_color, tr("%s Detected: %s"), ICON_MD_VERIFIED,
                     version_name);

  // Show feature availability
  if (detected_version_ == zelda3::OverworldVersion::kVanilla) {
    ImGui::TextColored(gui::GetWarningColor(),
                       tr("%s This ROM can be upgraded for expanded features"),
                       ICON_MD_UPGRADE);
  }
}

void RomLoadOptionsDialog::DrawUpgradeOptions() {
  bool is_vanilla = (detected_version_ == zelda3::OverworldVersion::kVanilla);

  ImGui::Text(tr("%s ZSCustomOverworld Options"), ICON_MD_AUTO_FIX_HIGH);
  ImGui::Spacing();

  if (is_vanilla) {
    ImGui::Checkbox(tr("Upgrade ROM to ZSCustomOverworld"),
                    &options_.upgrade_to_zscustom);

    if (options_.upgrade_to_zscustom) {
      ImGui::Indent();

      ImGui::Text(tr("Target Version:"));
      ImGui::SameLine();

      if (ImGui::RadioButton(tr("v2 (Basic)"),
                             options_.target_zso_version == 2)) {
        options_.target_zso_version = 2;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton(tr("v3 (Full)"),
                             options_.target_zso_version == 3)) {
        options_.target_zso_version = 3;
      }

      // Version comparison
      ImGui::TextColored(gui::GetTextSecondaryVec4(),
                         tr("v2: BG colors, main palettes, parent system"));
      ImGui::TextColored(gui::GetTextSecondaryVec4(),
                         tr("v3: + wide/tall areas, animated GFX, overlays"));

      // Tail expansion option (only for v3)
      if (options_.target_zso_version == 3) {
        ImGui::Spacing();
        ImGui::TextColored(gui::GetWarningColor(), tr("Experimental:"));
        ImGui::Checkbox(tr("Enable special world tail (0xA0-0xBF)"),
                        &options_.enable_tail_expansion);
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              tr("Enables access to unused special world map slots.\n"
                 "REQUIRES additional ASM patch for pointer table expansion.\n"
                 "Without the patch, maps will show blank tiles (safe)."));
        }
      }

      ImGui::Unindent();
    }
  } else {
    ImGui::TextColored(
        gui::GetSuccessColor(), tr("%s ROM already has ZSCustomOverworld %s"),
        ICON_MD_CHECK_CIRCLE,
        zelda3::OverworldVersionHelper::GetVersionName(detected_version_));
  }

  // Enable custom overworld features toggle
  ImGui::Spacing();
  ImGui::Checkbox(tr("Enable custom overworld features in editor"),
                  &options_.enable_custom_overworld);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        tr("Enables ZSCustomOverworld-specific UI elements.\n"
           "Auto-enabled if ASM is detected in ROM."));
  }
}

void RomLoadOptionsDialog::DrawFeatureFlagPresets() {
  ImGui::Text(tr("%s Feature Presets"), ICON_MD_TUNE);
  ImGui::Spacing();

  // Preset selection combo
  if (ImGui::BeginCombo("##PresetCombo",
                        kPresetNames[selected_preset_index_])) {
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
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "%s",
                     kPresetDescriptions[selected_preset_index_]);

  // Advanced toggle
  ImGui::Spacing();
  if (ImGui::Button(show_advanced_ ? tr("Hide Advanced Options")
                                   : tr("Show Advanced Options"))) {
    show_advanced_ = !show_advanced_;
  }
}

void RomLoadOptionsDialog::DrawFeatureFlagDetails() {
  ImGui::Text(tr("%s Feature Flags"), ICON_MD_FLAG);
  ImGui::Spacing();

  // Overworld flags
  if (ImGui::TreeNodeEx("Overworld", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox(tr("Save overworld maps"), &options_.save_overworld_maps);
    ImGui::Checkbox(tr("Save entrances"), &options_.save_overworld_entrances);
    ImGui::Checkbox(tr("Save exits"), &options_.save_overworld_exits);
    ImGui::Checkbox(tr("Save items"), &options_.save_overworld_items);
    ImGui::Checkbox(tr("Save sprites"), &options_.save_overworld_sprites);
    ImGui::TreePop();
  }

  // Dungeon flags
  if (ImGui::TreeNodeEx("Dungeon")) {
    ImGui::Checkbox(tr("Save dungeon maps"), &options_.save_dungeon_maps);
    ImGui::TreePop();
  }

  // Graphics flags
  if (ImGui::TreeNodeEx("Graphics")) {
    ImGui::Checkbox(tr("Save all palettes"), &options_.save_all_palettes);
    ImGui::Checkbox(tr("Save GFX groups"), &options_.save_gfx_groups);
    ImGui::TreePop();
  }
}

void RomLoadOptionsDialog::DrawProjectOptions() {
  ImGui::Text(tr("%s Project Options"), ICON_MD_FOLDER);
  ImGui::Spacing();

  ImGui::Checkbox(tr("Create associated project file"),
                  &options_.create_project);

  if (options_.create_project) {
    ImGui::Indent();

    ImGui::Text(tr("Project Name:"));
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##ProjectName", project_name_buffer_,
                     sizeof(project_name_buffer_));
    options_.project_name = project_name_buffer_;

    ImGui::TextColored(
        gui::GetTextSecondaryVec4(),
        tr("Project file stores settings, labels, and preferences."));

    ImGui::Unindent();
  }
}

void RomLoadOptionsDialog::DrawActionButtons() {
  ImGui::Checkbox(tr("Remember for this ROM file"), &options_.remember_for_rom);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s",
        tr("Skip this dialog the next time this file opens.\n"
           "Change it later with ROM Load Options… in the command palette.\n"
           "Forgotten when the ROM's ZSCustomOverworld version changes."));
  }
  ImGui::Spacing();

  const float button_width = 120.0f;
  const float spacing = 10.0f;
  float total_width = button_width * 2 + spacing;

  // Center buttons
  float avail = ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX((avail - total_width) * 0.5f + ImGui::GetCursorPosX());

  // Cancel button
  if (ImGui::Button(tr("Cancel"), ImVec2(button_width, 0))) {
    is_open_ = false;
    confirmed_ = false;
  }

  ImGui::SameLine(0, spacing);

  // Confirm button with accent color
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);

  {
    gui::StyleColorGuard btn_guard(
        {{ImGuiCol_Button, accent},
         {ImGuiCol_ButtonHovered,
          ImVec4(accent.x * 1.1f, accent.y * 1.1f, accent.z * 1.1f, accent.w)},
         {ImGuiCol_ButtonActive, ImVec4(accent.x * 0.9f, accent.y * 0.9f,
                                        accent.z * 0.9f, accent.w)}});

    // StrFormat needs a literal format, so pick the whole string.
    const std::string confirm_label =
        reviewing_ ? absl::StrFormat("%s Apply", ICON_MD_CHECK)
                   : absl::StrFormat("%s Continue", ICON_MD_CHECK);
    if (ImGui::Button(confirm_label.c_str(), ImVec2(button_width, 0))) {
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
  }
}

void RomLoadOptionsDialog::ApplyPreset(const std::string& preset_name) {
  if (preset_name == "Vanilla ROM Hack") {
    options_.upgrade_to_zscustom = false;
    options_.enable_custom_overworld = false;
    options_.save_overworld_maps = true;
    options_.save_overworld_entrances = true;
    options_.save_overworld_exits = true;
    options_.save_overworld_items = true;
    options_.save_overworld_sprites = true;
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
    options_.save_overworld_sprites = true;
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
    options_.save_overworld_sprites = true;
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
    options_.save_overworld_sprites = false;
    options_.save_dungeon_maps = false;
    options_.save_all_palettes = false;
    options_.save_gfx_groups = false;
  }
}

void RomLoadOptionsDialog::ApplyOptionsToFeatureFlags() {
  ApplyToFeatureFlags(options_, core::FeatureFlags::get());
}

void RomLoadOptionsDialog::ApplyToFeatureFlags(
    const LoadOptions& options, core::FeatureFlags::Flags& flags) {
  flags.overworld.kSaveOverworldMaps = options.save_overworld_maps;
  flags.overworld.kSaveOverworldEntrances = options.save_overworld_entrances;
  flags.overworld.kSaveOverworldExits = options.save_overworld_exits;
  flags.overworld.kSaveOverworldItems = options.save_overworld_items;
  flags.overworld.kSaveOverworldSprites = options.save_overworld_sprites;
  flags.overworld.kLoadCustomOverworld = options.enable_custom_overworld;
  flags.overworld.kEnableSpecialWorldExpansion = options.enable_tail_expansion;
  flags.kSaveDungeonMaps = options.save_dungeon_maps;
  flags.kSaveAllPalettes = options.save_all_palettes;
  flags.kSaveGfxGroups = options.save_gfx_groups;
}

RomLoadOptionsDialog::LoadOptions RomLoadOptionsDialog::OptionsFromFeatureFlags(
    const core::FeatureFlags::Flags& flags) {
  LoadOptions options;
  options.save_overworld_maps = flags.overworld.kSaveOverworldMaps;
  options.save_overworld_entrances = flags.overworld.kSaveOverworldEntrances;
  options.save_overworld_exits = flags.overworld.kSaveOverworldExits;
  options.save_overworld_items = flags.overworld.kSaveOverworldItems;
  options.save_overworld_sprites = flags.overworld.kSaveOverworldSprites;
  options.enable_custom_overworld = flags.overworld.kLoadCustomOverworld;
  options.enable_tail_expansion = flags.overworld.kEnableSpecialWorldExpansion;
  options.save_dungeon_maps = flags.kSaveDungeonMaps;
  options.save_all_palettes = flags.kSaveAllPalettes;
  options.save_gfx_groups = flags.kSaveGfxGroups;
  return options;
}

std::string RomLoadOptionsDialog::EncodeRememberedChoice(
    const LoadOptions& options, int zso_version_byte) {
  std::string encoded =
      absl::StrCat("v=", kRememberedChoiceVersion, ";zso=", zso_version_byte);
  for (const auto& flag : kRememberedFlags) {
    absl::StrAppend(&encoded, ";", flag.key, "=",
                    options.*flag.member ? "1" : "0");
  }

  // ';' separates fields, and a tab or newline would split the legacy INI
  // line, so neither may reach the stored preset name.
  std::string preset = options.selected_preset;
  preset.erase(std::remove_if(preset.begin(), preset.end(),
                              [](char c) {
                                return c == ';' || c == '\t' || c == '\r' ||
                                       c == '\n';
                              }),
               preset.end());
  if (!preset.empty()) {
    absl::StrAppend(&encoded, ";preset=", preset);
  }
  return encoded;
}

std::optional<RomLoadOptionsDialog::RememberedChoice>
RomLoadOptionsDialog::DecodeRememberedChoice(std::string_view encoded) {
  std::unordered_map<std::string_view, std::string_view> fields;
  for (std::string_view part :
       absl::StrSplit(encoded, ';', absl::SkipEmpty())) {
    const size_t eq = part.find('=');
    if (eq == std::string_view::npos) {
      continue;
    }
    fields[part.substr(0, eq)] = part.substr(eq + 1);
  }

  int version = 0;
  auto it = fields.find("v");
  if (it == fields.end() || !absl::SimpleAtoi(it->second, &version) ||
      version != kRememberedChoiceVersion) {
    return std::nullopt;
  }

  RememberedChoice choice;
  it = fields.find("zso");
  if (it == fields.end() ||
      !absl::SimpleAtoi(it->second, &choice.zso_version_byte) ||
      choice.zso_version_byte < 0 || choice.zso_version_byte > 0xFF) {
    return std::nullopt;
  }

  for (const auto& flag : kRememberedFlags) {
    it = fields.find(flag.key);
    if (it == fields.end() || (it->second != "0" && it->second != "1")) {
      return std::nullopt;
    }
    choice.options.*flag.member = it->second == "1";
  }

  it = fields.find("preset");
  if (it != fields.end()) {
    choice.options.selected_preset = std::string(it->second);
  }
  return choice;
}

std::string RomLoadOptionsDialog::RomChoiceKey(const std::string& rom_path) {
  if (rom_path.empty()) {
    return {};
  }
  std::filesystem::path path(rom_path);
  if (path.is_relative()) {
    std::error_code ec;
    auto absolute = std::filesystem::absolute(path, ec);
    if (!ec) {
      path = std::move(absolute);
    }
  }
  return path.lexically_normal().string();
}

bool RomLoadOptionsDialog::ShouldPromptUpgrade() const {
  return detected_version_ == zelda3::OverworldVersion::kVanilla;
}

}  // namespace editor
}  // namespace yaze
