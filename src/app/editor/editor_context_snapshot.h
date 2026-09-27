#ifndef YAZE_APP_EDITOR_EDITOR_CONTEXT_SNAPSHOT_H_
#define YAZE_APP_EDITOR_EDITOR_CONTEXT_SNAPSHOT_H_

#include <algorithm>
#include <string>
#include <vector>

namespace yaze::editor {

enum class EditorContextDiagnosticSeverity {
  kInfo,
  kWarning,
  kError,
};

enum class EditorContextActionKind {
  kOpenWindow,
  kCommand,
};

struct EditorContextValue {
  std::string id;
  std::string label;
  std::string value;
};

struct EditorContextDiagnostic {
  std::string id;
  EditorContextDiagnosticSeverity severity =
      EditorContextDiagnosticSeverity::kInfo;
  std::string message;
  std::string action_id;
};

struct EditorContextAction {
  std::string id;
  std::string label;
  EditorContextActionKind kind = EditorContextActionKind::kOpenWindow;
  std::string target;
  bool enabled = true;
  std::string disabled_reason;
};

struct EditorExperimentContext {
  bool experimental = false;
  bool acknowledged = true;
  std::string save_posture;
};

/**
 * Pure semantic snapshot shared by shell surfaces and automation.
 *
 * Editors own the meaning of the values. Consumers own presentation and
 * dispatch stable action targets; no ImGui state or callbacks live here.
 */
struct EditorContextSnapshot {
  std::string category;
  std::string title;
  std::string subtitle;
  std::string semantic_owner;
  std::vector<EditorContextValue> metadata;
  std::vector<EditorContextValue> counts;
  std::vector<EditorContextDiagnostic> diagnostics;
  std::vector<EditorContextAction> actions;
  std::vector<std::string> capabilities;
  EditorExperimentContext experiment;
  bool has_pending_changes = false;
  std::string pending_label;

  bool empty() const {
    return title.empty() && subtitle.empty() && metadata.empty() &&
           counts.empty() && diagnostics.empty() && actions.empty();
  }

  bool HasCapability(const std::string& capability) const {
    return std::find(capabilities.begin(), capabilities.end(), capability) !=
           capabilities.end();
  }
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_EDITOR_CONTEXT_SNAPSHOT_H_
