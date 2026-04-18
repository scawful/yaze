#ifndef YAZE_APP_EDITOR_SYSTEM_PROJECT_WORKFLOW_STATUS_H_
#define YAZE_APP_EDITOR_SYSTEM_PROJECT_WORKFLOW_STATUS_H_

#include <chrono>
#include <string>
#include <vector>

namespace yaze::editor {

enum class ProjectWorkflowState {
  kIdle,
  kRunning,
  kSuccess,
  kFailure,
};

struct ProjectWorkflowStatus {
  bool visible = false;
  bool can_cancel = false;
  std::string label;
  std::string summary;
  std::string detail;
  std::string output_tail;
  ProjectWorkflowState state = ProjectWorkflowState::kIdle;
};

struct ProjectWorkflowHistoryEntry {
  std::string kind;
  ProjectWorkflowStatus status;
  std::string output_log;
  std::chrono::system_clock::time_point timestamp;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_PROJECT_WORKFLOW_STATUS_H_
