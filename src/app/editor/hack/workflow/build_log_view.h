#ifndef YAZE_APP_EDITOR_HACK_WORKFLOW_BUILD_LOG_VIEW_H_
#define YAZE_APP_EDITOR_HACK_WORKFLOW_BUILD_LOG_VIEW_H_

#include <optional>
#include <string>

#include "app/editor/system/session/build_log_view_model.h"

namespace yaze::editor::workflow {

// Structured build output: outcome counts, build receipt checks and
// collapsible steps, with problems-only, raw-text and copy controls.
class BuildLogView {
 public:
  // Re-parses only when the text differs from the previous call.
  void SetLog(const std::string& log);

  bool empty() const { return log_.empty(); }
  const BuildLogSummary& summary() const { return summary_; }
  const std::optional<BuildReceiptSummary>& receipt() const { return receipt_; }

  // `follow_tail` keeps the view pinned to the newest output while running.
  void Draw(const char* id, float height, bool follow_tail);

 private:
  void DrawToolbar();
  void DrawReceiptChecks();
  void DrawSteps();

  std::string log_;
  BuildLogSummary summary_;
  std::optional<BuildReceiptSummary> receipt_;
  int receipt_passed_ = 0;
  int receipt_failed_ = 0;
  bool problems_only_ = false;
  bool show_raw_ = false;
};

}  // namespace yaze::editor::workflow

#endif  // YAZE_APP_EDITOR_HACK_WORKFLOW_BUILD_LOG_VIEW_H_
