#include "session_coordinator.h"

#include <algorithm>
#include <cstdio>

#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

SessionCoordinator::SessionCoordinator(void* sessions_ptr,
                                       SessionCardRegistry* card_registry,
                                       ToastManager* toast_manager)
    : sessions_ptr_(sessions_ptr),
      card_registry_(card_registry),
      toast_manager_(toast_manager) {
  auto* sessions = static_cast<std::deque<EditorManager::RomSession>*>(sessions_ptr_);
  if (sessions && !sessions->empty()) {
    active_session_index_ = 0;
    session_count_ = sessions->size();
  }
}

// Helper macro to get sessions pointer
#define GET_SESSIONS() static_cast<std::deque<EditorManager::RomSession>*>(sessions_ptr_)

void SessionCoordinator::CreateNewSession() {
  auto* sessions = GET_SESSIONS();
  if (!sessions) return;
  
  if (session_count_ >= kMaxSessions) {
    ShowSessionLimitWarning();
    return;
  }
  
  // Create new empty session
  sessions->emplace_back();
  UpdateSessionCount();
  
  // Set as active session
  active_session_index_ = sessions->size() - 1;
  
  printf("[SessionCoordinator] Created new session %zu (total: %zu)\n", 
         active_session_index_, session_count_);
  
  ShowSessionOperationResult("Create Session", true);
}

void SessionCoordinator::DuplicateCurrentSession() {
  auto* sessions = GET_SESSIONS();
  if (!sessions || sessions->empty()) return;
  
  if (session_count_ >= kMaxSessions) {
    ShowSessionLimitWarning();
    return;
  }
  
  // Create new empty session (cannot actually duplicate due to non-movable editors)
  // TODO: Implement proper duplication when editors become movable
  sessions->emplace_back();
  UpdateSessionCount();
  
  // Set as active session
  active_session_index_ = sessions->size() - 1;
  
  printf("[SessionCoordinator] Duplicated session %zu (total: %zu)\n", 
         active_session_index_, session_count_);
  
  ShowSessionOperationResult("Duplicate Session", true);
}

void SessionCoordinator::CloseCurrentSession() {
  CloseSession(active_session_index_);
}

void SessionCoordinator::CloseSession(size_t index) {
  auto* sessions = GET_SESSIONS();
  if (!sessions || !IsValidSessionIndex(index)) return;
  
  if (session_count_ <= kMinSessions) {
    // Don't allow closing the last session
    if (toast_manager_) {
      toast_manager_->Show("Cannot close the last session", ToastType::kWarning);
    }
    return;
  }
  
  // Unregister cards for this session
  if (card_registry_) {
    card_registry_->UnregisterSession(index);
  }
  
  // Mark session as closed (don't erase due to non-movable editors)
  // TODO: Implement proper session removal when editors become movable
  sessions->at(index).custom_name = "[CLOSED SESSION]";
  
  // Note: We don't actually remove from the deque because EditorSet is not movable
  // This is a temporary solution until we refactor to use unique_ptr<EditorSet>
  UpdateSessionCount();
  
  // Adjust active session index
  if (active_session_index_ >= index && active_session_index_ > 0) {
    active_session_index_--;
  }
  
  printf("[SessionCoordinator] Closed session %zu (total: %zu)\n", 
         index, session_count_);
  
  ShowSessionOperationResult("Close Session", true);
}

void SessionCoordinator::RemoveSession(size_t index) {
  CloseSession(index);
}

void SessionCoordinator::SwitchToSession(size_t index) {
  if (!IsValidSessionIndex(index)) return;
  
  active_session_index_ = index;
  
  if (card_registry_) {
    card_registry_->SetActiveSession(index);
  }
  
  printf("[SessionCoordinator] Switched to session %zu\n", index);
}

void SessionCoordinator::ActivateSession(size_t index) {
  SwitchToSession(index);
}

size_t SessionCoordinator::GetActiveSessionIndex() const {
  return active_session_index_;
}

void* SessionCoordinator::GetActiveSession() {
  auto* sessions = GET_SESSIONS();
  if (!sessions || !IsValidSessionIndex(active_session_index_)) {
    return nullptr;
  }
  return &sessions->at(active_session_index_);
}

