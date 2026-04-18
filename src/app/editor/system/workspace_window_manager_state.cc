#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/system/workspace_window_manager.h"

#include <algorithm>

#include "imgui/imgui.h"
#include "util/log.h"

namespace yaze {
namespace editor {

namespace {

WindowDescriptor BuildDescriptorFromPanel(const WindowContent& panel) {
  WindowDescriptor descriptor;
  auto* panel_ptr = const_cast<WindowContent*>(&panel);
  descriptor.card_id = panel.GetId();
  descriptor.display_name = panel.GetDisplayName();
  descriptor.icon = panel.GetIcon();
  descriptor.category = panel.GetEditorCategory();
  descriptor.priority = panel.GetPriority();
  descriptor.workflow_group = panel.GetWorkflowGroup();
  descriptor.workflow_label = panel.GetWorkflowLabel();
  descriptor.workflow_description = panel.GetWorkflowDescription();
  descriptor.workflow_priority = panel.GetWorkflowPriority();
  descriptor.shortcut_hint = panel.GetShortcutHint();
  descriptor.scope = panel.GetScope();
  descriptor.window_lifecycle = panel.GetWindowLifecycle();
  descriptor.context_scope = panel.GetContextScope();
  descriptor.enabled_condition = [panel_ptr]() {
    return panel_ptr->IsEnabled();
  };
  descriptor.disabled_tooltip = panel.GetDisabledTooltip();
  descriptor.visibility_flag = nullptr;
  descriptor.window_title = panel.GetIcon() + " " + panel.GetDisplayName();
  return descriptor;
}

}  // namespace

void WorkspaceWindowManager::RegisterSession(size_t session_id) {
  if (!FindSessionWindowIds(session_id)) {
    session_state_.session_windows[session_id] = std::vector<std::string>();
    session_state_.session_window_mapping[session_id] =
        std::unordered_map<std::string, std::string>();
    session_state_.session_reverse_window_mapping[session_id] =
        std::unordered_map<std::string, std::string>();
    for (const auto& global_panel_id : global_panel_ids_) {
      TrackPanelForSession(session_id, global_panel_id, global_panel_id);
    }
    UpdateSessionCount();
    LOG_INFO("WorkspaceWindowManager", "Registered session %zu (total: %zu)",
             session_id, session_count_);
  }
}

void WorkspaceWindowManager::UnregisterSession(size_t session_id) {
  if (FindSessionWindowIds(session_id)) {
    UnregisterSessionPanels(session_id);
    session_state_.session_windows.erase(session_id);
    session_state_.session_window_mapping.erase(session_id);
    session_state_.session_reverse_window_mapping.erase(session_id);
    session_state_.session_context_keys.erase(session_id);
    UpdateSessionCount();

    if (active_session_ == session_id) {
      active_session_ = 0;
      if (!session_state_.session_windows.empty()) {
        active_session_ = session_state_.session_windows.begin()->first;
      }
    }

    LOG_INFO("WorkspaceWindowManager", "Unregistered session %zu (total: %zu)",
             session_id, session_count_);
  }
}

void WorkspaceWindowManager::SetActiveSession(size_t session_id) {
  active_session_ = session_id;
}

void WorkspaceWindowManager::SetContextKey(size_t session_id,
                                           WindowContextScope scope,
                                           std::string key) {
  RegisterSession(session_id);
  auto& session_map = session_state_.session_context_keys[session_id];
  const std::string old_key =
      (session_map.find(scope) != session_map.end()) ? session_map[scope] : "";
  if (old_key == key) {
    return;
  }
  session_map[scope] = std::move(key);
  ApplyContextPolicy(session_id, scope, old_key, session_map[scope]);
}

std::string WorkspaceWindowManager::GetContextKey(
    size_t session_id, WindowContextScope scope) const {
  auto sit = session_state_.session_context_keys.find(session_id);
  if (sit == session_state_.session_context_keys.end()) {
    return "";
  }
  const auto& session_map = sit->second;
  auto it = session_map.find(scope);
  if (it == session_map.end()) {
    return "";
  }
  return it->second;
}

void WorkspaceWindowManager::RegisterPanelAlias(
    const std::string& legacy_base_id, const std::string& canonical_base_id) {
  if (legacy_base_id.empty() || canonical_base_id.empty() ||
      legacy_base_id == canonical_base_id) {
    return;
  }
  panel_id_aliases_[legacy_base_id] = canonical_base_id;
}

std::string WorkspaceWindowManager::ResolvePanelAlias(
    const std::string& panel_id) const {
  return ResolveBaseWindowId(panel_id);
}

std::string WorkspaceWindowManager::ResolveBaseWindowId(
    const std::string& panel_id) const {
  if (panel_id.empty()) {
    return "";
  }

  std::string resolved = panel_id;
  std::unordered_set<std::string> visited;
  visited.insert(resolved);

  for (int depth = 0; depth < 16; ++depth) {
    auto alias_it = panel_id_aliases_.find(resolved);
    if (alias_it == panel_id_aliases_.end() || alias_it->second.empty()) {
      return resolved;
    }

    const std::string& next = alias_it->second;
    if (next == resolved || visited.count(next) > 0) {
      return resolved;
    }

    resolved = next;
    visited.insert(resolved);
  }

  return resolved;
}

void WorkspaceWindowManager::ApplyContextPolicy(size_t session_id,
                                                WindowContextScope scope,
                                                const std::string& old_key,
                                                const std::string& new_key) {
  (void)old_key;
  if (!new_key.empty()) {
    return;
  }

  const auto* session_windows = FindSessionWindowIds(session_id);
  if (!session_windows) {
    return;
  }

  for (const auto& prefixed_id : *session_windows) {
    const auto* desc = FindDescriptorByPrefixedId(prefixed_id);
    if (!desc) {
      continue;
    }
    if (desc->context_scope != scope) {
      continue;
    }
    if (!desc->visibility_flag || !*desc->visibility_flag) {
      continue;
    }
    if (IsWindowPinnedImpl(prefixed_id)) {
      continue;
    }

    const std::string base_id = GetBaseIdForPrefixedId(session_id, prefixed_id);
    if (!base_id.empty()) {
      (void)CloseWindowImpl(session_id, base_id);
    }
  }
}

std::string WorkspaceWindowManager::GetBaseIdForPrefixedId(
    size_t session_id, const std::string& prefixed_id) const {
  const auto* reverse = FindSessionReverseWindowMapping(session_id);
  if (!reverse) {
    return "";
  }
  auto it = reverse->find(prefixed_id);
  if (it == reverse->end()) {
    return "";
  }
  return it->second;
}

void WorkspaceWindowManager::SetVisibleWindowsImpl(
    size_t session_id, const std::vector<std::string>& panel_ids) {
  const auto* window_mapping = FindSessionWindowMapping(session_id);
  if (!window_mapping) {
    return;
  }

  std::unordered_set<std::string> visible_set;
  visible_set.reserve(panel_ids.size());
  for (const auto& panel_id : panel_ids) {
    visible_set.insert(ResolveBaseWindowId(panel_id));
  }

  for (const auto& [base_id, prefixed_id] : *window_mapping) {
    if (auto* descriptor = FindDescriptorByPrefixedId(prefixed_id)) {
      if (!descriptor->visibility_flag) {
        continue;
      }
      *descriptor->visibility_flag = visible_set.count(base_id) > 0;
    }
  }

  LOG_INFO("WorkspaceWindowManager", "Set %zu panels visible for session %zu",
           panel_ids.size(), session_id);
}

std::unordered_map<std::string, bool>
WorkspaceWindowManager::SerializeVisibilityState(size_t session_id) const {
  std::unordered_map<std::string, bool> state;

  const auto* session_mapping = FindSessionWindowMapping(session_id);
  if (!session_mapping) {
    return state;
  }

  for (const auto& [base_id, prefixed_id] : *session_mapping) {
    if (const auto* descriptor = FindDescriptorByPrefixedId(prefixed_id)) {
      if (descriptor->visibility_flag) {
        state[base_id] = *descriptor->visibility_flag;
      }
    }
  }

  return state;
}

void WorkspaceWindowManager::RestoreVisibilityState(
    size_t session_id, const std::unordered_map<std::string, bool>& state,
    bool publish_events) {
  const auto* session_mapping = FindSessionWindowMapping(session_id);
  if (!session_mapping) {
    LOG_WARN("WorkspaceWindowManager",
             "Cannot restore visibility: session %zu not found", session_id);
    return;
  }

  size_t restored = 0;
  for (const auto& [base_id, visible] : state) {
    const std::string canonical_base_id = ResolveBaseWindowId(base_id);
    auto mapping_it = session_mapping->find(canonical_base_id);
    if (mapping_it == session_mapping->end()) {
      continue;
    }
    if (auto* descriptor = FindDescriptorByPrefixedId(mapping_it->second)) {
      if (!descriptor->visibility_flag) {
        continue;
      }
      *descriptor->visibility_flag = visible;
      if (publish_events) {
        PublishWindowVisibilityChanged(session_id, mapping_it->second,
                                       canonical_base_id, descriptor->category,
                                       visible);
      }
      restored++;
    }
  }

  LOG_INFO("WorkspaceWindowManager",
           "Restored visibility for %zu/%zu panels in session %zu", restored,
           state.size(), session_id);
}

std::unordered_map<std::string, bool>
WorkspaceWindowManager::SerializePinnedState() const {
  std::unordered_map<std::string, bool> state;

  for (const auto& [prefixed_id, pinned] : pinned_panels_) {
    std::string base_id = prefixed_id;
    if (prefixed_id.size() > 2 && prefixed_id[0] == 's') {
      size_t dot_pos = prefixed_id.find('.');
      if (dot_pos != std::string::npos && dot_pos + 1 < prefixed_id.size()) {
        base_id = prefixed_id.substr(dot_pos + 1);
      }
    }
    state[base_id] = pinned;
  }

  // Include pins that were restored but never yet matched a live panel — e.g.
  // the user pinned the emulator panel in a prior session and has not re-opened
  // that editor yet this run. Without this, a save would drop those intentions.
  for (const auto& [base_id, pinned] : pending_pinned_base_ids_) {
    state.try_emplace(base_id, pinned);
  }

  return state;
}

void WorkspaceWindowManager::RestorePinnedState(
    const std::unordered_map<std::string, bool>& state) {
  std::unordered_map<std::string, bool> canonical_state;
  canonical_state.reserve(state.size());
  for (const auto& [base_id, pinned] : state) {
    canonical_state[ResolveBaseWindowId(base_id)] = pinned;
  }

  // Seed pending with every restored entry. TrackPanelForSession consumes
  // them as matching panels register; leftover entries persist so a repeat
  // Save round-trips them without asking the panel to actually register.
  pending_pinned_base_ids_ = canonical_state;

  for (const auto& [session_id, card_mapping] : session_card_mapping_) {
    for (const auto& [base_id, prefixed_id] : card_mapping) {
      auto state_it = canonical_state.find(base_id);
      if (state_it != canonical_state.end()) {
        pinned_panels_[prefixed_id] = state_it->second;
        pending_pinned_base_ids_.erase(base_id);
      }
    }
  }

  LOG_INFO("WorkspaceWindowManager",
           "Restored pinned state for %zu panels (%zu still pending "
           "registration)",
           canonical_state.size() - pending_pinned_base_ids_.size(),
           pending_pinned_base_ids_.size());
}

std::string WorkspaceWindowManager::GetWindowNameImpl(
    size_t session_id, const std::string& base_card_id) const {
  const WindowDescriptor* descriptor =
      GetWindowDescriptorImpl(session_id, base_card_id);
  if (!descriptor) {
    return "";
  }
  return GetWindowNameImpl(*descriptor);
}

std::string WorkspaceWindowManager::GetWindowNameImpl(
    const WindowDescriptor& descriptor) const {
  return descriptor.GetImGuiWindowName();
}

void WorkspaceWindowManager::HandleSidebarKeyboardNav(
    size_t session_id, const std::vector<WindowDescriptor>& cards) {
  if (!browser_state_.sidebar_has_focus &&
      ImGui::IsWindowHovered(ImGuiHoveredFlags_None) &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    browser_state_.sidebar_has_focus = true;
    browser_state_.focused_window_index = cards.empty() ? -1 : 0;
  }

  if (!browser_state_.sidebar_has_focus || cards.empty()) {
    return;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    browser_state_.sidebar_has_focus = false;
    browser_state_.focused_window_index = -1;
    return;
  }

  int card_count = static_cast<int>(cards.size());
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ||
      ImGui::IsKeyPressed(ImGuiKey_J)) {
    browser_state_.focused_window_index =
        std::min(browser_state_.focused_window_index + 1, card_count - 1);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) ||
      ImGui::IsKeyPressed(ImGuiKey_K)) {
    browser_state_.focused_window_index =
        std::max(browser_state_.focused_window_index - 1, 0);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
    browser_state_.focused_window_index = 0;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_End)) {
    browser_state_.focused_window_index = card_count - 1;
  }

  if (browser_state_.focused_window_index >= 0 &&
      browser_state_.focused_window_index < card_count) {
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
        ImGui::IsKeyPressed(ImGuiKey_Space)) {
      const auto& card = cards[browser_state_.focused_window_index];
      ToggleWindowImpl(session_id, card.card_id);
    }
  }
}

