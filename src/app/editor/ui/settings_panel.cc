#include "app/editor/ui/settings_panel.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/system/shortcut_manager.h"
#include "app/gui/app/feature_flags_menu.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "core/patch/asm_patch.h"
#include "core/patch/patch_manager.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "rom/rom.h"
#include "util/file_util.h"
#include "util/log.h"
#include "util/platform_paths.h"
#include "util/rom_hash.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

namespace {

struct HexListEditorState {
  std::string text;
  std::string error;
};

bool ParseHexToken(const std::string& token, uint16_t* out) {
  if (!out) {
    return false;
  }
  if (token.empty()) {
    return false;
  }
  std::string trimmed = token;
  if (absl::StartsWithIgnoreCase(trimmed, "0x")) {
    trimmed = trimmed.substr(2);
  }
  if (trimmed.empty()) {
    return false;
  }
  char* end = nullptr;
  unsigned long value = std::strtoul(trimmed.c_str(), &end, 16);
  if (end == nullptr || *end != '\0') {
    return false;
  }
  if (value > 0xFFFFu) {
    return false;
  }
  *out = static_cast<uint16_t>(value);
  return true;
}

bool ParseHexList(const std::string& input, std::vector<uint16_t>* out,
                  std::string* error) {
  if (!out) {
    return false;
  }
  out->clear();
  if (error) {
    error->clear();
  }
  if (input.empty()) {
    return true;
  }

  std::string normalized = input;
  for (char& c : normalized) {
    if (c == ',' || c == ';') {
      c = ' ';
    }
  }

  std::stringstream ss(normalized);
  std::string token;
  std::unordered_set<uint16_t> seen;
  while (ss >> token) {
    auto dash = token.find('-');
    if (dash != std::string::npos) {
      std::string left = token.substr(0, dash);
      std::string right = token.substr(dash + 1);
      uint16_t start = 0;
      uint16_t end = 0;
      if (!ParseHexToken(left, &start) || !ParseHexToken(right, &end)) {
        if (error) {
          *error = absl::StrFormat("Invalid range: %s", token);
        }
        return false;
      }
      if (end < start) {
        if (error) {
          *error = absl::StrFormat("Range end before start: %s", token);
        }
        return false;
      }
      for (uint16_t value = start; value <= end; ++value) {
        if (seen.insert(value).second) {
          out->push_back(value);
        }
        if (value == 0xFFFF) {
          break;
        }
      }
    } else {
      uint16_t value = 0;
      if (!ParseHexToken(token, &value)) {
        if (error) {
          *error = absl::StrFormat("Invalid hex value: %s", token);
        }
        return false;
      }
      if (seen.insert(value).second) {
        out->push_back(value);
      }
    }
  }
  return true;
}

std::string FormatHexList(const std::vector<uint16_t>& values) {
  std::string result;
  result.reserve(values.size() * 6);
  for (size_t i = 0; i < values.size(); ++i) {
    const uint16_t value = values[i];
    std::string token = value <= 0xFF ? absl::StrFormat("0x%02X", value)
                                      : absl::StrFormat("0x%04X", value);
    if (!result.empty()) {
      result.append(", ");
    }
    result.append(token);
  }
  return result;
}

std::vector<uint16_t> DefaultTrackTiles() {
  std::vector<uint16_t> values;
  for (uint16_t tile = 0xB0; tile <= 0xBE; ++tile) {
    values.push_back(tile);
  }
  return values;
}

std::vector<uint16_t> DefaultStopTiles() {
  return {0xB7, 0xB8, 0xB9, 0xBA};
}

std::vector<uint16_t> DefaultSwitchTiles() {
  return {0xD0, 0xD1, 0xD2, 0xD3};
}

std::vector<uint16_t> DefaultTrackObjectIds() {
  return {0x31};
}

std::vector<uint16_t> DefaultMinecartSpriteIds() {
  return {0xA3};
}

bool IsLocalEndpoint(const std::string& base_url) {
  if (base_url.empty()) {
    return false;
  }
  std::string lower = absl::AsciiStrToLower(base_url);
  return absl::StrContains(lower, "localhost") ||
         absl::StrContains(lower, "127.0.0.1") ||
         absl::StrContains(lower, "::1") ||
         absl::StrContains(lower, "0.0.0.0") ||
         absl::StrContains(lower, "192.168.") || absl::StartsWith(lower, "10.");
}

bool IsTailscaleEndpoint(const std::string& base_url) {
  if (base_url.empty()) {
    return false;
  }
  std::string lower = absl::AsciiStrToLower(base_url);
  return absl::StrContains(lower, ".ts.net") ||
         absl::StrContains(lower, "100.64.");
}

std::string BuildHostTagString(const UserSettings::Preferences::AiHost& host) {
  std::vector<std::string> tags;
  if (IsLocalEndpoint(host.base_url)) {
    tags.push_back("local");
  }
  if (IsTailscaleEndpoint(host.base_url)) {
    tags.push_back("tailscale");
  }
  if (absl::StartsWith(absl::AsciiStrToLower(host.base_url), "https://")) {
    tags.push_back("https");
  } else if (absl::StartsWith(absl::AsciiStrToLower(host.base_url),
                              "http://") &&
             !IsLocalEndpoint(host.base_url) &&
             !IsTailscaleEndpoint(host.base_url)) {
    tags.push_back("http");
  }
  if (host.supports_vision) {
    tags.push_back("vision");
  }
  if (host.supports_tools) {
    tags.push_back("tools");
  }
  if (host.supports_streaming) {
    tags.push_back("stream");
  }
  if (tags.empty()) {
    return "";
  }
  std::string result = "[";
  for (size_t i = 0; i < tags.size(); ++i) {
    result += tags[i];
    if (i + 1 < tags.size()) {
      result += ", ";
    }
  }
  result += "]";
  return result;
}

bool AddUniquePath(std::vector<std::string>* paths, const std::string& path) {
  if (!paths || path.empty()) {
    return false;
  }
  auto it = std::find(paths->begin(), paths->end(), path);
  if (it != paths->end()) {
    return false;
  }
  paths->push_back(path);
  return true;
}

}  // namespace

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

  if (ImGui::CollapsingHeader(ICON_MD_STORAGE " Files & Sync")) {
    ImGui::Indent();
    DrawFilesystemSettings();
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
  ImGui::Text("%s ROM Identity", ICON_MD_VIDEOGAME_ASSET);
  ImGui::Separator();

  const char* roles[] = {"base", "dev", "patched", "release"};
  int role_index = static_cast<int>(project_->rom_metadata.role);
  if (ImGui::Combo("Role", &role_index, roles, IM_ARRAYSIZE(roles))) {
    project_->rom_metadata.role = static_cast<project::RomRole>(role_index);
    project_->Save();
  }

  const char* policies[] = {"allow", "warn", "block"};
  int policy_index = static_cast<int>(project_->rom_metadata.write_policy);
  if (ImGui::Combo("Write Policy", &policy_index, policies,
                   IM_ARRAYSIZE(policies))) {
    project_->rom_metadata.write_policy =
        static_cast<project::RomWritePolicy>(policy_index);
    project_->Save();
  }

  std::string expected_hash = project_->rom_metadata.expected_hash;
  if (ImGui::InputText("Expected Hash", &expected_hash)) {
    project_->rom_metadata.expected_hash = expected_hash;
    project_->Save();
  }

  static std::string cached_rom_hash;
  static std::string cached_rom_path;
  if (rom_ && rom_->is_loaded()) {
    if (cached_rom_path != rom_->filename()) {
      cached_rom_path = rom_->filename();
      cached_rom_hash = util::ComputeRomHash(rom_->data(), rom_->size());
    }
    ImGui::Text("Current ROM Hash: %s", cached_rom_hash.empty()
                                            ? "(unknown)"
                                            : cached_rom_hash.c_str());
    if (ImGui::Button("Use Current ROM Hash")) {
      project_->rom_metadata.expected_hash = cached_rom_hash;
      project_->Save();
    }
  } else {
    ImGui::TextDisabled("Current ROM Hash: (no ROM loaded)");
  }

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

  ImGui::Spacing();
  ImGui::Text("%s ASM / Hack Manifest", ICON_MD_CODE);
  ImGui::Separator();
  ImGui::TextWrapped(
      "Optional: load a hack manifest JSON (generated by an ASM project) to "
      "annotate room tags, show feature flags, and surface which ROM regions "
      "are owned by ASM vs safe to edit in yaze.");

  std::string manifest_file = project_->hack_manifest_file;
  if (ImGui::InputText("Hack Manifest File", &manifest_file)) {
    project_->hack_manifest_file = manifest_file;
    project_->ReloadHackManifest();
    project_->Save();
  }

  const bool manifest_loaded = project_->hack_manifest.loaded();
  ImGui::SameLine();
  ImGui::TextDisabled(manifest_loaded ? "(loaded)" : "(not loaded)");
  if (ImGui::Button("Reload Manifest")) {
    project_->ReloadHackManifest();
  }

  if (manifest_loaded) {
    ImGui::Spacing();
    ImGui::Text("Hack: %s", project_->hack_manifest.hack_name().c_str());
    ImGui::Text("Manifest Version: %d",
                project_->hack_manifest.manifest_version());
    ImGui::Text("Hooks Tracked: %d", project_->hack_manifest.total_hooks());

    const auto& pipeline = project_->hack_manifest.build_pipeline();
    if (!pipeline.dev_rom.empty()) {
      ImGui::Text("Dev ROM: %s", pipeline.dev_rom.c_str());
    }
    if (!pipeline.patched_rom.empty()) {
      ImGui::Text("Patched ROM: %s", pipeline.patched_rom.c_str());
    }
    if (!pipeline.build_script.empty()) {
      ImGui::Text("Build Script: %s", pipeline.build_script.c_str());
    }

    const auto& msg_layout = project_->hack_manifest.message_layout();
    if (msg_layout.first_expanded_id != 0 || msg_layout.last_expanded_id != 0) {
      ImGui::Text("Expanded Messages: 0x%03X-0x%03X (%d)",
                  msg_layout.first_expanded_id, msg_layout.last_expanded_id,
                  msg_layout.expanded_count);
    }

    if (ImGui::TreeNode(ICON_MD_FLAG " Hack Feature Flags")) {
      for (const auto& flag : project_->hack_manifest.feature_flags()) {
        ImGui::BulletText("%s = %d (%s)", flag.name.c_str(), flag.value,
                          flag.enabled ? "enabled" : "disabled");
        if (!flag.source.empty()) {
          ImGui::SameLine();
          ImGui::TextDisabled("%s", flag.source.c_str());
        }
      }
      ImGui::TreePop();
    }

    if (ImGui::TreeNode(ICON_MD_LABEL " Room Tags (Dispatch)")) {
      for (const auto& tag : project_->hack_manifest.room_tags()) {
        ImGui::BulletText("0x%02X: %s", tag.tag_id, tag.name.c_str());
        if (!tag.enabled && !tag.feature_flag.empty()) {
          ImGui::SameLine();
          ImGui::TextDisabled("(disabled by %s)", tag.feature_flag.c_str());
        }
        if (!tag.purpose.empty() && ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", tag.purpose.c_str());
        }
      }
      ImGui::TreePop();
    }
  }

  ImGui::Spacing();
  ImGui::Text("%s Backup Settings", ICON_MD_BACKUP);
  ImGui::Separator();

  std::string backup_folder = project_->rom_backup_folder;
  if (ImGui::InputText("Backup Folder", &backup_folder)) {
    project_->rom_backup_folder = backup_folder;
    project_->Save();
  }

  bool backup_on_save = project_->workspace_settings.backup_on_save;
  if (ImGui::Checkbox("Backup Before Save", &backup_on_save)) {
    project_->workspace_settings.backup_on_save = backup_on_save;
    project_->Save();
  }

  int retention = project_->workspace_settings.backup_retention_count;
  if (ImGui::InputInt("Retention Count", &retention)) {
    project_->workspace_settings.backup_retention_count =
        std::max(0, retention);
    project_->Save();
  }

  bool keep_daily = project_->workspace_settings.backup_keep_daily;
  if (ImGui::Checkbox("Keep Daily Snapshots", &keep_daily)) {
    project_->workspace_settings.backup_keep_daily = keep_daily;
    project_->Save();
  }

  int keep_days = project_->workspace_settings.backup_keep_daily_days;
  if (ImGui::InputInt("Keep Daily Days", &keep_days)) {
    project_->workspace_settings.backup_keep_daily_days =
        std::max(1, keep_days);
    project_->Save();
  }

  ImGui::Spacing();
  ImGui::Text("%s Dungeon Overlay", ICON_MD_TRAIN);
  ImGui::Separator();
  ImGui::TextWrapped(
      "Configure collision/object IDs used by minecart overlays and audits. "
      "Hex values, ranges allowed (e.g. B0-BE).");

  static std::string overlay_project_path;
  static HexListEditorState track_tiles_state;
  static HexListEditorState stop_tiles_state;
  static HexListEditorState switch_tiles_state;
  static HexListEditorState track_object_state;
  static HexListEditorState minecart_sprite_state;

  if (overlay_project_path != project_->filepath) {
    overlay_project_path = project_->filepath;
    track_tiles_state.text =
        FormatHexList(project_->dungeon_overlay.track_tiles);
    stop_tiles_state.text =
        FormatHexList(project_->dungeon_overlay.track_stop_tiles);
    switch_tiles_state.text =
        FormatHexList(project_->dungeon_overlay.track_switch_tiles);
    track_object_state.text =
        FormatHexList(project_->dungeon_overlay.track_object_ids);
    minecart_sprite_state.text =
        FormatHexList(project_->dungeon_overlay.minecart_sprite_ids);
    track_tiles_state.error.clear();
    stop_tiles_state.error.clear();
    switch_tiles_state.error.clear();
    track_object_state.error.clear();
    minecart_sprite_state.error.clear();
  }

  auto draw_hex_list = [&](const char* label, const char* hint,
                           HexListEditorState& state,
                           const std::vector<uint16_t>& defaults,
                           std::vector<uint16_t>* target) {
    if (!target) {
      return;
    }

    bool apply = false;
    ImGui::PushItemWidth(-180.0f);
    if (ImGui::InputTextWithHint(label, hint, &state.text)) {
      state.error.clear();
    }
    ImGui::PopItemWidth();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      apply = true;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton(absl::StrFormat("Apply##%s", label).c_str())) {
      apply = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(absl::StrFormat("Defaults##%s", label).c_str())) {
      state.text = FormatHexList(defaults);
      apply = true;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Reset to defaults");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(absl::StrFormat("Clear##%s", label).c_str())) {
      state.text.clear();
      apply = true;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Clear list (empty uses defaults)");
    }

    const bool uses_defaults = target->empty();
    const std::vector<uint16_t>& effective_values =
        uses_defaults ? defaults : *target;
    ImGui::SameLine();
    ImGui::TextDisabled(ICON_MD_INFO);
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("Effective: %s", FormatHexList(effective_values).c_str());
      if (uses_defaults) {
        ImGui::TextDisabled("Using defaults (list is empty)");
      }
      ImGui::EndTooltip();
    }

    if (apply) {
      std::vector<uint16_t> parsed;
      std::string error;
      if (ParseHexList(state.text, &parsed, &error)) {
        *target = parsed;
        project_->Save();
        state.error.clear();
        state.text = FormatHexList(parsed);
      } else {
        state.error = error;
      }
    }

    if (!state.error.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s",
                         state.error.c_str());
    }
  };

  draw_hex_list("Track Tiles", "0xB0-0xBE", track_tiles_state,
                DefaultTrackTiles(), &project_->dungeon_overlay.track_tiles);
  draw_hex_list("Stop Tiles", "0xB7, 0xB8, 0xB9, 0xBA", stop_tiles_state,
                DefaultStopTiles(),
                &project_->dungeon_overlay.track_stop_tiles);
  draw_hex_list("Switch Tiles", "0xD0-0xD3", switch_tiles_state,
                DefaultSwitchTiles(),
                &project_->dungeon_overlay.track_switch_tiles);
  draw_hex_list("Track Object IDs", "0x31", track_object_state,
                DefaultTrackObjectIds(),
                &project_->dungeon_overlay.track_object_ids);
  draw_hex_list("Minecart Sprite IDs", "0xA3", minecart_sprite_state,
                DefaultMinecartSpriteIds(),
                &project_->dungeon_overlay.minecart_sprite_ids);
}

