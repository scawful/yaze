#include "app/editor/shell/dialogs/new_project_dialog.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "util/file_util.h"

namespace yaze {
namespace editor {

namespace {

struct TemplateDescriptor {
  const char* id;
  const char* icon;
  const char* name;
  const char* use_when;
  const char* what_changes;
  int skill_level;  // 1 beginner, 2 intermediate, 3 advanced
};

// Mirror of the catalog the welcome screen's template section uses. Kept
// inline here so the dialog doesn't depend on welcome_screen internals; both
// lists are short and stable, duplication is cheaper than plumbing.
constexpr TemplateDescriptor kTemplates[] = {
    {"Vanilla ROM Hack", ICON_MD_COTTAGE, "Vanilla ROM Hack",
     "Edit rooms, sprites, or graphics without custom code.",
     "Creates a project on top of the ROM. The ROM itself is only modified "
     "when you save edits inside the editors.",
     1},
    {"ZSCustomOverworld v3", ICON_MD_TERRAIN, "ZSCustomOverworld v3",
     "Resize overworld areas and add custom map features.",
     "Applies the ZSO3 patch: expanded overworld tables, extended palette "
     "and GFX storage, and custom entrances/exits.",
     2},
    {"ZSCustomOverworld v2", ICON_MD_MAP, "ZSCustomOverworld v2",
     "Port an older hack that already uses ZSO v2.",
     "Applies the legacy ZSO2 patch. Smaller feature set than v3.", 2},
    {"Randomizer Compatible", ICON_MD_SHUFFLE, "Randomizer Compatible",
     "Build a ROM that has to work with ALTTPR or similar.",
     "Keeps edits inside the surface randomizers patch over. Skips ASM "
     "hooks and overworld remapping.",
     3},
};
constexpr int kTemplateCount = sizeof(kTemplates) / sizeof(kTemplates[0]);

int FindTemplateIndex(const std::string& name) {
  if (name.empty())
    return 0;
  for (int i = 0; i < kTemplateCount; ++i) {
    if (name == kTemplates[i].id)
      return i;
  }
  return 0;
}

}  // namespace

void NewProjectDialog::Open(const std::string& initial_template) {
  selected_template_ = FindTemplateIndex(initial_template);
  // Seed the project name with the template name so users who don't care
  // about naming get something reasonable by default — they can still edit.
  std::snprintf(project_name_buffer_, sizeof(project_name_buffer_), "%s",
                kTemplates[selected_template_].name);
  rom_path_buffer_[0] = '\0';
  status_message_.clear();
  open_requested_ = true;
  just_opened_ = true;
}

void NewProjectDialog::Reset() {
  open_requested_ = false;
  just_opened_ = false;
  status_message_.clear();
}

void NewProjectDialog::ApplyTemplateSelection(const std::string& name) {
  const int new_index = FindTemplateIndex(name);
  selected_template_ = new_index;
}

bool NewProjectDialog::Draw() {
  if (!open_requested_)
    return false;

  constexpr const char* kPopupId = "##new_project_dialog";
  if (just_opened_) {
    ImGui::OpenPopup(kPopupId);
    just_opened_ = false;
  }

  ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing);
  if (!ImGui::BeginPopupModal(kPopupId, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoSavedSettings)) {
    // Popup closed externally (escape key inside ImGui's own machinery, etc.).
    Reset();
    return false;
  }

  const ImVec4 text_secondary = gui::GetTextSecondaryVec4();

  ImGui::TextUnformatted(ICON_MD_ROCKET_LAUNCH " Start a new project");
  {
    gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
    ImGui::TextWrapped(
        "Pick a template, point at the ROM it should build on, and give the "
        "project a name. The project file will be saved in your configured "
        "projects folder.");
  }
  ImGui::Separator();
  ImGui::Spacing();

