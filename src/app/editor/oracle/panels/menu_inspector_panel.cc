#include "app/editor/oracle/panels/menu_inspector_panel.h"

#include <filesystem>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/panel_registration.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/widgets/themed_widgets.h"
#include "core/project.h"

namespace yaze::editor {

namespace {

std::string DetermineInitialProjectPath() {
  if (auto* project = ContentRegistry::Context::current_project()) {
    if (!project->code_folder.empty()) {
      return project->GetAbsolutePath(project->code_folder);
    }
    if (!project->filepath.empty()) {
      return std::filesystem::path(project->filepath).parent_path().string();
    }
  }
  return std::filesystem::current_path().string();
}

}  // namespace

void OracleMenuInspectorPanel::EnsureProjectPath() {
  if (project_path_.empty()) {
    project_path_ = DetermineInitialProjectPath();
  }

  if (!attempted_initial_load_ && !project_path_.empty()) {
    attempted_initial_load_ = true;
    RefreshRegistry();
  }
}

void OracleMenuInspectorPanel::RefreshRegistry() {
  auto registry_or = core::BuildOracleMenuRegistry(project_path_);
  if (!registry_or.ok()) {
    has_registry_ = false;
    last_error_ = std::string(registry_or.status().message());
    status_message_.clear();
    filtered_components_.clear();
    selected_component_list_index_ = -1;
    return;
  }

  registry_ = std::move(registry_or.value());
  has_registry_ = true;
  last_error_.clear();
  status_message_ = absl::StrFormat(
      "Loaded %zu asm files, %zu bins, %zu draw routines, %zu components.",
      registry_.asm_files.size(), registry_.bins.size(),
      registry_.draw_routines.size(), registry_.components.size());

  filtered_components_.clear();
  selected_component_list_index_ = -1;
}

bool OracleMenuInspectorPanel::MatchesFilter(const std::string& value,
                                             const std::string& filter) const {
  if (filter.empty()) {
    return true;
  }
  const std::string value_lower = absl::AsciiStrToLower(value);
  const std::string filter_lower = absl::AsciiStrToLower(filter);
  return value_lower.find(filter_lower) != std::string::npos;
}

void OracleMenuInspectorPanel::DrawToolbar() {
  ImGui::InputText("Project Root", &project_path_);
  ImGui::SameLine();
  if (ImGui::Button("Auto Detect")) {
    project_path_ = DetermineInitialProjectPath();
  }
  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    RefreshRegistry();
  }
}

void OracleMenuInspectorPanel::DrawSummary() const {
  if (!has_registry_) {
    return;
  }
  ImGui::TextDisabled(
      "ASM: %zu  Bins: %zu  Draw: %zu  Components: %zu  Warnings: %zu",
      registry_.asm_files.size(), registry_.bins.size(),
      registry_.draw_routines.size(), registry_.components.size(),
      registry_.warnings.size());
}

