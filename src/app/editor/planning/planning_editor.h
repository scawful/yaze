#ifndef YAZE_APP_EDITOR_PLANNING_PLANNING_EDITOR_H_
#define YAZE_APP_EDITOR_PLANNING_PLANNING_EDITOR_H_

#include <optional>
#include <string>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "core/project_plan.h"

namespace yaze::editor {

// Project planning: goal, milestones, play route and open decisions, kept in
// the yaze.plan/1 file named by the project's [planning] section. Works with
// or without a ROM and never writes the ROM; Save() writes only the plan file.
class PlanningEditor : public Editor {
 public:
  PlanningEditor() { type_ = EditorType::kPlanning; }

  void Initialize() override;
  absl::Status Load() override { return absl::OkStatus(); }
  absl::Status Save() override;
  absl::Status Update() override;

  absl::Status Undo() override { return absl::OkStatus(); }
  absl::Status Redo() override { return absl::OkStatus(); }
  absl::Status Cut() override { return absl::OkStatus(); }
  absl::Status Copy() override { return absl::OkStatus(); }
  absl::Status Paste() override { return absl::OkStatus(); }
  absl::Status Find() override { return absl::OkStatus(); }
  bool CanUndo() const override { return false; }
  bool CanRedo() const override { return false; }
  bool CanCut() const override { return false; }
  bool CanCopy() const override { return false; }
  bool CanPaste() const override { return false; }

  void DrawOverview();
  void DrawRoute();

  bool dirty() const { return dirty_; }
  const std::optional<core::ProjectPlan>& plan() const { return plan_; }

 private:
  // Loads the plan when the project's [planning] path changes.
  void SyncWithProject();
  absl::Status Reload();
  bool DrawPlanHeader();
  void DrawNoPlan();
  void DrawGoal();
  void DrawMilestones();
  void DrawRouteStrip();
  void DrawDecisionQueue();
  void DrawReferences();
  void DrawStepList();
  void DrawStepDetail();
  void DrawItems();
  void DrawTargets(const std::vector<std::string>& targets, const char* id);
  void OpenTarget(const std::string& target);
  void MarkDirty() { dirty_ = true; }

  std::string plan_path_;
  std::optional<core::ProjectPlan> plan_;
  std::string load_error_;
  std::string save_error_;
  std::string new_plan_path_;
  bool dirty_ = false;
  bool panels_registered_ = false;
  int selected_step_ = -1;
  int selected_item_ = -1;
  bool items_at_step_only_ = true;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_PLANNING_PLANNING_EDITOR_H_
