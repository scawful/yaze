#ifndef YAZE_APP_EDITOR_AGENT_AUTOMATION_BRIDGE_H_
#define YAZE_APP_EDITOR_AGENT_AUTOMATION_BRIDGE_H_

#if defined(YAZE_WITH_GRPC)

#include "absl/synchronization/mutex.h"
#include "app/editor/agent/agent_chat_widget.h"
#include "app/test/test_manager.h"

namespace yaze {
namespace editor {

class AutomationBridge : public test::HarnessListener {
 public:
  AutomationBridge() = default;
  ~AutomationBridge() override = default;

  void SetChatWidget(AgentChatWidget* widget) {
    absl::MutexLock lock(&mutex_);
    chat_widget_ = widget;
  }

  void OnHarnessTestUpdated(
      const test::HarnessTestExecution& execution) override;

  void OnHarnessPlanSummary(const std::string& summary) override;

 private:
  absl::Mutex mutex_;
  AgentChatWidget* chat_widget_ ABSL_GUARDED_BY(mutex_) = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // defined(YAZE_WITH_GRPC)

#endif  // YAZE_APP_EDITOR_AGENT_AUTOMATION_BRIDGE_H_