void SettingsPanel::DrawFilesystemSettings() {
  if (!user_settings_) {
    return;
  }

  auto& prefs = user_settings_->prefs();
  auto& roots = prefs.project_root_paths;
  static int selected_root_index = -1;
  static std::string new_root_path;

  ImGui::Text("%s Project Roots", ICON_MD_FOLDER_OPEN);
  ImGui::Separator();

  if (roots.empty()) {
    ImGui::TextDisabled("No project roots configured.");
  }

  if (ImGui::BeginChild("ProjectRootsList", ImVec2(0, 140), true)) {
    for (size_t i = 0; i < roots.size(); ++i) {
      const bool is_default = roots[i] == prefs.default_project_root;
      std::string label =
          util::PlatformPaths::NormalizePathForDisplay(roots[i]);
      if (is_default) {
        label += " (default)";
      }
      if (ImGui::Selectable(label.c_str(),
                            selected_root_index == static_cast<int>(i))) {
        selected_root_index = static_cast<int>(i);
      }
    }
  }
  ImGui::EndChild();

  const bool has_selection =
      selected_root_index >= 0 &&
      selected_root_index < static_cast<int>(roots.size());
  if (has_selection) {
    if (ImGui::Button("Set Default")) {
      prefs.default_project_root = roots[selected_root_index];
      user_settings_->Save();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_DELETE " Remove")) {
      const std::string removed = roots[selected_root_index];
      roots.erase(roots.begin() + selected_root_index);
      if (prefs.default_project_root == removed) {
        prefs.default_project_root = roots.empty() ? "" : roots.front();
      }
      selected_root_index = roots.empty()
                                ? -1
                                : std::min(selected_root_index,
                                           static_cast<int>(roots.size() - 1));
      user_settings_->Save();
    }
  }

  ImGui::Spacing();
  ImGui::Text("%s Add Root", ICON_MD_ADD);
  ImGui::Separator();

  ImGui::InputTextWithHint("##project_root_add", "Add folder path...",
                           &new_root_path);
  if (ImGui::Button(ICON_MD_ADD " Add Path")) {
    const std::string trimmed =
        std::string(absl::StripAsciiWhitespace(new_root_path));
    if (!trimmed.empty()) {
      if (AddUniquePath(&roots, trimmed) &&
          prefs.default_project_root.empty()) {
        prefs.default_project_root = trimmed;
      }
      user_settings_->Save();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Browse")) {
    const std::string folder = util::FileDialogWrapper::ShowOpenFolderDialog();
    if (!folder.empty()) {
      if (AddUniquePath(&roots, folder) && prefs.default_project_root.empty()) {
        prefs.default_project_root = folder;
      }
      user_settings_->Save();
    }
  }

  ImGui::Spacing();
  ImGui::Text("%s Quick Add", ICON_MD_BOLT);
  ImGui::Separator();

  if (ImGui::Button(ICON_MD_HOME " Add Documents")) {
    auto docs_dir = util::PlatformPaths::GetUserDocumentsDirectory();
    if (docs_dir.ok()) {
      if (AddUniquePath(&roots, docs_dir->string()) &&
          prefs.default_project_root.empty()) {
        prefs.default_project_root = docs_dir->string();
      }
      user_settings_->Save();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_CLOUD " Add iCloud Projects")) {
    auto icloud_dir =
        util::PlatformPaths::GetUserDocumentsSubdirectory("iCloud");
    if (icloud_dir.ok()) {
      if (AddUniquePath(&roots, icloud_dir->string())) {
        prefs.default_project_root = icloud_dir->string();
      }
      user_settings_->Save();
    }
  }
  ImGui::TextDisabled(
      "iCloud projects live in Documents/Yaze/iCloud on this Mac.");

  ImGui::Spacing();
  ImGui::Text("%s Sync Options", ICON_MD_SYNC);
  ImGui::Separator();

  bool use_icloud_sync = prefs.use_icloud_sync;
  if (ImGui::Checkbox("Use iCloud sync (Documents)", &use_icloud_sync)) {
    prefs.use_icloud_sync = use_icloud_sync;
    if (use_icloud_sync) {
      auto icloud_dir =
          util::PlatformPaths::GetUserDocumentsSubdirectory("iCloud");
      if (icloud_dir.ok()) {
        AddUniquePath(&roots, icloud_dir->string());
        prefs.default_project_root = icloud_dir->string();
      }
    }
    user_settings_->Save();
  }

  bool use_files_app = prefs.use_files_app;
  if (ImGui::Checkbox("Prefer Files app on iOS", &use_files_app)) {
    prefs.use_files_app = use_files_app;
    user_settings_->Save();
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
    ImGui::SetTooltip(
        "Display ROM, session, cursor, and zoom info at bottom of window");
  }
}