void OracleMenuInspectorPanel::DrawBinsTab() {
  const auto& theme = AgentUI::GetTheme();
  ImGui::Checkbox("Missing bins only", &bins_missing_only_);

  if (ImGui::BeginTable("oracle_menu_bins_table", 6,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                        ImVec2(0, 320))) {
    ImGui::TableSetupColumn("Label");
    ImGui::TableSetupColumn("Bin Path");
    ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("ASM");
    ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 55.0f);
    ImGui::TableHeadersRow();

    for (const auto& entry : registry_.bins) {
      if (bins_missing_only_ && entry.exists) {
        continue;
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(entry.label.empty() ? "(unlabeled)"
                                                 : entry.label.c_str());
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(entry.resolved_bin_path.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%llu", static_cast<unsigned long long>(entry.size_bytes));
      ImGui::TableSetColumnIndex(3);
      ImGui::TextColored(
          entry.exists ? theme.status_success : theme.status_error, "%s",
          entry.exists ? "OK" : "MISSING");
      ImGui::TableSetColumnIndex(4);
      ImGui::TextUnformatted(entry.asm_path.c_str());
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%d", entry.line);
    }

    ImGui::EndTable();
  }
}

void OracleMenuInspectorPanel::DrawDrawRoutinesTab() {
  ImGui::InputText("Filter", &draw_filter_);

  if (ImGui::BeginTable("oracle_menu_draw_table", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                        ImVec2(0, 320))) {
    ImGui::TableSetupColumn("Routine");
    ImGui::TableSetupColumn("ASM");
    ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 55.0f);
    ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableHeadersRow();

    for (const auto& routine : registry_.draw_routines) {
      if (!MatchesFilter(routine.label, draw_filter_)) {
        continue;
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(routine.label.c_str());
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(routine.asm_path.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%d", routine.line);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%d", routine.references);
      ImGui::TableSetColumnIndex(4);
      ImGui::TextUnformatted(routine.local ? "local" : "global");
    }

    ImGui::EndTable();
  }
}

void OracleMenuInspectorPanel::DrawComponentsTab() {
  ImGui::InputText("Table Filter", &component_table_filter_);

  filtered_components_.clear();
  filtered_components_.reserve(registry_.components.size());
  for (const auto& component : registry_.components) {
    if (!MatchesFilter(component.table_label, component_table_filter_)) {
      continue;
    }
    filtered_components_.push_back(&component);
  }

  if (selected_component_list_index_ >=
      static_cast<int>(filtered_components_.size())) {
    selected_component_list_index_ = -1;
  }

  const float left_width = ImGui::GetContentRegionAvail().x * 0.58f;
  ImGui::BeginChild("oracle_menu_components_list", ImVec2(left_width, 340.0f),
                    ImGuiChildFlags_Borders);
  if (ImGui::BeginTable("oracle_menu_components_table", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Table");
    ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 55.0f);
    ImGui::TableSetupColumn("Row", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Col", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Ref");
    ImGui::TableHeadersRow();

    for (int i = 0; i < static_cast<int>(filtered_components_.size()); ++i) {
      const auto* component = filtered_components_[i];
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      const bool selected = (selected_component_list_index_ == i);
      std::string row_label =
          absl::StrFormat("%s##oracle_component_%d", component->table_label, i);
      if (ImGui::Selectable(row_label.c_str(), selected,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        selected_component_list_index_ = i;
        edit_row_ = component->row;
        edit_col_ = component->col;
      }

      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", component->index);
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%d", component->row);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%d", component->col);
      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%s:%d", component->asm_path.c_str(), component->line);
    }

    ImGui::EndTable();
  }
  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginChild("oracle_menu_component_editor", ImVec2(0, 340.0f),
                    ImGuiChildFlags_Borders);
  if (selected_component_list_index_ < 0 ||
      selected_component_list_index_ >=
          static_cast<int>(filtered_components_.size())) {
    ImGui::TextDisabled("Select a component row to preview/apply edits.");
    ImGui::EndChild();
    return;
  }

  const auto* component = filtered_components_[selected_component_list_index_];
  ImGui::Text("Table: %s", component->table_label.c_str());
  ImGui::Text("Index: %d", component->index);
  ImGui::Text("ASM: %s:%d", component->asm_path.c_str(), component->line);
  if (!component->note.empty()) {
    ImGui::TextDisabled("Note: %s", component->note.c_str());
  }
  ImGui::Separator();

  ImGui::InputInt("Row", &edit_row_);
  ImGui::InputInt("Col", &edit_col_);
  if (edit_row_ < 0) {
    edit_row_ = 0;
  }
  if (edit_col_ < 0) {
    edit_col_ = 0;
  }

  if (ImGui::Button("Preview")) {
    auto edit_or = core::SetOracleMenuComponentOffset(
        registry_.project_root, component->asm_path, component->table_label,
        component->index, edit_row_, edit_col_, false);
    if (!edit_or.ok()) {
      status_message_ =
          absl::StrFormat("Preview failed: %s", edit_or.status().message());
    } else {
      const auto& edit = edit_or.value();
      status_message_ = absl::StrFormat(
          "Preview %s[%d] (%d,%d) -> (%d,%d)", edit.table_label, edit.index,
          edit.old_row, edit.old_col, edit.new_row, edit.new_col);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Apply")) {
    auto edit_or = core::SetOracleMenuComponentOffset(
        registry_.project_root, component->asm_path, component->table_label,
        component->index, edit_row_, edit_col_, true);
    if (!edit_or.ok()) {
      status_message_ =
          absl::StrFormat("Apply failed: %s", edit_or.status().message());
    } else {
      const auto& edit = edit_or.value();
      status_message_ = absl::StrFormat(
          "Applied %s[%d] (%d,%d) -> (%d,%d)", edit.table_label, edit.index,
          edit.old_row, edit.old_col, edit.new_row, edit.new_col);
      RefreshRegistry();
    }
  }

  ImGui::EndChild();
}

void OracleMenuInspectorPanel::Draw(bool* /*p_open*/) {
  const auto& theme = AgentUI::GetTheme();
  EnsureProjectPath();
  DrawToolbar();

  if (!last_error_.empty()) {
    ImGui::TextColored(theme.status_error, "%s", last_error_.c_str());
    return;
  }

  if (!status_message_.empty()) {
    ImGui::TextDisabled("%s", status_message_.c_str());
  }
  DrawSummary();

  if (!has_registry_) {
    ImGui::TextDisabled("No menu registry data loaded.");
    return;
  }

  if (gui::BeginThemedTabBar("oracle_menu_tabs")) {
    if (ImGui::BeginTabItem("Bins")) {
      DrawBinsTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Draw Routines")) {
      DrawDrawRoutinesTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Components")) {
      DrawComponentsTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Warnings")) {
      if (registry_.warnings.empty()) {
        ImGui::TextDisabled("No warnings.");
      } else {
        for (const auto& warning : registry_.warnings) {
          ImGui::BulletText("%s", warning.c_str());
        }
      }
      ImGui::EndTabItem();
    }
    gui::EndThemedTabBar();
  }
}

REGISTER_PANEL(OracleMenuInspectorPanel);

}  // namespace yaze::editor