void WorkspaceWindowManager::MarkWindowRecentlyUsedImpl(
    const std::string& card_id) {
  last_used_at_[card_id] = ++mru_counter_;
}

std::vector<WindowDescriptor> WorkspaceWindowManager::GetWindowsSortedByMRUImpl(
    size_t session_id, const std::string& category) const {
  auto panels = GetWindowsInCategoryImpl(session_id, category);

  std::sort(
      panels.begin(), panels.end(),
      [this, session_id](const WindowDescriptor& a, const WindowDescriptor& b) {
        bool a_pinned = IsWindowPinnedImpl(session_id, a.card_id);
        bool b_pinned = IsWindowPinnedImpl(session_id, b.card_id);
        if (a_pinned != b_pinned) {
          return a_pinned > b_pinned;
        }

        auto a_it = last_used_at_.find(a.card_id);
        auto b_it = last_used_at_.find(b.card_id);
        uint64_t a_time = (a_it != last_used_at_.end()) ? a_it->second : 0;
        uint64_t b_time = (b_it != last_used_at_.end()) ? b_it->second : 0;
        if (a_time != b_time) {
          return a_time > b_time;
        }

        return a.priority < b.priority;
      });

  return panels;
}

size_t WorkspaceWindowManager::GetVisibleWindowCount(size_t session_id) const {
  size_t count = 0;
  const auto* session_windows = FindSessionWindowIds(session_id);
  if (!session_windows) {
    return count;
  }
  for (const auto& prefixed_card_id : *session_windows) {
    if (const auto* descriptor = FindDescriptorByPrefixedId(prefixed_card_id)) {
      if (descriptor->visibility_flag && *descriptor->visibility_flag) {
        count++;
      }
    }
  }
  return count;
}

