#include "app/editor/planning/planning_editor.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/events/core_events.h"
#include "app/editor/registry/content_registry.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "core/project.h"
#include "core/project_plan_agent.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze::editor {
namespace {

constexpr const char* kCategory = "Planning";
constexpr const char* kMilestoneStatuses[] = {"planned", "active", "done"};
constexpr const char* kItemStatuses[] = {"implemented", "prototype", "decided",
                                         "proposed", "open"};
constexpr const char* kDecisionStatuses[] = {"open", "decided", "dropped"};
constexpr const char* kStepKinds[] = {"story",  "errand", "dungeon",
                                      "shrine", "boss",   "optional"};

class PlanningPanel : public WindowContent {
 public:
  PlanningPanel(std::string id, std::string name, const char* icon,
                int priority, std::function<void()> draw)
      : id_(std::move(id)),
        name_(std::move(name)),
        icon_(icon),
        priority_(priority),
        draw_(std::move(draw)) {}

  std::string GetId() const override { return id_; }
  std::string GetDisplayName() const override { return name_; }
  std::string GetIcon() const override { return icon_; }
  std::string GetEditorCategory() const override { return kCategory; }
  WindowPresentationPolicy GetPresentationPolicy() const override {
    return WindowPresentationPolicy::CoreWorkspace();
  }
  int GetPriority() const override { return priority_; }
  bool IsVisibleByDefault() const override { return true; }
  float GetPreferredWidth() const override { return 640.0f; }
  void Draw(bool* /*p_open*/) override {
    if (draw_) {
      draw_();
    }
  }

 private:
  std::string id_;
  std::string name_;
  std::string icon_;
  int priority_;
  std::function<void()> draw_;
};

template <size_t N>
bool StatusCombo(const char* label, std::string* value,
                 const char* const (&options)[N]) {
  bool changed = false;
  if (ImGui::BeginCombo(label, value->empty() ? "-" : value->c_str())) {
    for (const char* option : options) {
      const bool selected = *value == option;
      if (ImGui::Selectable(option, selected)) {
        *value = option;
        changed = true;
      }
    }
    ImGui::EndCombo();
  }
  return changed;
}

ImVec4 StatusColor(const std::string& status) {
  if (status == "implemented" || status == "done" || status == "decided") {
    return gui::GetSuccessColor();
  }
  if (status == "prototype" || status == "active") {
    return gui::GetInfoColor();
  }
  if (status == "open") {
    return gui::GetWarningColor();
  }
  return gui::GetDisabledColor();
}

std::string Today() {
  return absl::FormatTime("%Y-%m-%d", absl::Now(), absl::LocalTimeZone());
}

std::string DaysUntil(const std::string& date) {
  absl::Time due;
  std::string error;
  if (date.empty() ||
      !absl::ParseTime("%Y-%m-%d", date, absl::LocalTimeZone(), &due, &error)) {
    return "";
  }
  const int64_t days = absl::ToInt64Hours(due - absl::Now()) / 24;
  if (days < 0) {
    return absl::StrFormat("%d days past", -days);
  }
  return absl::StrFormat("%d days left", days);
}

std::string JoinTargets(const std::vector<std::string>& targets) {
  return absl::StrJoin(targets, ", ");
}

std::vector<std::string> SplitTargets(const std::string& text) {
  std::vector<std::string> out;
  for (absl::string_view part :
       absl::StrSplit(text, ',', absl::SkipWhitespace())) {
    out.emplace_back(absl::StripAsciiWhitespace(part));
  }
  return out;
}

std::string StepLabel(const core::PlanRouteStep& step) {
  return step.code.empty() ? step.title : step.code;
}

std::string JsonString(const core::PlanJson& obj, const char* key,
                       const char* fallback) {
  auto it = obj.find(key);
  return it != obj.end() && it->is_string() ? it->get<std::string>()
                                            : std::string(fallback);
}

}  // namespace

void PlanningEditor::Initialize() {
  if (panels_registered_ || !dependencies_.window_manager) {
    return;
  }
  auto* window_manager = dependencies_.window_manager;
  window_manager->RegisterWindowContent(std::make_unique<PlanningPanel>(
      "planning.overview", "Plan Overview", ICON_MD_DASHBOARD, 10,
      [this]() { DrawOverview(); }));
  window_manager->RegisterWindowContent(std::make_unique<PlanningPanel>(
      "planning.route", "Route", ICON_MD_ROUTE, 20, [this]() { DrawRoute(); }));
  panels_registered_ = true;
}

