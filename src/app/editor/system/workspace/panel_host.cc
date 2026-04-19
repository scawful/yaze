#include "app/editor/system/workspace/panel_host.h"

#include "app/editor/system/workspace/workspace_window_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

WindowDescriptor WindowHost::ToDescriptor(const WindowDefinition& definition) {
  WindowDescriptor descriptor;
  descriptor.card_id = definition.id;
  descriptor.display_name = definition.display_name;
  descriptor.icon = definition.icon;
  descriptor.category = definition.category;
  descriptor.window_title = definition.window_title;
  descriptor.shortcut_hint = definition.shortcut_hint;
  descriptor.priority = definition.priority;
  descriptor.scope = definition.scope;
  descriptor.window_lifecycle = definition.window_lifecycle;
  descriptor.context_scope = definition.context_scope;
  descriptor.on_show = definition.on_show;
  descriptor.on_hide = definition.on_hide;
  descriptor.visibility_flag = definition.visibility_flag;
  return descriptor;
}

bool WindowHost::RegisterPanel(size_t session_id,
                               const WindowDefinition& definition) {
  if (!window_manager_ || definition.id.empty()) {
    return false;
  }

  for (const auto& legacy_id : definition.legacy_ids) {
    RegisterPanelAlias(legacy_id, definition.id);
  }

  window_manager_->RegisterWindow(session_id, ToDescriptor(definition));
  if (definition.visible_by_default) {
    window_manager_->OpenWindow(session_id, definition.id);
  }

  return true;
}

bool WindowHost::RegisterPanel(const WindowDefinition& definition) {
  if (!window_manager_) {
    return false;
  }
  return RegisterPanel(window_manager_->GetActiveSessionId(), definition);
}

bool WindowHost::RegisterPanels(
    size_t session_id, const std::vector<WindowDefinition>& definitions) {
  if (!window_manager_) {
    return false;
  }

  bool any_registered = false;
  for (const auto& definition : definitions) {
    any_registered = RegisterPanel(session_id, definition) || any_registered;
  }
  return any_registered;
}

bool WindowHost::RegisterPanels(
    const std::vector<WindowDefinition>& definitions) {
  if (!window_manager_) {
    return false;
  }
  return RegisterPanels(window_manager_->GetActiveSessionId(), definitions);
}

void WindowHost::RegisterPanelAlias(const std::string& legacy_id,
                                    const std::string& canonical_id) {
  if (!window_manager_) {
    return;
  }
  window_manager_->RegisterPanelAlias(legacy_id, canonical_id);
}

bool WindowHost::OpenWindow(size_t session_id, const std::string& window_id) {
  return window_manager_ && window_manager_->OpenWindow(session_id, window_id);
}

bool WindowHost::CloseWindow(size_t session_id, const std::string& window_id) {
  return window_manager_ && window_manager_->CloseWindow(session_id, window_id);
}

bool WindowHost::ToggleWindow(size_t session_id, const std::string& window_id) {
  return window_manager_ &&
         window_manager_->ToggleWindow(session_id, window_id);
}

bool WindowHost::IsWindowOpen(size_t session_id,
                              const std::string& window_id) const {
  return window_manager_ &&
         window_manager_->IsWindowOpen(session_id, window_id);
}

bool WindowHost::OpenAndFocusWindow(size_t session_id,
                                    const std::string& window_id) const {
  if (!window_manager_) {
    return false;
  }

  if (!window_manager_->OpenWindow(session_id, window_id)) {
    return false;
  }

  const std::string window_name =
      window_manager_->GetWorkspaceWindowName(session_id, window_id);
  if (window_name.empty()) {
    return false;
  }

  ImGui::SetWindowFocus(window_name.c_str());
  return true;
}

std::string WindowHost::ResolveWindowId(const std::string& window_id) const {
  return window_manager_ ? window_manager_->ResolveWindowAlias(window_id)
                         : window_id;
}

std::string WindowHost::GetWorkspaceWindowName(
    size_t session_id, const std::string& window_id) const {
  return window_manager_
             ? window_manager_->GetWorkspaceWindowName(session_id, window_id)
             : std::string();
}

}  // namespace editor
}  // namespace yaze
