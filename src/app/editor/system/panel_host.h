#ifndef YAZE_APP_EDITOR_SYSTEM_PANEL_HOST_H_
#define YAZE_APP_EDITOR_SYSTEM_PANEL_HOST_H_

#include <functional>
#include <string>
#include <vector>

#include "app/editor/system/editor_panel.h"

namespace yaze {
namespace editor {

class PanelManager;
struct PanelDescriptor;

/**
 * @brief Declarative registration contract for editor panels.
 *
 * This decouples panel metadata from individual editor implementations and
 * keeps visibility/layout concerns in a single host-facing API.
 */
struct PanelDefinition {
  std::string id;
  std::string display_name;
  std::string icon;
  std::string category;
  std::string window_title;
  std::string shortcut_hint;
  int priority = 50;
  bool visible_by_default = false;
  bool* visibility_flag = nullptr;
  PanelScope scope = PanelScope::kSession;
  PanelCategory panel_category = PanelCategory::EditorBound;
  PanelContextScope context_scope = PanelContextScope::kNone;
  std::vector<std::string> legacy_ids;
  std::function<void()> on_show;
  std::function<void()> on_hide;
};

/**
 * @brief Thin host API over PanelManager for declarative panel workflows.
 */
class PanelHost {
 public:
  explicit PanelHost(PanelManager* panel_manager = nullptr)
      : panel_manager_(panel_manager) {}

  void SetPanelManager(PanelManager* panel_manager) {
    panel_manager_ = panel_manager;
  }
  PanelManager* panel_manager() const { return panel_manager_; }

  bool RegisterPanel(size_t session_id, const PanelDefinition& definition);
  bool RegisterPanel(const PanelDefinition& definition);
  bool RegisterPanels(size_t session_id,
                      const std::vector<PanelDefinition>& definitions);
  bool RegisterPanels(const std::vector<PanelDefinition>& definitions);

  void RegisterPanelAlias(const std::string& legacy_id,
                          const std::string& canonical_id);

  bool ShowPanel(size_t session_id, const std::string& panel_id);
  bool HidePanel(size_t session_id, const std::string& panel_id);
  bool TogglePanel(size_t session_id, const std::string& panel_id);
  bool IsPanelVisible(size_t session_id, const std::string& panel_id) const;

  bool OpenAndFocus(size_t session_id, const std::string& panel_id) const;

  std::string ResolvePanelId(const std::string& panel_id) const;
  std::string GetPanelWindowName(size_t session_id,
                                 const std::string& panel_id) const;

 private:
  static PanelDescriptor ToDescriptor(const PanelDefinition& definition);

  PanelManager* panel_manager_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_PANEL_HOST_H_