absl::Status PlanningEditor::Update() {
  return SyncWithProject();
}

absl::Status PlanningEditor::Save() {
  if (!plan_ || !dirty_ || plan_path_.empty()) {
    return absl::OkStatus();
  }
  if (!project() || project()->filepath != project_path_ ||
      project()->PlanningFilePath() != plan_path_) {
    return absl::FailedPreconditionError(
        "Restore the plan's project before saving its edits");
  }
  std::string expected_source = loaded_source_;
  std::string disk_source;
  if (core::ProjectPlan::LoadFromFile(plan_path_, &disk_source).ok() &&
      disk_source != loaded_source_) {
    if (!core::AdoptPlanAgentOnlyChanges(&*plan_, loaded_source_,
                                         disk_source)) {
      save_error_ =
          "The plan file changed outside agent status since it was loaded. "
          "Your edits are kept; copy them, then Reload to see the file.";
      return absl::AbortedError(save_error_);
    }
    expected_source = std::move(disk_source);
  }
  absl::Status status = plan_->SaveToFile(plan_path_, expected_source);
  if (!status.ok()) {
    save_error_ = std::string(status.message());
    return status;
  }
  save_error_.clear();
  loaded_source_ = plan_->Serialize();
  dirty_ = false;
  return absl::OkStatus();
}

absl::Status PlanningEditor::SyncWithProject() {
  const std::string path = project() ? project()->PlanningFilePath() : "";
  const std::string project_path = project() ? project()->filepath : "";
  if (path == plan_path_ && project_path == project_path_) {
    return absl::OkStatus();
  }
  if (dirty_) {
    save_error_ = "Save or discard plan edits before replacing their project";
    return absl::FailedPreconditionError(save_error_);
  }
  plan_path_ = path;
  project_path_ = project_path;
  loaded_source_.clear();
  plan_.reset();
  load_error_.clear();
  dirty_ = false;
  selected_step_ = -1;
  selected_item_ = -1;
  if (!plan_path_.empty()) {
    return Reload();
  }
  return absl::OkStatus();
}

absl::Status PlanningEditor::Reload(bool discard_changes) {
  if (dirty_ && !discard_changes) {
    save_error_ = "Save or explicitly discard plan edits before reloading";
    return absl::FailedPreconditionError(save_error_);
  }
  std::string source;
  auto loaded = core::ProjectPlan::LoadFromFile(plan_path_, &source);
  if (!loaded.ok()) {
    load_error_ = std::string(loaded.status().message());
    return loaded.status();
  }
  plan_ = std::move(*loaded);
  loaded_source_ = std::move(source);
  load_error_.clear();
  save_error_.clear();
  dirty_ = false;
  if (selected_step_ >= static_cast<int>(plan_->route_steps.size())) {
    selected_step_ = -1;
  }
  if (selected_item_ >= static_cast<int>(plan_->route_items.size())) {
    selected_item_ = -1;
  }
  return absl::OkStatus();
}

void PlanningEditor::DiscardProjectDraftChanges() {
  dirty_ = false;
  plan_.reset();
  plan_path_.clear();
  project_path_.clear();
  loaded_source_.clear();
  save_error_.clear();
  (void)SyncWithProject();
}

void PlanningEditor::DrawNoPlan() {
  if (!project() || project()->filepath.empty()) {
    ImGui::TextWrapped(
        "Open a .yaze project to plan it. The plan is a JSON file named by "
        "the project's [planning] section.");
    return;
  }
  if (!load_error_.empty()) {
    gui::ColoredTextF(gui::GetErrorColor(), "%s", load_error_.c_str());
    if (ImGui::Button(ICON_MD_REFRESH " Retry")) {
      (void)Reload();
    }
    ImGui::Separator();
  }
  ImGui::TextWrapped(
      load_error_.empty()
          ? "This project has no plan file. Create one, or set [planning] "
            "file= in the .yaze file."
          : "The linked plan could not be loaded. Retry after repairing it, or "
            "create a plan at a new path. Existing files are never replaced.");
  if (new_plan_path_.empty()) {
    const std::filesystem::path project_path(project()->filepath);
    new_plan_path_ = (project_path.parent_path() /
                      (project_path.stem().string() + ".plan.json"))
                         .string();
  }
  ImGui::SetNextItemWidth(-1);
  ImGui::InputText("##new_plan_path", &new_plan_path_);
  if (ImGui::Button(ICON_MD_ADD " Create plan and link it to the project")) {
    absl::Status status =
        core::ProjectPlan::CreateAndLink(project(), new_plan_path_);
    if (!status.ok()) {
      load_error_ = std::string(status.message());
    } else {
      (void)SyncWithProject();
    }
  }
}

