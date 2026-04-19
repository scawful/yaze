#ifndef YAZE_APP_EDITOR_SYSTEM_PANEL_HOST_H_
#define YAZE_APP_EDITOR_SYSTEM_PANEL_HOST_H_

#include <functional>
#include <string>
#include <vector>

#include "app/editor/system/workspace/editor_panel.h"

namespace yaze {
namespace editor {

class WorkspaceWindowManager;
struct WindowDescriptor;

/**
 * @brief Declarative registration contract for editor windows.
 *
 * This decouples window metadata from individual editor implementations and
 * keeps visibility/layout concerns in a single host-facing API.
 */
struct WindowDefinition {
  std::string id;
  std::string display_name;
  std::string icon;
  std::string category;
  std::string window_title;
  std::string shortcut_hint;
  int priority = 50;
  bool visible_by_default = false;
  bool* visibility_flag = nullptr;
  WindowScope scope = WindowScope::kSession;
  WindowLifecycle window_lifecycle = WindowLifecycle::EditorBound;
  WindowContextScope context_scope = WindowContextScope::kNone;
  std::vector<std::string> legacy_ids;
  std::function<void()> on_show;
  std::function<void()> on_hide;
};

/**
 * @brief Thin host API over WorkspaceWindowManager for declarative window workflows.
 */
class WindowHost {
 public:
  explicit WindowHost(WorkspaceWindowManager* window_manager = nullptr)
      : window_manager_(window_manager) {}

  void SetWindowManager(WorkspaceWindowManager* window_manager) {
    window_manager_ = window_manager;
  }
  WorkspaceWindowManager* window_manager() const { return window_manager_; }

  bool RegisterPanel(size_t session_id, const WindowDefinition& definition);
  bool RegisterPanel(const WindowDefinition& definition);
  bool RegisterPanels(size_t session_id,
                      const std::vector<WindowDefinition>& definitions);
  bool RegisterPanels(const std::vector<WindowDefinition>& definitions);

  bool RegisterWindow(size_t session_id, const WindowDefinition& definition) {
    return RegisterPanel(session_id, definition);
  }
  bool RegisterWindow(const WindowDefinition& definition) {
    return RegisterPanel(definition);
  }
  bool RegisterWindows(size_t session_id,
                       const std::vector<WindowDefinition>& definitions) {
    return RegisterPanels(session_id, definitions);
  }
  bool RegisterWindows(const std::vector<WindowDefinition>& definitions) {
    return RegisterPanels(definitions);
  }

  void RegisterPanelAlias(const std::string& legacy_id,
                          const std::string& canonical_id);
  void RegisterWindowAlias(const std::string& legacy_id,
                           const std::string& canonical_id) {
    RegisterPanelAlias(legacy_id, canonical_id);
  }

  bool OpenWindow(size_t session_id, const std::string& window_id);
  bool CloseWindow(size_t session_id, const std::string& window_id);
  bool ToggleWindow(size_t session_id, const std::string& window_id);
  bool IsWindowOpen(size_t session_id, const std::string& window_id) const;

  bool OpenAndFocusWindow(size_t session_id,
                          const std::string& window_id) const;

  std::string ResolveWindowId(const std::string& window_id) const;
  std::string GetWorkspaceWindowName(size_t session_id,
                                     const std::string& window_id) const;

 private:
  static WindowDescriptor ToDescriptor(const WindowDefinition& definition);

  WorkspaceWindowManager* window_manager_ = nullptr;
};

using PanelDefinition = WindowDefinition;
using PanelHost = WindowHost;

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_PANEL_HOST_H_