void WorkspaceWindowManager::UpdateSessionCount() {
  session_count_ = session_cards_.size();
}

std::string WorkspaceWindowManager::GetPrefixedWindowId(
    size_t session_id, const std::string& base_id) const {
  const std::string resolved_base_id = ResolveBaseWindowId(base_id);

  const auto* session_mapping = FindSessionWindowMapping(session_id);
  if (session_mapping) {
    auto card_it = session_mapping->find(resolved_base_id);
    if (card_it != session_mapping->end()) {
      return card_it->second;
    }
  }

  if (cards_.find(resolved_base_id) != cards_.end()) {
    return resolved_base_id;
  }

  return "";
}

void WorkspaceWindowManager::RegisterPanelDescriptorForSession(
    size_t session_id, const WindowContent& panel) {
  RegisterSession(session_id);
  std::string panel_id =
      MakeWindowId(session_id, panel.GetId(), panel.GetScope());
  bool already_registered = (cards_.find(panel_id) != cards_.end());
  WindowDescriptor descriptor = BuildDescriptorFromPanel(panel);
  RegisterPanel(session_id, descriptor);
  if (!already_registered && panel.IsVisibleByDefault()) {
    OpenWindowImpl(session_id, panel.GetId());
  }
}

void WorkspaceWindowManager::TrackPanelForSession(size_t session_id,
                                                  const std::string& base_id,
                                                  const std::string& panel_id) {
  const std::string canonical_base_id = ResolveBaseWindowId(base_id);

  auto& card_list = session_cards_[session_id];
  if (std::find(card_list.begin(), card_list.end(), panel_id) ==
      card_list.end()) {
    card_list.push_back(panel_id);
  }
  session_card_mapping_[session_id][canonical_base_id] = panel_id;
  session_reverse_card_mapping_[session_id][panel_id] = canonical_base_id;

  // If RestorePinnedState previously carried a pin for this base id that
  // couldn't be applied yet (panel hadn't registered), apply it now. User's
  // live pinned_panels_ wins on unregister/re-register — pending is consumed
  // exactly once.
  auto pending_it = pending_pinned_base_ids_.find(canonical_base_id);
  if (pending_it != pending_pinned_base_ids_.end()) {
    pinned_panels_[panel_id] = pending_it->second;
    pending_pinned_base_ids_.erase(pending_it);
  }
}

void WorkspaceWindowManager::UnregisterSessionPanels(size_t session_id) {
  const auto* session_windows = FindSessionWindowIds(session_id);
  if (!session_windows) {
    return;
  }

  for (const auto& prefixed_card_id : *session_windows) {
    if (global_panel_ids_.find(prefixed_card_id) != global_panel_ids_.end()) {
      continue;
    }
    cards_.erase(prefixed_card_id);
    centralized_visibility_.erase(prefixed_card_id);
    pinned_panels_.erase(prefixed_card_id);
  }
}

}  // namespace editor
}  // namespace yaze