void* SessionCoordinator::GetSession(size_t index) {
  auto* sessions = GET_SESSIONS();
  if (!sessions || !IsValidSessionIndex(index)) {
    return nullptr;
  }
  return &sessions->at(index);
}

bool SessionCoordinator::HasMultipleSessions() const {
  return session_count_ > 1;
}

size_t SessionCoordinator::GetActiveSessionCount() const {
  return session_count_;
}

bool SessionCoordinator::HasDuplicateSession(const std::string& filepath) const {
  auto* sessions = GET_SESSIONS();
  if (!sessions || filepath.empty()) return false;
  
  for (const auto& session : *sessions) {
    if (session.filepath == filepath) {
      return true;
    }
  }
  return false;
}

void SessionCoordinator::DrawSessionSwitcher() {
  auto* sessions = GET_SESSIONS();
  if (!sessions || sessions->empty()) return;
  
  if (!show_session_switcher_) return;
  
  ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), 
                         ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  
  if (!ImGui::Begin("Session Switcher", &show_session_switcher_)) {
    ImGui::End();
    return;
  }
  
  ImGui::Text("%s Active Sessions (%zu)", ICON_MD_TAB, session_count_);
  ImGui::Separator();
  
  for (size_t i = 0; i < sessions->size(); ++i) {
    const auto& session = sessions->at(i);
    bool is_active = (i == active_session_index_);
    
    ImGui::PushID(static_cast<int>(i));
    
    // Session tab
    if (ImGui::Selectable(GetSessionDisplayName(i).c_str(), is_active)) {
      SwitchToSession(i);
    }
    
    // Right-click context menu
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("SessionContextMenu");
    }
    
    if (ImGui::BeginPopup("SessionContextMenu")) {
      DrawSessionContextMenu(i);
      ImGui::EndPopup();
    }
    
    ImGui::PopID();
  }
  
  ImGui::Separator();
  
  // Action buttons
  if (ImGui::Button(absl::StrFormat("%s New Session", ICON_MD_ADD).c_str())) {
    CreateNewSession();
  }
  
  ImGui::SameLine();
  if (ImGui::Button(absl::StrFormat("%s Duplicate", ICON_MD_CONTENT_COPY).c_str())) {
    DuplicateCurrentSession();
  }
  
  ImGui::SameLine();
  if (HasMultipleSessions() && ImGui::Button(absl::StrFormat("%s Close", ICON_MD_CLOSE).c_str())) {
    CloseCurrentSession();
  }
  
  ImGui::End();
}

void SessionCoordinator::DrawSessionManager() {
  auto* sessions = GET_SESSIONS();
  if (!sessions || sessions->empty()) return;
  
  if (!show_session_manager_) return;
  
  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), 
                         ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  
  if (!ImGui::Begin("Session Manager", &show_session_manager_)) {
    ImGui::End();
    return;
  }
  
  // Session statistics
  ImGui::Text("%s Session Statistics", ICON_MD_ANALYTICS);
  ImGui::Separator();
  
  ImGui::Text("Total Sessions: %zu", GetTotalSessionCount());
  ImGui::Text("Loaded Sessions: %zu", GetLoadedSessionCount());
  ImGui::Text("Empty Sessions: %zu", GetEmptySessionCount());
  
  ImGui::Spacing();
  
  // Session list
  if (ImGui::BeginTable("SessionTable", 4, 
                       ImGuiTableFlags_Borders | 
                       ImGuiTableFlags_RowBg |
                       ImGuiTableFlags_Resizable)) {
    
    ImGui::TableSetupColumn("Session", ImGuiTableColumnFlags_WidthStretch, 0.3f);
    ImGui::TableSetupColumn("ROM File", ImGuiTableColumnFlags_WidthStretch, 0.4f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.2f);
    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableHeadersRow();
    
    for (size_t i = 0; i < sessions->size(); ++i) {
      const auto& session = sessions->at(i);
      bool is_active = (i == active_session_index_);
      
      ImGui::PushID(static_cast<int>(i));
      
      ImGui::TableNextRow();
      
      // Session name
      ImGui::TableNextColumn();
      if (is_active) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s %s", 
                          ICON_MD_RADIO_BUTTON_CHECKED, GetSessionDisplayName(i).c_str());
      } else {
        ImGui::Text("%s %s", ICON_MD_RADIO_BUTTON_UNCHECKED, GetSessionDisplayName(i).c_str());
      }
      
      // ROM file
      ImGui::TableNextColumn();
      if (session.rom.is_loaded()) {
        ImGui::Text("%s", session.filepath.c_str());
      } else {
        ImGui::TextDisabled("(No ROM loaded)");
      }
      
      // Status
      ImGui::TableNextColumn();
      if (session.rom.is_loaded()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Loaded");
      } else {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Empty");
      }
      
      // Actions
      ImGui::TableNextColumn();
      if (!is_active && ImGui::SmallButton("Switch")) {
        SwitchToSession(i);
      }
      
      ImGui::SameLine();
      if (HasMultipleSessions() && ImGui::SmallButton("Close")) {
        CloseSession(i);
      }
      
      ImGui::PopID();
    }
    
    ImGui::EndTable();
  }
  
  ImGui::End();
}