bool PlanningEditor::DrawPlanHeader() {
  (void)SyncWithProject();
  if (!plan_) {
    DrawNoPlan();
    return false;
  }
  ImGui::TextDisabled("%s", plan_path_.c_str());
  ImGui::SameLine();
  ImGui::BeginDisabled(!dirty_);
  if (ImGui::SmallButton(ICON_MD_SAVE " Save plan")) {
    absl::Status status = Save();
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show(
          status.ok() ? "Plan saved" : std::string(status.message()),
          status.ok() ? ToastType::kSuccess : ToastType::kError);
    }
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_REFRESH " Reload")) {
    if (dirty_) {
      ImGui::OpenPopup("Discard plan edits?");
    } else {
      (void)Reload();
    }
  }
  if (ImGui::BeginPopupModal("Discard plan edits?", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Reload discards the unsaved plan edits.");
    if (ImGui::Button("Discard and reload")) {
      (void)Reload(true);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Keep editing")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (dirty_) {
    ImGui::SameLine();
    gui::ColoredText("unsaved", gui::GetModifiedColor());
  }
  if (!save_error_.empty()) {
    gui::ColoredTextF(gui::GetErrorColor(), "%s", save_error_.c_str());
  }
  return plan_.has_value();
}

void PlanningEditor::DrawOverview() {
  if (!DrawPlanHeader()) {
    return;
  }
  if (!plan_->title.empty()) {
    ImGui::TextUnformatted(plan_->title.c_str());
  }
  if (!plan_->about.empty()) {
    ImGui::TextWrapped("%s", plan_->about.c_str());
  }
  ImGui::Separator();
  DrawGoal();
  DrawMilestones();
  DrawRouteStrip();
  DrawDecisionQueue();
  DrawAgentStatus();
  DrawReferences();
}

void PlanningEditor::DrawGoal() {
  auto& goal = plan_->goal;
  ImGui::SeparatorText(ICON_MD_FLAG " Goal");
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputTextWithHint("##goal_title", "What this push delivers",
                               &goal.title)) {
    MarkDirty();
  }
  ImGui::SetNextItemWidth(120);
  if (ImGui::InputTextWithHint("##goal_due", "YYYY-MM-DD", &goal.due)) {
    MarkDirty();
  }
  const std::string left = DaysUntil(goal.due);
  if (!left.empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s", left.c_str());
  }
  if (ImGui::InputTextMultiline("##goal_detail", &goal.detail,
                                ImVec2(-1, ImGui::GetTextLineHeight() * 3))) {
    MarkDirty();
  }
}

