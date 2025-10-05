#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/agent/automation_bridge.h"

#if defined(YAZE_WITH_GRPC)

#include "absl/time/time.h"
#include "app/editor/agent/agent_chat_widget.h"

// test_manager.h already included in automation_bridge.h

namespace yaze {
namespace editor {

void AutomationBridge::OnHarnessTestUpdated(
    const test::HarnessTestExecution& execution) {
  absl::MutexLock lock(&mutex_);
  if (!chat_widget_) {
    return;
  }

  AgentChatWidget::AutomationTelemetry telemetry;
  telemetry.test_id = execution.test_id;
  telemetry.name = execution.name;
  telemetry.status = test::HarnessStatusToString(execution.status);
  telemetry.message = execution.error_message;
  telemetry.updated_at = (execution.completed_at == absl::InfiniteFuture() ||
                          execution.completed_at == absl::InfinitePast())
                            ? absl::Now()
                            : execution.completed_at;

  chat_widget_->UpdateHarnessTelemetry(telemetry);
}

void AutomationBridge::OnHarnessPlanSummary(const std::string& summary) {
  absl::MutexLock lock(&mutex_);
  if (!chat_widget_) {
    return;
  }
  chat_widget_->SetLastPlanSummary(summary);
}

}  // namespace editor
}  // namespace yaze

#endif  // defined(YAZE_WITH_GRPC)