void SessionCoordinator::DrawSessionRenameDialog() {
  if (!show_session_rename_dialog_) return;
  
  ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_Always);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), 
                         ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  
  if (!ImGui::Begin("Rename Session", &show_session_rename_dialog_)) {
    ImGui::End();
    return;
  }
  
  ImGui::Text("Rename session %zu:", session_to_rename_);
  ImGui::InputText("Name", session_rename_buffer_, sizeof(session_rename_buffer_));
  
  ImGui::Spacing();
  
  if (ImGui::Button("OK")) {
    RenameSession(session_to_rename_, session_rename_buffer_);
    show_session_rename_dialog_ = false;
    session_rename_buffer_[0] = '\0';
  }
  
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    show_session_rename_dialog_ = false;
    session_rename_buffer_[0] = '\0';
  }
  
  ImGui::End();
}

void SessionCoordinator::DrawSessionTabs() {
  auto* sessions = GET_SESSIONS();
  if (!sessions || sessions->empty()) return;
  
  if (ImGui::BeginTabBar("SessionTabs")) {
    for (size_t i = 0; i < sessions->size(); ++i) {
      bool is_active = (i == active_session_index_);
      const auto& session = sessions->at(i);
      
      std::string tab_name = GetSessionDisplayName(i);
      if (session.rom.is_loaded()) {
        tab_name += " ";
        tab_name += ICON_MD_CHECK_CIRCLE;
      }
      
      if (ImGui::BeginTabItem(tab_name.c_str())) {
        if (!is_active) {
          SwitchToSession(i);
        }
        ImGui::EndTabItem();
      }
      
      // Right-click context menu
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup(absl::StrFormat("SessionTabContext_%zu", i).c_str());
      }
      
      if (ImGui::BeginPopup(absl::StrFormat("SessionTabContext_%zu", i).c_str())) {
        DrawSessionContextMenu(i);
        ImGui::EndPopup();
      }
    }
    ImGui::EndTabBar();
  }
}

void SessionCoordinator::DrawSessionIndicator() {
  if (!HasMultipleSessions()) return;
  
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  ImVec4 accent_color = ConvertColorToImVec4(theme.accent);
  
  ImGui::PushStyleColor(ImGuiCol_Text, accent_color);
  ImGui::Text("%s Session %zu", ICON_MD_TAB, active_session_index_);
  ImGui::PopStyleColor();
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Active Session: %s\nClick to open session switcher", 
                     GetActiveSessionDisplayName().c_str());
  }
  
  if (ImGui::IsItemClicked()) {
    ToggleSessionSwitcher();
  }
}

std::string SessionCoordinator::GetSessionDisplayName(size_t index) const {
  auto* sessions = GET_SESSIONS();
  if (!sessions || !IsValidSessionIndex(index)) {
    return "Invalid Session";
  }
  
  const auto& session = sessions->at(index);
  
  if (!session.custom_name.empty()) {
    return session.custom_name;
  }
  
  if (session.rom.is_loaded()) {
    return absl::StrFormat("Session %zu (%s)", index, 
                          std::filesystem::path(session.filepath).stem().string());
  }
  
  return absl::StrFormat("Session %zu (Empty)", index);
}

std::string SessionCoordinator::GetActiveSessionDisplayName() const {
  return GetSessionDisplayName(active_session_index_);
}

void SessionCoordinator::RenameSession(size_t index, const std::string& new_name) {
  auto* sessions = GET_SESSIONS();
  if (!sessions || !IsValidSessionIndex(index) || new_name.empty()) return;
  
  sessions->at(index).custom_name = new_name;
  printf("[SessionCoordinator] Renamed session %zu to '%s'\n", index, new_name.c_str());
}