void SettingsPanel::DrawEditorBehavior() {
  if (!user_settings_)
    return;

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

  if (ImGui::SliderInt("Limit", &user_settings_->prefs().recent_files_limit, 5,
                       50)) {
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
  if (ImGui::Checkbox("Use HMagic sprite names (expanded)",
                      &user_settings_->prefs().prefer_hmagic_sprite_names)) {
    yaze::zelda3::SetPreferHmagicSpriteNames(
        user_settings_->prefs().prefer_hmagic_sprite_names);
    user_settings_->Save();
  }
}

void SettingsPanel::DrawPerformanceSettings() {
  if (!user_settings_)
    return;

  ImGui::Text("%s Graphics", ICON_MD_IMAGE);
  ImGui::Separator();

  if (ImGui::Checkbox("V-Sync", &user_settings_->prefs().vsync)) {
    user_settings_->Save();
  }

  if (ImGui::SliderInt("Target FPS", &user_settings_->prefs().target_fps, 30,
                       144)) {
    user_settings_->Save();
  }

  ImGui::Spacing();
  ImGui::Text("%s Memory", ICON_MD_MEMORY);
  ImGui::Separator();

  if (ImGui::SliderInt("Cache Size (MB)",
                       &user_settings_->prefs().cache_size_mb, 128, 2048)) {
    user_settings_->Save();
  }

  if (ImGui::SliderInt("Undo History",
                       &user_settings_->prefs().undo_history_size, 10, 200)) {
    user_settings_->Save();
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Current FPS: %.1f", ImGui::GetIO().Framerate);
  ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
}

void SettingsPanel::DrawAIAgentSettings() {
  if (!user_settings_)
    return;

  auto& prefs = user_settings_->prefs();
  auto& hosts = prefs.ai_hosts;
  static int selected_host_index = -1;

  auto draw_key_row = [&](const char* label, std::string* key,
                          const char* env_var, const char* id) {
    ImGui::PushID(id);
    ImGui::Text("%s", label);
    const ImVec2 button_size = ImGui::CalcTextSize(ICON_MD_SYNC " Env");
    float env_button_width =
        button_size.x + ImGui::GetStyle().FramePadding.x * 2.0f;
    float input_width = ImGui::GetContentRegionAvail().x - env_button_width -
                        ImGui::GetStyle().ItemSpacing.x;
    bool stack = input_width < 160.0f;
    ImGui::SetNextItemWidth(stack ? -1.0f : input_width);
    if (ImGui::InputTextWithHint("##key", "API key...", key,
                                 ImGuiInputTextFlags_Password)) {
      user_settings_->Save();
    }
    if (!stack) {
      ImGui::SameLine();
    }
    if (ImGui::SmallButton(ICON_MD_SYNC " Env")) {
      const char* env_key = std::getenv(env_var);
      if (env_key) {
        *key = env_key;
        user_settings_->Save();
      }
    }
    ImGui::Spacing();
    ImGui::PopID();
  };

  ImGui::Text("%s Provider Keys", ICON_MD_VPN_KEY);
  ImGui::Separator();
  draw_key_row("OpenAI", &prefs.openai_api_key, "OPENAI_API_KEY", "openai_key");
  draw_key_row("Anthropic", &prefs.anthropic_api_key, "ANTHROPIC_API_KEY",
               "anthropic_key");
  draw_key_row("Google (Gemini)", &prefs.gemini_api_key, "GEMINI_API_KEY",
               "gemini_key");
  ImGui::Spacing();

  // Provider selection
  ImGui::Text("%s Provider Defaults (legacy)", ICON_MD_CLOUD);
  ImGui::Separator();

  const char* providers[] = {"Ollama (Local)", "Gemini (Cloud)",
                             "Mock (Testing)"};
  if (ImGui::Combo("##Provider", &prefs.ai_provider, providers,
                   IM_ARRAYSIZE(providers))) {
    user_settings_->Save();
  }

  ImGui::Spacing();
  ImGui::Text("%s Host Routing", ICON_MD_STORAGE);
  ImGui::Separator();

  const char* active_preview = "None";
  const char* remote_preview = "None";
  for (const auto& host : hosts) {
    if (!prefs.active_ai_host_id.empty() &&
        host.id == prefs.active_ai_host_id) {
      active_preview = host.label.c_str();
    }
    if (!prefs.remote_build_host_id.empty() &&
        host.id == prefs.remote_build_host_id) {
      remote_preview = host.label.c_str();
    }
  }

  if (ImGui::BeginCombo("Active Host", active_preview)) {
    for (size_t i = 0; i < hosts.size(); ++i) {
      const bool is_selected = (!prefs.active_ai_host_id.empty() &&
                                hosts[i].id == prefs.active_ai_host_id);
      if (ImGui::Selectable(hosts[i].label.c_str(), is_selected)) {
        prefs.active_ai_host_id = hosts[i].id;
        if (prefs.remote_build_host_id.empty()) {
          prefs.remote_build_host_id = hosts[i].id;
        }
        user_settings_->Save();
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  if (ImGui::BeginCombo("Remote Build Host", remote_preview)) {
    for (size_t i = 0; i < hosts.size(); ++i) {
      const bool is_selected = (!prefs.remote_build_host_id.empty() &&
                                hosts[i].id == prefs.remote_build_host_id);
      if (ImGui::Selectable(hosts[i].label.c_str(), is_selected)) {
        prefs.remote_build_host_id = hosts[i].id;
        user_settings_->Save();
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::Spacing();
  ImGui::Text("%s AI Hosts", ICON_MD_STORAGE);
  ImGui::Separator();

  if (selected_host_index >= static_cast<int>(hosts.size())) {
    selected_host_index = hosts.empty() ? -1 : 0;
  }
  if (selected_host_index < 0 && !hosts.empty()) {
    for (size_t i = 0; i < hosts.size(); ++i) {
      if (!prefs.active_ai_host_id.empty() &&
          hosts[i].id == prefs.active_ai_host_id) {
        selected_host_index = static_cast<int>(i);
        break;
      }
    }
    if (selected_host_index < 0) {
      selected_host_index = 0;
    }
  }

  ImGui::BeginChild("##ai_host_list", ImVec2(0, 150), true);
  for (size_t i = 0; i < hosts.size(); ++i) {
    const bool is_selected = static_cast<int>(i) == selected_host_index;
    std::string label = hosts[i].label;
    if (hosts[i].id == prefs.active_ai_host_id) {
      label += " (active)";
    }
    if (hosts[i].id == prefs.remote_build_host_id) {
      label += " (build)";
    }
    if (ImGui::Selectable(label.c_str(), is_selected)) {
      selected_host_index = static_cast<int>(i);
    }
    std::string tags = BuildHostTagString(hosts[i]);
    if (!tags.empty()) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", tags.c_str());
    }
  }
  ImGui::EndChild();

  auto add_host = [&](UserSettings::Preferences::AiHost host) {
    if (host.id.empty()) {
      host.id = absl::StrFormat("host-%zu", hosts.size() + 1);
    }
    hosts.push_back(host);
    selected_host_index = static_cast<int>(hosts.size() - 1);
    if (prefs.active_ai_host_id.empty()) {
      prefs.active_ai_host_id = host.id;
    }
    if (prefs.remote_build_host_id.empty()) {
      prefs.remote_build_host_id = host.id;
    }
    user_settings_->Save();
  };

  if (ImGui::Button(ICON_MD_ADD " Add Host")) {
    UserSettings::Preferences::AiHost host;
    host.label = "New Host";
    host.base_url = "http://localhost:1234";
    host.api_type = "openai";
    add_host(host);
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DELETE " Remove") && selected_host_index >= 0 &&
      selected_host_index < static_cast<int>(hosts.size())) {
    const std::string removed_id = hosts[selected_host_index].id;
    hosts.erase(hosts.begin() + selected_host_index);
    if (prefs.active_ai_host_id == removed_id) {
      prefs.active_ai_host_id = hosts.empty() ? "" : hosts.front().id;
    }
    if (prefs.remote_build_host_id == removed_id) {
      prefs.remote_build_host_id = prefs.active_ai_host_id;
    }
    selected_host_index =
        hosts.empty()
            ? -1
            : std::min(selected_host_index, static_cast<int>(hosts.size() - 1));
    user_settings_->Save();
  }

  ImGui::SameLine();
  if (ImGui::Button("Add LM Studio")) {
    UserSettings::Preferences::AiHost host;
    host.label = "LM Studio (local)";
    host.base_url = "http://localhost:1234";
    host.api_type = "lmstudio";
    host.supports_tools = true;
    host.supports_streaming = true;
    add_host(host);
  }
  ImGui::SameLine();
  if (ImGui::Button("Add Ollama")) {
    UserSettings::Preferences::AiHost host;
    host.label = "Ollama (local)";
    host.base_url = "http://localhost:11434";
    host.api_type = "ollama";
    host.supports_tools = true;
    host.supports_streaming = true;
    add_host(host);
  }

  static std::string tailscale_host;
  ImGui::InputTextWithHint("##tailscale_host", "host.ts.net:1234",
                           &tailscale_host);
  ImGui::SameLine();
  if (ImGui::Button("Add Tailscale Host")) {
    std::string trimmed =
        std::string(absl::StripAsciiWhitespace(tailscale_host));
    if (!trimmed.empty()) {
      UserSettings::Preferences::AiHost host;
      host.label = "Tailscale Host";
      if (absl::StrContains(trimmed, "://")) {
        host.base_url = trimmed;
      } else {
        host.base_url = "http://" + trimmed;
      }
      host.api_type = "openai";
      host.supports_tools = true;
      host.supports_streaming = true;
      host.allow_insecure = true;
      add_host(host);
      tailscale_host.clear();
    }
  }

  if (selected_host_index >= 0 &&
      selected_host_index < static_cast<int>(hosts.size())) {
    auto& host = hosts[static_cast<size_t>(selected_host_index)];
    ImGui::Spacing();
    ImGui::Text("Host Details");
    ImGui::Separator();
    if (ImGui::InputText("Label", &host.label)) {
      user_settings_->Save();
    }
    if (ImGui::InputText("Base URL", &host.base_url)) {
      user_settings_->Save();
    }

    const char* api_types[] = {"openai",    "ollama",   "gemini",
                               "anthropic", "lmstudio", "grpc"};
    int api_index = 0;
    for (int i = 0; i < IM_ARRAYSIZE(api_types); ++i) {
      if (host.api_type == api_types[i]) {
        api_index = i;
        break;
      }
    }
    if (ImGui::Combo("API Type", &api_index, api_types,
                     IM_ARRAYSIZE(api_types))) {
      host.api_type = api_types[api_index];
      user_settings_->Save();
    }

    if (ImGui::InputText("API Key", &host.api_key,
                         ImGuiInputTextFlags_Password)) {
      user_settings_->Save();
    }
    if (ImGui::InputText("Keychain ID", &host.credential_id)) {
      user_settings_->Save();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Use Host ID")) {
      host.credential_id = host.id;
      user_settings_->Save();
    }
    if (!host.credential_id.empty() && host.api_key.empty()) {
      ImGui::TextDisabled("Keychain lookup enabled (leave API key empty).");
    }

    if (ImGui::Checkbox("Supports Vision", &host.supports_vision)) {
      user_settings_->Save();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Supports Tools", &host.supports_tools)) {
      user_settings_->Save();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Supports Streaming", &host.supports_streaming)) {
      user_settings_->Save();
    }
    if (ImGui::Checkbox("Allow Insecure HTTP", &host.allow_insecure)) {
      user_settings_->Save();
    }
  }

  ImGui::Spacing();
  ImGui::Text("%s Local Model Paths", ICON_MD_FOLDER);
  ImGui::Separator();

  auto& model_paths = prefs.ai_model_paths;
  static int selected_model_path = -1;
  static std::string new_model_path;

  if (model_paths.empty()) {
    ImGui::TextDisabled("No model paths configured.");
  }

  if (ImGui::BeginChild("ModelPathsList", ImVec2(0, 120), true)) {
    for (size_t i = 0; i < model_paths.size(); ++i) {
      std::string label =
          util::PlatformPaths::NormalizePathForDisplay(model_paths[i]);
      if (ImGui::Selectable(label.c_str(),
                            selected_model_path == static_cast<int>(i))) {
        selected_model_path = static_cast<int>(i);
      }
    }
  }
  ImGui::EndChild();

  const bool has_model_selection =
      selected_model_path >= 0 &&
      selected_model_path < static_cast<int>(model_paths.size());
  if (has_model_selection) {
    if (ImGui::Button(ICON_MD_DELETE " Remove")) {
      model_paths.erase(model_paths.begin() + selected_model_path);
      selected_model_path =
          model_paths.empty()
              ? -1
              : std::min(selected_model_path,
                         static_cast<int>(model_paths.size() - 1));
      user_settings_->Save();
    }
  }

  ImGui::Spacing();
  ImGui::InputTextWithHint("##model_path_add", "Add folder path...",
                           &new_model_path);
  if (ImGui::Button(ICON_MD_ADD " Add Path")) {
    const std::string trimmed =
        std::string(absl::StripAsciiWhitespace(new_model_path));
    if (!trimmed.empty() && AddUniquePath(&model_paths, trimmed)) {
      user_settings_->Save();
      new_model_path.clear();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Browse")) {
    const std::string folder = util::FileDialogWrapper::ShowOpenFolderDialog();
    if (!folder.empty() && AddUniquePath(&model_paths, folder)) {
      user_settings_->Save();
    }
  }

  ImGui::Spacing();
  ImGui::Text("%s Quick Add", ICON_MD_BOLT);
  ImGui::Separator();
  const auto home_dir = util::PlatformPaths::GetHomeDirectory();
  if (ImGui::Button(ICON_MD_HOME " Add ~/models")) {
    if (!home_dir.empty() && home_dir != ".") {
      if (AddUniquePath(&model_paths, (home_dir / "models").string())) {
        user_settings_->Save();
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Add ~/.lmstudio/models")) {
    if (!home_dir.empty() && home_dir != ".") {
      if (AddUniquePath(&model_paths,
                        (home_dir / ".lmstudio" / "models").string())) {
        user_settings_->Save();
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Add ~/.ollama/models")) {
    if (!home_dir.empty() && home_dir != ".") {
      if (AddUniquePath(&model_paths,
                        (home_dir / ".ollama" / "models").string())) {
        user_settings_->Save();
      }
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

  if (ImGui::SliderInt("Max Tokens", &user_settings_->prefs().ai_max_tokens,
                       256, 8192)) {
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
  if (ImGui::TreeNodeEx(ICON_MD_KEYBOARD " Shortcuts",
                        ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::InputTextWithHint("##shortcut_filter", "Filter shortcuts...",
                             &shortcut_filter_);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Filter by action name or key combo");
    }
    ImGui::Spacing();

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
    ImGui::TextDisabled(
        "Tip: Use Cmd/Opt labels on macOS or Ctrl/Alt on Windows/Linux. "
        "Function keys and symbols (/, -) are supported.");
    ImGui::TreePop();
  }
}

bool SettingsPanel::MatchesShortcutFilter(const std::string& text) const {
  if (shortcut_filter_.empty()) {
    return true;
  }
  std::string haystack = absl::AsciiStrToLower(text);
  std::string needle = absl::AsciiStrToLower(shortcut_filter_);
  return absl::StrContains(haystack, needle);
}

void SettingsPanel::DrawGlobalShortcuts() {
  if (!shortcut_manager_ || !user_settings_) {
    ImGui::TextDisabled("Not available");
    return;
  }

  auto shortcuts =
      shortcut_manager_->GetShortcutsByScope(Shortcut::Scope::kGlobal);
  if (shortcuts.empty()) {
    ImGui::TextDisabled("No global shortcuts registered.");
    return;
  }

  static std::unordered_map<std::string, std::string> editing;

  bool has_match = false;
  for (const auto& sc : shortcuts) {
    std::string label = sc.name;
    std::string keys = PrintShortcut(sc.keys);
    if (!MatchesShortcutFilter(label) && !MatchesShortcutFilter(keys)) {
      continue;
    }
    has_match = true;
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
  if (!has_match) {
    ImGui::TextDisabled("No shortcuts match the current filter.");
  }
}

void SettingsPanel::DrawEditorShortcuts() {
  if (!shortcut_manager_ || !user_settings_) {
    ImGui::TextDisabled("Not available");
    return;
  }

  auto shortcuts =
      shortcut_manager_->GetShortcutsByScope(Shortcut::Scope::kEditor);
  std::map<std::string, std::vector<Shortcut>> grouped;
  static std::unordered_map<std::string, std::string> editing;

  for (const auto& sc : shortcuts) {
    auto pos = sc.name.find(".");
    std::string group =
        pos != std::string::npos ? sc.name.substr(0, pos) : "general";
    grouped[group].push_back(sc);
  }
  bool has_match = false;
  for (const auto& [group, list] : grouped) {
    std::vector<Shortcut> filtered;
    filtered.reserve(list.size());
    for (const auto& sc : list) {
      std::string keys = PrintShortcut(sc.keys);
      if (MatchesShortcutFilter(sc.name) || MatchesShortcutFilter(keys)) {
        filtered.push_back(sc);
      }
    }
    if (filtered.empty()) {
      continue;
    }
    has_match = true;
    if (ImGui::TreeNode(group.c_str())) {
      for (const auto& sc : filtered) {
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
        if (ImGui::InputText("##editor", &value,
                             ImGuiInputTextFlags_EnterReturnsTrue |
                                 ImGuiInputTextFlags_AutoSelectAll)) {
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
  if (!has_match) {
    ImGui::TextDisabled("No shortcuts match the current filter.");
  }
}

void SettingsPanel::DrawPanelShortcuts() {
  if (!panel_manager_ || !user_settings_) {
    ImGui::TextDisabled("Registry not available");
    return;
  }

  // Simplified shortcut editor for sidebar
  auto categories = panel_manager_->GetAllCategories();

  bool has_match = false;
  for (const auto& category : categories) {
    auto cards = panel_manager_->GetPanelsInCategory(0, category);
    std::vector<decltype(cards)::value_type> filtered_cards;
    filtered_cards.reserve(cards.size());
    for (const auto& card : cards) {
      if (MatchesShortcutFilter(card.display_name) ||
          MatchesShortcutFilter(card.card_id)) {
        filtered_cards.push_back(card);
      }
    }
    if (filtered_cards.empty()) {
      continue;
    }
    has_match = true;
    if (ImGui::TreeNode(category.c_str())) {

      for (const auto& card : filtered_cards) {
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
              user_settings_->prefs().panel_shortcuts[card.card_id] =
                  shortcut_edit_buffer_;
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
            strncpy(shortcut_edit_buffer_, current_shortcut.c_str(),
                    sizeof(shortcut_edit_buffer_) - 1);
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
  if (!has_match) {
    ImGui::TextDisabled("No shortcuts match the current filter.");
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
  if (ImGui::BeginTabBar("##PatchFolders",
                         ImGuiTabBarFlags_FittingPolicyScroll)) {
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
  if (!selected_patch_)
    return;

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
  if (!param)
    return;

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
