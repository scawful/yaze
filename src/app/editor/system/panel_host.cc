#include "app/editor/system/panel_host.h"

#include "app/editor/system/panel_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

PanelDescriptor PanelHost::ToDescriptor(const PanelDefinition& definition) {
  PanelDescriptor descriptor;
  descriptor.card_id = definition.id;
  descriptor.display_name = definition.display_name;
  descriptor.icon = definition.icon;
  descriptor.category = definition.category;
  descriptor.window_title = definition.window_title;
  descriptor.shortcut_hint = definition.shortcut_hint;
  descriptor.priority = definition.priority;
  descriptor.scope = definition.scope;
  descriptor.panel_category = definition.panel_category;
  descriptor.context_scope = definition.context_scope;
  descriptor.on_show = definition.on_show;
  descriptor.on_hide = definition.on_hide;
  descriptor.visibility_flag = definition.visibility_flag;
  return descriptor;
}

bool PanelHost::RegisterPanel(size_t session_id,
                              const PanelDefinition& definition) {
  if (!panel_manager_ || definition.id.empty()) {
    return false;
  }

  for (const auto& legacy_id : definition.legacy_ids) {
    RegisterPanelAlias(legacy_id, definition.id);
  }

  panel_manager_->RegisterPanel(session_id, ToDescriptor(definition));
  if (definition.visible_by_default) {
    panel_manager_->ShowPanel(session_id, definition.id);
  }

  return true;
}

bool PanelHost::RegisterPanel(const PanelDefinition& definition) {
  if (!panel_manager_) {
    return false;
  }
  return RegisterPanel(panel_manager_->GetActiveSessionId(), definition);
}

bool PanelHost::RegisterPanels(
    size_t session_id, const std::vector<PanelDefinition>& definitions) {
  if (!panel_manager_) {
    return false;
  }

  bool any_registered = false;
  for (const auto& definition : definitions) {
    any_registered = RegisterPanel(session_id, definition) || any_registered;
  }
  return any_registered;
}

bool PanelHost::RegisterPanels(
    const std::vector<PanelDefinition>& definitions) {
  if (!panel_manager_) {
    return false;
  }
  return RegisterPanels(panel_manager_->GetActiveSessionId(), definitions);
}

void PanelHost::RegisterPanelAlias(const std::string& legacy_id,
                                   const std::string& canonical_id) {
  if (!panel_manager_) {
    return;
  }
  panel_manager_->RegisterPanelAlias(legacy_id, canonical_id);
}

bool PanelHost::ShowPanel(size_t session_id, const std::string& panel_id) {
  return panel_manager_ && panel_manager_->ShowPanel(session_id, panel_id);
}

bool PanelHost::HidePanel(size_t session_id, const std::string& panel_id) {
  return panel_manager_ && panel_manager_->HidePanel(session_id, panel_id);
}

bool PanelHost::TogglePanel(size_t session_id, const std::string& panel_id) {
  return panel_manager_ && panel_manager_->TogglePanel(session_id, panel_id);
}

bool PanelHost::IsPanelVisible(size_t session_id,
                               const std::string& panel_id) const {
  return panel_manager_ && panel_manager_->IsPanelVisible(session_id, panel_id);
}

bool PanelHost::OpenAndFocus(size_t session_id,
                             const std::string& panel_id) const {
  if (!panel_manager_) {
    return false;
  }

  if (!panel_manager_->ShowPanel(session_id, panel_id)) {
    return false;
  }

  const std::string window_name =
      panel_manager_->GetPanelWindowName(session_id, panel_id);
  if (window_name.empty()) {
    return false;
  }

  ImGui::SetWindowFocus(window_name.c_str());
  return true;
}

std::string PanelHost::ResolvePanelId(const std::string& panel_id) const {
  return panel_manager_ ? panel_manager_->ResolvePanelAlias(panel_id)
                        : panel_id;
}

std::string PanelHost::GetPanelWindowName(size_t session_id,
                                          const std::string& panel_id) const {
  return panel_manager_ ? panel_manager_->GetPanelWindowName(session_id, panel_id)
                        : std::string();
}

}  // namespace editor
}  // namespace yaze