std::string SessionCoordinator::GenerateUniqueEditorTitle(
    const std::string& editor_name, size_t session_index) const {
  auto* sessions = GET_SESSIONS();
  
  if (!sessions || sessions->size() <= 1) {
    // Single session - use simple name
    return editor_name;
  }

  if (session_index >= sessions->size()) {
    return editor_name;
  }

  // Multi-session - include session identifier
  const auto& session = sessions->at(session_index);
  std::string session_name = session.custom_name.empty() 
      ? session.rom.title() 
      : session.custom_name;

  // Truncate long session names
  if (session_name.length() > 20) {
    session_name = session_name.substr(0, 17) + "...";
  }

  return absl::StrFormat("%s - %s##session_%zu", editor_name, session_name,
                         session_index);
}

void SessionCoordinator::SetActiveSessionIndex(size_t index) {
  SwitchToSession(index);
}

void SessionCoordinator::UpdateSessionCount() {
  auto* sessions = GET_SESSIONS();
  if (sessions) {
    session_count_ = sessions->size();
  } else {
    session_count_ = 0;
  }
}

void SessionCoordinator::ShowAllCardsInActiveSession() {
  if (card_registry_) {
    card_registry_->ShowAllCardsInSession(active_session_index_);
  }
}

void SessionCoordinator::HideAllCardsInActiveSession() {
  if (card_registry_) {
    card_registry_->HideAllCardsInSession(active_session_index_);
  }
}

void SessionCoordinator::ShowCardsInCategory(const std::string& category) {
  if (card_registry_) {
    card_registry_->ShowAllCardsInCategory(active_session_index_, category);
  }
}

void SessionCoordinator::HideCardsInCategory(const std::string& category) {
  if (card_registry_) {
    card_registry_->HideAllCardsInCategory(active_session_index_, category);
  }
}

bool SessionCoordinator::IsValidSessionIndex(size_t index) const {
  auto* sessions = GET_SESSIONS();
  return sessions && index < sessions->size();
}

bool SessionCoordinator::IsSessionActive(size_t index) const {
  return index == active_session_index_;
}

bool SessionCoordinator::IsSessionLoaded(size_t index) const {
  auto* sessions = GET_SESSIONS();
  return IsValidSessionIndex(index) && sessions && sessions->at(index).rom.is_loaded();
}

size_t SessionCoordinator::GetTotalSessionCount() const {
  return session_count_;
}

size_t SessionCoordinator::GetLoadedSessionCount() const {
  auto* sessions = GET_SESSIONS();
  if (!sessions) return 0;
  
  size_t count = 0;
  for (const auto& session : *sessions) {
    if (session.rom.is_loaded()) {
      count++;
    }
  }
  return count;
}

size_t SessionCoordinator::GetEmptySessionCount() const {
  return session_count_ - GetLoadedSessionCount();
}

absl::Status SessionCoordinator::LoadRomIntoSession(const std::string& filename, size_t session_index) {
  auto* sessions = GET_SESSIONS();
  if (!sessions || filename.empty()) {
    return absl::InvalidArgumentError("Invalid parameters");
  }
  
  size_t target_index = (session_index == SIZE_MAX) ? active_session_index_ : session_index;
  if (!IsValidSessionIndex(target_index)) {
    return absl::InvalidArgumentError("Invalid session index");
  }
  
  // TODO: Implement actual ROM loading
  printf("[SessionCoordinator] LoadRomIntoSession: %s -> session %zu\n", 
         filename.c_str(), target_index);
  
  return absl::OkStatus();
}

absl::Status SessionCoordinator::SaveActiveSession(const std::string& filename) {
  auto* sessions = GET_SESSIONS();
  if (!sessions || !IsValidSessionIndex(active_session_index_)) {
    return absl::FailedPreconditionError("No active session");
  }
  
  // TODO: Implement actual ROM saving
  printf("[SessionCoordinator] SaveActiveSession: session %zu\n", active_session_index_);
  
  return absl::OkStatus();
}