void PlanningEditor::DrawMilestones() {
  ImGui::SeparatorText(ICON_MD_TIMELINE " Milestones");
  const core::PlanMilestone* current = plan_->CurrentMilestone(Today());
  if (current) {
    gui::ColoredTextF(gui::GetInfoColor(), "Now: %s", current->title.c_str());
    if (!current->detail.empty()) {
      ImGui::TextWrapped("%s", current->detail.c_str());
    }
  }
  if (!ImGui::TreeNode("##milestones", "All milestones (%zu)",
                       plan_->milestones.size())) {
    return;
  }
  if (ImGui::BeginTable("##milestone_table", 4,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Milestone", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Start", ImGuiTableColumnFlags_WidthFixed, 90);
    ImGui::TableSetupColumn("End", ImGuiTableColumnFlags_WidthFixed, 90);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 110);
    ImGui::TableHeadersRow();
    for (size_t i = 0; i < plan_->milestones.size(); ++i) {
      auto& m = plan_->milestones[i];
      ImGui::PushID(static_cast<int>(i));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(-1);
      if (ImGui::InputText("##title", &m.title)) {
        MarkDirty();
      }
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(-1);
      if (ImGui::InputText("##start", &m.start)) {
        MarkDirty();
      }
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(-1);
      if (ImGui::InputText("##end", &m.end)) {
        MarkDirty();
      }
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(-1);
      if (StatusCombo("##status", &m.status, kMilestoneStatuses)) {
        MarkDirty();
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  if (ImGui::SmallButton(ICON_MD_ADD " Milestone")) {
    core::PlanMilestone m;
    m.id = plan_->NewMilestoneId();
    m.title = "New milestone";
    m.status = "planned";
    plan_->milestones.push_back(std::move(m));
    MarkDirty();
  }
  ImGui::TreePop();
}

void PlanningEditor::DrawRouteStrip() {
  ImGui::SeparatorText(ICON_MD_ROUTE " Route");
  if (plan_->route_steps.empty()) {
    ImGui::TextDisabled("No route steps yet. Add them in the Route window.");
    return;
  }
  ImGui::BeginChild("##route_strip",
                    ImVec2(0, ImGui::GetFrameHeightWithSpacing() * 1.6f), false,
                    ImGuiWindowFlags_HorizontalScrollbar);
  for (size_t i = 0; i < plan_->route_steps.size(); ++i) {
    const auto& step = plan_->route_steps[i];
    if (i > 0) {
      ImGui::SameLine(0, 2);
      ImGui::TextDisabled(ICON_MD_CHEVRON_RIGHT);
      ImGui::SameLine(0, 2);
    }
    ImGui::PushID(static_cast<int>(i));
    const bool selected = selected_step_ == static_cast<int>(i);
    if (selected) {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button(StepLabel(step).c_str())) {
      selected_step_ = static_cast<int>(i);
      selected_item_ = -1;
    }
    if (selected) {
      ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s\n%s", step.title.c_str(), step.note.c_str());
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
  if (selected_step_ >= 0 &&
      selected_step_ < static_cast<int>(plan_->route_steps.size())) {
    const auto& step = plan_->route_steps[selected_step_];
    ImGui::Text("%s", step.title.c_str());
    if (!step.note.empty()) {
      ImGui::TextWrapped("%s", step.note.c_str());
    }
    DrawTargets(step.targets, "strip");
    DrawAgentObservation("step:" + step.id);
    for (const auto* item : plan_->ItemsAtStep(step.id)) {
      ImGui::BulletText("%s", item->name.c_str());
      ImGui::SameLine();
      gui::ColoredText(item->status.c_str(), StatusColor(item->status));
    }
  }
}

void PlanningEditor::DrawDecisionQueue() {
  const auto open = plan_->OpenDecisions();
  ImGui::SeparatorText(
      absl::StrFormat(ICON_MD_CHECKLIST " Decisions (%zu open)", open.size())
          .c_str());
  for (size_t i = 0; i < plan_->decisions.size(); ++i) {
    auto& d = plan_->decisions[i];
    ImGui::PushID(static_cast<int>(i));
    const bool is_open = d.status.empty() || d.status == "open";
    std::string header = d.priority > 0
                             ? absl::StrFormat("%d. %s", d.priority, d.title)
                             : d.title;
    if (!is_open) {
      header += absl::StrFormat(" (%s)", d.status);
    }
    ImGui::PushStyleColor(ImGuiCol_Text,
                          is_open ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
                                  : gui::GetDisabledColor());
    const bool expanded = ImGui::TreeNode("##decision", "%s", header.c_str());
    ImGui::PopStyleColor();
    if (expanded) {
      if (!d.recommendation.empty()) {
        ImGui::TextWrapped("Recommendation: %s", d.recommendation.c_str());
      }
      ImGui::SetNextItemWidth(120);
      if (StatusCombo("Status", &d.status, kDecisionStatuses)) {
        MarkDirty();
      }
      if (ImGui::InputTextMultiline(
              "##answer", &d.answer,
              ImVec2(-1, ImGui::GetTextLineHeight() * 3))) {
        MarkDirty();
      }
      DrawTargets(d.targets, "decision");
      for (const auto& ref : d.refs) {
        ImGui::TextDisabled("%s", ref.c_str());
      }
      DrawAgentObservation("decision:" + d.id);
      ImGui::TreePop();
    }
    ImGui::PopID();
  }
  if (ImGui::SmallButton(ICON_MD_ADD " Decision")) {
    core::PlanDecision d;
    d.id = plan_->NewDecisionId();
    d.title = "New decision";
    d.status = "open";
    plan_->decisions.push_back(std::move(d));
    MarkDirty();
  }
}

void PlanningEditor::DrawReferences() {
  if (plan_->references.empty()) {
    return;
  }
  ImGui::SeparatorText(ICON_MD_EVENT_NOTE " References");
  for (size_t i = 0; i < plan_->references.size(); ++i) {
    const auto& r = plan_->references[i];
    ImGui::PushID(static_cast<int>(i));
    ImGui::BulletText("%s", r.label.c_str());
    if (!r.author.empty() || !r.date.empty()) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s %s", r.author.c_str(), r.date.c_str());
    }
    if (!r.path.empty()) {
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_CONTENT_COPY)) {
        ImGui::SetClipboardText(r.path.c_str());
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Copy path: %s", r.path.c_str());
      }
    }
    ImGui::PopID();
  }
}

void PlanningEditor::DrawRoute() {
  if (!DrawPlanHeader()) {
    return;
  }
  if (ImGui::BeginTable(
          "##route_layout", 2,
          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Steps", ImGuiTableColumnFlags_WidthStretch, 0.45f);
    ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthStretch,
                            0.55f);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawStepList();
    ImGui::TableNextColumn();
    DrawStepDetail();
    ImGui::EndTable();
  }
  ImGui::Separator();
  DrawItems();
}

void PlanningEditor::DrawStepList() {
  auto& steps = plan_->route_steps;
  ImGui::BeginChild("##step_list", ImVec2(0, 320));
  std::string last_group;
  int move_from = -1;
  int move_to = -1;
  for (size_t i = 0; i < steps.size(); ++i) {
    const auto& step = steps[i];
    if (!step.group.empty() && step.group != last_group) {
      ImGui::SeparatorText(step.group.c_str());
      last_group = step.group;
    }
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::Selectable(absl::StrFormat("%zu. %s  %s", i + 1, StepLabel(step),
                                          step.code.empty() ? "" : step.title)
                              .c_str(),
                          selected_step_ == static_cast<int>(i))) {
      selected_step_ = static_cast<int>(i);
    }
    if (ImGui::BeginDragDropSource()) {
      const int index = static_cast<int>(i);
      ImGui::SetDragDropPayload("PLAN_STEP", &index, sizeof(index));
      ImGui::TextUnformatted(StepLabel(step).c_str());
      ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload =
              ImGui::AcceptDragDropPayload("PLAN_STEP")) {
        move_from = *static_cast<const int*>(payload->Data);
        move_to = static_cast<int>(i);
      }
      ImGui::EndDragDropTarget();
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
  if (move_from >= 0 && move_to >= 0 && move_from != move_to) {
    auto moved = std::move(steps[move_from]);
    steps.erase(steps.begin() + move_from);
    steps.insert(steps.begin() + move_to, std::move(moved));
    selected_step_ = move_to;
    MarkDirty();
  }
  if (ImGui::SmallButton(ICON_MD_ADD " Step")) {
    core::PlanRouteStep step;
    step.id = plan_->NewStepId();
    step.title = "New step";
    step.kind = "story";
    const size_t at = selected_step_ >= 0 ? selected_step_ + 1 : steps.size();
    steps.insert(steps.begin() + at, std::move(step));
    selected_step_ = static_cast<int>(at);
    MarkDirty();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Drag steps to reorder");
}

void PlanningEditor::DrawStepDetail() {
  auto& steps = plan_->route_steps;
  if (selected_step_ < 0 || selected_step_ >= static_cast<int>(steps.size())) {
    ImGui::TextDisabled("Select a step");
    return;
  }
  auto& step = steps[selected_step_];
  ImGui::PushID("step_detail");
  std::string edited_id = step.id;
  if (ImGui::InputText("Id", &edited_id)) {
    const auto status = plan_->RenameStep(step.id, std::move(edited_id));
    if (status.ok()) {
      save_error_.clear();
      MarkDirty();
    } else {
      save_error_ = std::string(status.message());
    }
  }
  if (ImGui::InputText("Code", &step.code)) {
    MarkDirty();
  }
  if (ImGui::InputText("Title", &step.title)) {
    MarkDirty();
  }
  if (ImGui::InputText("Group", &step.group)) {
    MarkDirty();
  }
  if (StatusCombo("Kind", &step.kind, kStepKinds)) {
    MarkDirty();
  }
  std::string targets = JoinTargets(step.targets);
  if (ImGui::InputTextWithHint("Targets", "room:0x0C, map:0x4B, area:D1",
                               &targets)) {
    step.targets = SplitTargets(targets);
    MarkDirty();
  }
  DrawTargets(step.targets, "detail");
  if (ImGui::InputTextMultiline("##note", &step.note,
                                ImVec2(-1, ImGui::GetTextLineHeight() * 4))) {
    MarkDirty();
  }
  DrawAgentObservation("step:" + step.id);
  if (ImGui::SmallButton(ICON_MD_DELETE " Remove step")) {
    steps.erase(steps.begin() + selected_step_);
    selected_step_ =
        std::min<int>(selected_step_, static_cast<int>(steps.size()) - 1);
    MarkDirty();
  }
  ImGui::PopID();
}

void PlanningEditor::DrawItems() {
  auto& items = plan_->route_items;
  const std::string step_id =
      selected_step_ >= 0 &&
              selected_step_ < static_cast<int>(plan_->route_steps.size())
          ? plan_->route_steps[selected_step_].id
          : "";
  ImGui::Text("Items (%zu)", items.size());
  ImGui::SameLine();
  ImGui::Checkbox("Selected step only", &items_at_step_only_);
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_ADD " Item")) {
    core::PlanRouteItem item;
    item.id = plan_->NewItemId();
    item.name = "New item";
    item.step = step_id;
    item.status = "proposed";
    items.push_back(std::move(item));
    selected_item_ = static_cast<int>(items.size()) - 1;
    MarkDirty();
  }
  if (ImGui::BeginTable("##items", 5,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_BordersInnerH,
                        ImVec2(0, 220))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Step", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 90);
    ImGui::TableSetupColumn("Where", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (size_t i = 0; i < items.size(); ++i) {
      const auto& item = items[i];
      if (items_at_step_only_ && !step_id.empty() && item.step != step_id) {
        continue;
      }
      ImGui::PushID(static_cast<int>(i));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (ImGui::Selectable(item.name.c_str(),
                            selected_item_ == static_cast<int>(i),
                            ImGuiSelectableFlags_SpanAllColumns)) {
        selected_item_ = static_cast<int>(i);
      }
      ImGui::TableNextColumn();
      const int step_index = plan_->FindStep(item.step);
      ImGui::TextUnformatted(
          step_index >= 0 ? StepLabel(plan_->route_steps[step_index]).c_str()
                          : item.step.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(item.category.c_str());
      ImGui::TableNextColumn();
      gui::ColoredText(item.status.c_str(), StatusColor(item.status));
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(item.where.c_str());
      ImGui::PopID();
    }
    ImGui::EndTable();
  }

  if (selected_item_ < 0 || selected_item_ >= static_cast<int>(items.size())) {
    return;
  }
  auto& item = items[selected_item_];
  ImGui::SeparatorText(item.name.c_str());
  ImGui::PushID("item_detail");
  if (ImGui::InputText("Name", &item.name)) {
    MarkDirty();
  }
  if (ImGui::BeginCombo("Step", item.step.c_str())) {
    for (const auto& step : plan_->route_steps) {
      if (ImGui::Selectable(
              absl::StrFormat("%s  %s", StepLabel(step), step.title).c_str(),
              item.step == step.id)) {
        item.step = step.id;
        MarkDirty();
      }
    }
    ImGui::EndCombo();
  }
  if (ImGui::InputText("Category", &item.category)) {
    MarkDirty();
  }
  if (StatusCombo("Status", &item.status, kItemStatuses)) {
    MarkDirty();
  }
  if (ImGui::Checkbox("Required", &item.required)) {
    MarkDirty();
  }
  if (ImGui::InputText("Where", &item.where)) {
    MarkDirty();
  }
  if (ImGui::InputTextMultiline("##item_note", &item.note,
                                ImVec2(-1, ImGui::GetTextLineHeight() * 3))) {
    MarkDirty();
  }
  DrawAgentObservation("item:" + item.id);
  for (const auto& need : item.needs) {
    const int step_index = plan_->FindStep(need.step);
    ImGui::BulletText("Needed at %s: %s",
                      step_index >= 0
                          ? StepLabel(plan_->route_steps[step_index]).c_str()
                          : need.step.c_str(),
                      need.what.c_str());
  }
  if (ImGui::SmallButton(ICON_MD_DELETE " Remove item")) {
    items.erase(items.begin() + selected_item_);
    selected_item_ = -1;
    MarkDirty();
  }
  ImGui::PopID();
}

void PlanningEditor::DrawAgentStatus() {
  const core::PlanJson* section = core::FindPlanAgentStatus(*plan_);
  if (section == nullptr) {
    return;
  }
  ImGui::SeparatorText(ICON_MD_SMART_TOY " Agent status");
  const auto observations = section->find("observations");
  const size_t count =
      observations != section->end() && observations->is_object()
          ? observations->size()
          : 0;
  ImGui::TextDisabled("%zu observations, updated %s by %s", count,
                      JsonString(*section, "updated_utc", "-").c_str(),
                      JsonString(*section, "agent", "-").c_str());
  const auto orphans = core::OrphanedPlanAgentObservations(*plan_);
  if (!orphans.empty()) {
    gui::ColoredTextF(gui::GetWarningColor(),
                      "%zu observations name entries no longer in the plan: %s",
                      orphans.size(), absl::StrJoin(orphans, ", ").c_str());
  }
  if (count == 0 ||
      !ImGui::TreeNode("##agent_observations", "Observations (%zu)", count)) {
    return;
  }
  for (auto it = observations->begin(); it != observations->end(); ++it) {
    ImGui::BulletText("%s", it.key().c_str());
    const std::string state = JsonString(it.value(), "state", "");
    if (!state.empty()) {
      ImGui::SameLine();
      gui::ColoredText(state.c_str(), gui::GetInfoColor());
    }
  }
  ImGui::TreePop();
}

void PlanningEditor::DrawAgentObservation(const std::string& key) {
  const core::PlanJson* obs = core::FindPlanAgentObservation(*plan_, key);
  if (obs == nullptr) {
    return;
  }
  ImGui::PushID(key.c_str());
  ImGui::SeparatorText(ICON_MD_SMART_TOY " Agent status");
  const std::string state = JsonString(*obs, "state", "");
  if (!state.empty()) {
    gui::ColoredText(state.c_str(), gui::GetInfoColor());
  }
  for (const char* field : {"summary", "next"}) {
    const std::string text = JsonString(*obs, field, "");
    if (!text.empty()) {
      ImGui::TextWrapped("%s", text.c_str());
    }
  }
  if (auto evidence = obs->find("evidence");
      evidence != obs->end() && evidence->is_array()) {
    for (const auto& entry : *evidence) {
      if (entry.is_string()) {
        ImGui::BulletText("%s", entry.get<std::string>().c_str());
      }
    }
  }
  if (auto prs = obs->find("prs"); prs != obs->end() && prs->is_array()) {
    std::vector<std::string> labels;
    for (const auto& pr : *prs) {
      labels.push_back(pr.is_string() ? pr.get<std::string>()
                                      : "#" + pr.dump());
    }
    ImGui::TextDisabled("PRs: %s", absl::StrJoin(labels, ", ").c_str());
  }
  ImGui::TextDisabled(
      "%s %s - read-only; agents update this, your notes stay "
      "yours",
      JsonString(*obs, "agent", "").c_str(),
      JsonString(*obs, "observed_utc", "").c_str());
  ImGui::PopID();
}

void PlanningEditor::DrawTargets(const std::vector<std::string>& targets,
                                 const char* id) {
  if (targets.empty()) {
    return;
  }
  ImGui::PushID(id);
  for (size_t i = 0; i < targets.size(); ++i) {
    if (i > 0) {
      ImGui::SameLine();
    }
    ImGui::PushID(static_cast<int>(i));
    const core::PlanTarget target = core::PlanTarget::Parse(targets[i]);
    if (target.NumericId() >= 0) {
      if (ImGui::SmallButton(targets[i].c_str())) {
        OpenTarget(targets[i]);
      }
    } else {
      ImGui::TextDisabled("%s", targets[i].c_str());
    }
    ImGui::PopID();
  }
  ImGui::PopID();
}

void PlanningEditor::OpenTarget(const std::string& text) {
  const core::PlanTarget target = core::PlanTarget::Parse(text);
  const int id = target.NumericId();
  auto* bus = ContentRegistry::Context::event_bus();
  if (!bus || id < 0) {
    return;
  }
  if (target.kind == "room") {
    bus->Publish(JumpToRoomRequestEvent::Create(id, dependencies_.session_id));
  } else if (target.kind == "map") {
    bus->Publish(JumpToMapRequestEvent::Create(id, dependencies_.session_id));
  }
}

}  // namespace yaze::editor