  // --- Template picker ------------------------------------------------------
  ImGui::TextUnformatted("Template");
  const char* combo_preview = kTemplates[selected_template_].name;
  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo("##template_combo", combo_preview)) {
    for (int i = 0; i < kTemplateCount; ++i) {
      const bool selected = (i == selected_template_);
      const std::string label =
          absl::StrFormat("%s  %s", kTemplates[i].icon, kTemplates[i].name);
      if (ImGui::Selectable(label.c_str(), selected)) {
        selected_template_ = i;
      }
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  // Template description block — gives the user an at-a-glance sense of what
  // the checkbox actually does, same copy as the welcome screen details.
  const TemplateDescriptor& tmpl = kTemplates[selected_template_];
  {
    gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
    ImGui::TextWrapped("%s Use when: %s", ICON_MD_LIGHTBULB, tmpl.use_when);
    ImGui::TextWrapped("%s Effect: %s", ICON_MD_EDIT, tmpl.what_changes);
    const char* skill_tag = tmpl.skill_level == 1   ? "Beginner"
                            : tmpl.skill_level == 2 ? "Intermediate"
                                                    : "Advanced";
    ImGui::TextWrapped("%s Skill: %s", ICON_MD_INFO, skill_tag);
  }

  ImGui::Spacing();

  // --- ROM picker -----------------------------------------------------------
  ImGui::TextUnformatted("Source ROM");
  const float browse_width = ImGui::CalcTextSize(" Browse…").x +
                             ImGui::CalcTextSize(ICON_MD_FOLDER_OPEN).x +
                             ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
  ImGui::SetNextItemWidth(-browse_width - ImGui::GetStyle().ItemSpacing.x);
  ImGui::InputText("##rom_path", rom_path_buffer_, sizeof(rom_path_buffer_));
  ImGui::SameLine();
  if (ImGui::Button(
          absl::StrFormat("%s Browse…", ICON_MD_FOLDER_OPEN).c_str())) {
    auto picked = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!picked.empty()) {
      std::snprintf(rom_path_buffer_, sizeof(rom_path_buffer_), "%s",
                    picked.c_str());
      status_message_.clear();
    }
  }

  ImGui::Spacing();

  // --- Project name ---------------------------------------------------------
  ImGui::TextUnformatted("Project name");
  ImGui::SetNextItemWidth(-1);
  ImGui::InputText("##project_name", project_name_buffer_,
                   sizeof(project_name_buffer_));
  {
    gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
    ImGui::TextWrapped(
        "Used as the project display name and to derive the project filename. "
        "Spaces are replaced with underscores.");
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // --- Actions --------------------------------------------------------------
  const std::string rom_path(rom_path_buffer_);
  const std::string project_name(project_name_buffer_);
  const bool rom_valid = !rom_path.empty() && std::filesystem::exists(rom_path);
  const bool name_valid = !project_name.empty();
  const bool can_create = rom_valid && name_valid;

  if (!can_create)
    ImGui::BeginDisabled();
  if (ImGui::Button(ICON_MD_ROCKET_LAUNCH " Create project", ImVec2(180, 0))) {
    if (create_callback_) {
      create_callback_(kTemplates[selected_template_].id, rom_path,
                       project_name);
    }
    ImGui::CloseCurrentPopup();
    Reset();
    ImGui::EndPopup();
    return false;
  }
  if (!can_create)
    ImGui::EndDisabled();

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_CLOSE " Cancel", ImVec2(100, 0)) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape, /*repeat=*/false)) {
    ImGui::CloseCurrentPopup();
    Reset();
    ImGui::EndPopup();
    return false;
  }

  // Inline validation hint — more discoverable than a disabled button alone.
  if (!can_create) {
    ImGui::SameLine();
    const ImVec4 warn = gui::ConvertColorToImVec4(
        gui::ThemeManager::Get().GetCurrentTheme().warning);
    gui::StyleColorGuard warn_guard(ImGuiCol_Text, warn);
    if (!rom_valid && rom_path.empty()) {
      ImGui::TextUnformatted(ICON_MD_WARNING " Pick a ROM to continue");
    } else if (!rom_valid) {
      ImGui::TextUnformatted(ICON_MD_WARNING " ROM path does not exist");
    } else if (!name_valid) {
      ImGui::TextUnformatted(ICON_MD_WARNING " Give the project a name");
    }
  } else if (!status_message_.empty()) {
    ImGui::SameLine();
    ImGui::TextUnformatted(status_message_.c_str());
  }

  ImGui::EndPopup();
  return true;
}

}  // namespace editor
}  // namespace yaze