absl::Status SessionCoordinator::SaveSessionAs(size_t session_index, const std::string& filename) {
  auto* sessions = GET_SESSIONS();
  if (!sessions || !IsValidSessionIndex(session_index) || filename.empty()) {
    return absl::InvalidArgumentError("Invalid parameters");
  }
  
  // TODO: Implement actual ROM saving
  printf("[SessionCoordinator] SaveSessionAs: session %zu -> %s\n", 
         session_index, filename.c_str());
  
  return absl::OkStatus();
}

void SessionCoordinator::CleanupClosedSessions() {
  auto* sessions = GET_SESSIONS();
  if (!sessions) return;
  
  // Mark empty sessions as closed (except keep at least one)
  // TODO: Actually remove when editors become movable
  size_t loaded_count = 0;
  for (auto& session : *sessions) {
    if (session.rom.is_loaded()) {
      loaded_count++;
    }
  }
  
  if (loaded_count > 0) {
    for (auto& session : *sessions) {
      if (!session.rom.is_loaded() && sessions->size() > 1) {
        session.custom_name = "[CLOSED SESSION]";
      }
    }
  }
  
  UpdateSessionCount();
  printf("[SessionCoordinator] Cleaned up closed sessions (remaining: %zu)\n", session_count_);
}

void SessionCoordinator::ClearAllSessions() {
  auto* sessions = GET_SESSIONS();
  if (!sessions) return;
  
  // Unregister all session cards
  if (card_registry_) {
    for (size_t i = 0; i < sessions->size(); ++i) {
      card_registry_->UnregisterSession(i);
    }
  }
  
  // Mark all sessions as closed instead of clearing
  // TODO: Actually clear when editors become movable
  for (auto& session : *sessions) {
    session.custom_name = "[CLOSED SESSION]";
  }
  
  active_session_index_ = 0;
  UpdateSessionCount();
  
  printf("[SessionCoordinator] Cleared all sessions\n");
}

void SessionCoordinator::FocusNextSession() {
  auto* sessions = GET_SESSIONS();
  if (!sessions || sessions->empty()) return;
  
  size_t next_index = (active_session_index_ + 1) % sessions->size();
  SwitchToSession(next_index);
}

void SessionCoordinator::FocusPreviousSession() {
  auto* sessions = GET_SESSIONS();
  if (!sessions || sessions->empty()) return;
  
  size_t prev_index = (active_session_index_ == 0) ? 
                     sessions->size() - 1 : active_session_index_ - 1;
  SwitchToSession(prev_index);
}

void SessionCoordinator::FocusFirstSession() {
  auto* sessions = GET_SESSIONS();
  if (!sessions || sessions->empty()) return;
  SwitchToSession(0);
}

void SessionCoordinator::FocusLastSession() {
  auto* sessions = GET_SESSIONS();
  if (!sessions || sessions->empty()) return;
  SwitchToSession(sessions->size() - 1);
}

void SessionCoordinator::UpdateActiveSession() {
  auto* sessions = GET_SESSIONS();
  if (sessions && !sessions->empty() && active_session_index_ >= sessions->size()) {
    active_session_index_ = sessions->size() - 1;
  }
}

void SessionCoordinator::ValidateSessionIndex(size_t index) const {
  if (!IsValidSessionIndex(index)) {
    throw std::out_of_range(absl::StrFormat("Invalid session index: %zu", index));
  }
}

std::string SessionCoordinator::GenerateUniqueSessionName(const std::string& base_name) const {
  auto* sessions = GET_SESSIONS();
  if (!sessions) return base_name;
  
  std::string name = base_name;
  int counter = 1;
  
  while (true) {
    bool found = false;
    for (const auto& session : *sessions) {
      if (session.custom_name == name) {
        found = true;
        break;
      }
    }
    
    if (!found) break;
    
    name = absl::StrFormat("%s %d", base_name, counter++);
  }
  
  return name;
}

void SessionCoordinator::ShowSessionLimitWarning() {
  if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Maximum %zu sessions allowed", kMaxSessions), 
        ToastType::kWarning);
  }
}

void SessionCoordinator::ShowSessionOperationResult(const std::string& operation, bool success) {
  if (toast_manager_) {
    std::string message = absl::StrFormat("%s %s", operation, 
                                         success ? "succeeded" : "failed");
    ToastType type = success ? ToastType::kSuccess : ToastType::kError;
    toast_manager_->Show(message, type);
  }
}

