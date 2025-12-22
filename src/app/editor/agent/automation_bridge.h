#ifndef YAZE_APP_EDITOR_AGENT_AUTOMATION_BRIDGE_H_
#define YAZE_APP_EDITOR_AGENT_AUTOMATION_BRIDGE_H_

#if defined(YAZE_WITH_GRPC)

// Must define before any ImGui includes (test_manager.h includes ImGui headers)
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <string>

#include "absl/synchronization/mutex.h"
#include "app/editor/agent/agent_chat.h"
#include "app/test/test_manager.h"

namespace yaze {
namespace editor {

class AutomationBridge : public test::HarnessListener {
 public:
  AutomationBridge() = default;
  ~AutomationBridge() override = default;

  void SetAgentChat(AgentChat* chat) {
    absl::MutexLock lock(&mutex_);
    agent_chat_ = chat;
  }

  void OnHarnessTestUpdated(
      const test::HarnessTestExecution& execution) override;

  void OnHarnessPlanSummary(const std::string& summary) override;

 private:
  absl::Mutex mutex_;
  AgentChat* agent_chat_ ABSL_GUARDED_BY(mutex_) = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // defined(YAZE_WITH_GRPC)

#endif  // YAZE_APP_EDITOR_AGENT_AUTOMATION_BRIDGE_H_