void SessionCoordinator::DrawSessionTab(size_t index, bool is_active) {
  auto* sessions = GET_SESSIONS();
  if (!sessions || index >= sessions->size()) return;
  
  const auto& session = sessions->at(index);
  
  ImVec4 color = GetSessionColor(index);
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  
  std::string tab_name = GetSessionDisplayName(index);
  if (session.rom.is_loaded()) {
    tab_name += " ";
    tab_name += ICON_MD_CHECK_CIRCLE;
  }
  
  if (ImGui::BeginTabItem(tab_name.c_str())) {
    if (!is_active) {
      SwitchToSession(index);
    }
    ImGui::EndTabItem();
  }
  
  ImGui::PopStyleColor();
}

void SessionCoordinator::DrawSessionContextMenu(size_t index) {
  if (ImGui::MenuItem(absl::StrFormat("%s Switch to Session", ICON_MD_TAB).c_str())) {
    SwitchToSession(index);
  }
  
  if (ImGui::MenuItem(absl::StrFormat("%s Rename", ICON_MD_EDIT).c_str())) {
    session_to_rename_ = index;
    strncpy(session_rename_buffer_, GetSessionDisplayName(index).c_str(), 
            sizeof(session_rename_buffer_) - 1);
    session_rename_buffer_[sizeof(session_rename_buffer_) - 1] = '\0';
    show_session_rename_dialog_ = true;
  }
  
  if (ImGui::MenuItem(absl::StrFormat("%s Duplicate", ICON_MD_CONTENT_COPY).c_str())) {
    // TODO: Implement session duplication
  }
  
  ImGui::Separator();
  
  if (HasMultipleSessions() && 
      ImGui::MenuItem(absl::StrFormat("%s Close Session", ICON_MD_CLOSE).c_str())) {
    CloseSession(index);
  }
}

void SessionCoordinator::DrawSessionBadge(size_t index) {
  auto* sessions = GET_SESSIONS();
  if (!sessions || index >= sessions->size()) return;
  
  const auto& session = sessions->at(index);
  ImVec4 color = GetSessionColor(index);
  
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  
  if (session.rom.is_loaded()) {
    ImGui::Text("%s", ICON_MD_CHECK_CIRCLE);
  } else {
    ImGui::Text("%s", ICON_MD_RADIO_BUTTON_UNCHECKED);
  }
  
  ImGui::PopStyleColor();
}

ImVec4 SessionCoordinator::GetSessionColor(size_t index) const {
  // Generate consistent colors for sessions
  static const ImVec4 colors[] = {
    ImVec4(0.0f, 1.0f, 0.0f, 1.0f),  // Green
    ImVec4(0.0f, 0.5f, 1.0f, 1.0f),  // Blue
    ImVec4(1.0f, 0.5f, 0.0f, 1.0f),  // Orange
    ImVec4(1.0f, 0.0f, 1.0f, 1.0f),  // Magenta
    ImVec4(1.0f, 1.0f, 0.0f, 1.0f),  // Yellow
    ImVec4(0.0f, 1.0f, 1.0f, 1.0f),  // Cyan
    ImVec4(1.0f, 0.0f, 0.0f, 1.0f),  // Red
    ImVec4(0.5f, 0.5f, 0.5f, 1.0f),  // Gray
  };
  
  return colors[index % (sizeof(colors) / sizeof(colors[0]))];
}

std::string SessionCoordinator::GetSessionIcon(size_t index) const {
  auto* sessions = GET_SESSIONS();
  if (!sessions || index >= sessions->size()) return ICON_MD_RADIO_BUTTON_UNCHECKED;
  
  const auto& session = sessions->at(index);
  
  if (session.rom.is_loaded()) {
    return ICON_MD_CHECK_CIRCLE;
  } else {
    return ICON_MD_RADIO_BUTTON_UNCHECKED;
  }
}

bool SessionCoordinator::IsSessionEmpty(size_t index) const {
  auto* sessions = GET_SESSIONS();
  return IsValidSessionIndex(index) && sessions && !sessions->at(index).rom.is_loaded();
}

bool SessionCoordinator::IsSessionClosed(size_t index) const {
  return !IsValidSessionIndex(index);
}

bool SessionCoordinator::IsSessionModified(size_t index) const {
  // TODO: Implement modification tracking
  return false;
}

}  // namespace editor
}  // namespace yaze
