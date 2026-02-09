#include "session_coordinator.h"
#include <absl/status/status.h>
#include <absl/status/statusor.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor_manager.h"
#include "app/editor/events/core_events.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/session_types.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "core/color.h"
#include "editor/editor.h"
#include "editor/system/user_settings.h"
#include "editor/ui/toast_manager.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

SessionCoordinator::SessionCoordinator(PanelManager* panel_manager,
                                       ToastManager* toast_manager,
                                       UserSettings* user_settings)
    : panel_manager_(panel_manager),
      toast_manager_(toast_manager),
      user_settings_(user_settings) {}

void SessionCoordinator::AddObserver(SessionObserver* observer) {
  if (observer) {
    observers_.push_back(observer);
  }
}

void SessionCoordinator::RemoveObserver(SessionObserver* observer) {
  observers_.erase(std::remove(observers_.begin(), observers_.end(), observer),
                   observers_.end());
}

void SessionCoordinator::NotifySessionSwitched(size_t index,
                                               RomSession* session) {
  // Notify traditional observers
  for (auto* observer : observers_) {
    observer->OnSessionSwitched(index, session);
  }

  // Publish event to EventBus (dual-path during transition)
  if (event_bus_) {
    // Track previous index for the event (before we update active_session_index_)
    size_t old_index = active_session_index_;
    event_bus_->Publish(
        SessionSwitchedEvent::Create(old_index, index, session));
  }
}

void SessionCoordinator::NotifySessionCreated(size_t index,
                                              RomSession* session) {
  // Notify traditional observers
  for (auto* observer : observers_) {
    observer->OnSessionCreated(index, session);
  }

  // Publish event to EventBus (dual-path during transition)
  if (event_bus_) {
    event_bus_->Publish(SessionCreatedEvent::Create(index, session));
  }
}

void SessionCoordinator::NotifySessionClosed(size_t index) {
  // Notify traditional observers
  for (auto* observer : observers_) {
    observer->OnSessionClosed(index);
  }

  // Publish event to EventBus (dual-path during transition)
  if (event_bus_) {
    event_bus_->Publish(SessionClosedEvent::Create(index));
  }
}

void SessionCoordinator::NotifySessionRomLoaded(size_t index,
                                                RomSession* session) {
  // Notify traditional observers
  for (auto* observer : observers_) {
    observer->OnSessionRomLoaded(index, session);
  }

  // Publish event to EventBus (dual-path during transition)
  if (event_bus_ && session) {
    event_bus_->Publish(
        RomLoadedEvent::Create(&session->rom, session->filepath, index));
  }
}

void SessionCoordinator::CreateNewSession() {
  if (session_count_ >= kMaxSessions) {
    ShowSessionLimitWarning();
    return;
  }

  // Create new empty session
  sessions_.push_back(std::make_unique<RomSession>());
  UpdateSessionCount();

  // Set as active session
  active_session_index_ = sessions_.size() - 1;

  // Configure the new session
  if (editor_manager_) {
    auto& session = sessions_.back();
    editor_manager_->ConfigureSession(session.get());
  }

  LOG_INFO("SessionCoordinator", "Created new session %zu (total: %zu)",
           active_session_index_, session_count_);

  // Notify observers
  NotifySessionCreated(active_session_index_, sessions_.back().get());

  ShowSessionOperationResult("Create Session", true);
}

void SessionCoordinator::DuplicateCurrentSession() {
  if (sessions_.empty())
    return;

  if (session_count_ >= kMaxSessions) {
    ShowSessionLimitWarning();
    return;
  }

  // Create new empty session (cannot actually duplicate due to non-movable
  // editors)
  // TODO: Implement proper duplication when editors become movable
  sessions_.push_back(std::make_unique<RomSession>());
  UpdateSessionCount();

  // Set as active session
  active_session_index_ = sessions_.size() - 1;

  // Configure the new session
  if (editor_manager_) {
    auto& session = sessions_.back();
    editor_manager_->ConfigureSession(session.get());
  }

  LOG_INFO("SessionCoordinator", "Duplicated session %zu (total: %zu)",
           active_session_index_, session_count_);

  // Notify observers
  NotifySessionCreated(active_session_index_, sessions_.back().get());

  ShowSessionOperationResult("Duplicate Session", true);
}

void SessionCoordinator::CloseCurrentSession() {
  CloseSession(active_session_index_);
}

void SessionCoordinator::CloseSession(size_t index) {
  if (!IsValidSessionIndex(index))
    return;

  if (session_count_ <= kMinSessions) {
    // Don't allow closing the last session
    if (toast_manager_) {
      toast_manager_->Show("Cannot close the last session",
                           ToastType::kWarning);
    }
    return;
  }

  // Unregister cards for this session
  if (panel_manager_) {
    panel_manager_->UnregisterSession(index);
  }

  // Notify observers before removal
  NotifySessionClosed(index);

  // Remove session (safe now with unique_ptr!)
  sessions_.erase(sessions_.begin() + index);
  UpdateSessionCount();

  // Adjust active session index
  if (active_session_index_ >= index && active_session_index_ > 0) {
    active_session_index_--;
  }

  LOG_INFO("SessionCoordinator", "Closed session %zu (total: %zu)", index,
           session_count_);

  ShowSessionOperationResult("Close Session", true);
}

void SessionCoordinator::RemoveSession(size_t index) {
  CloseSession(index);
}

void SessionCoordinator::SwitchToSession(size_t index) {
  if (!IsValidSessionIndex(index))
    return;

  size_t old_index = active_session_index_;
  active_session_index_ = index;

  if (panel_manager_) {
    panel_manager_->SetActiveSession(index);
  }

  // Only notify if actually switching to a different session
  if (old_index != index) {
    NotifySessionSwitched(index, sessions_[index].get());
  }
}

void SessionCoordinator::ActivateSession(size_t index) {
  SwitchToSession(index);
}

size_t SessionCoordinator::GetActiveSessionIndex() const {
  return active_session_index_;
}

void* SessionCoordinator::GetActiveSession() const {
  if (!IsValidSessionIndex(active_session_index_)) {
    return nullptr;
  }
  return sessions_[active_session_index_].get();
}

RomSession* SessionCoordinator::GetActiveRomSession() const {
  return static_cast<RomSession*>(GetActiveSession());
}

Rom* SessionCoordinator::GetCurrentRom() const {
  auto* session = GetActiveRomSession();
  return session ? &session->rom : nullptr;
}

zelda3::GameData* SessionCoordinator::GetCurrentGameData() const {
  auto* session = GetActiveRomSession();
  return session ? &session->game_data : nullptr;
}

EditorSet* SessionCoordinator::GetCurrentEditorSet() const {
  auto* session = GetActiveRomSession();
  return session ? &session->editors : nullptr;
}

void* SessionCoordinator::GetSession(size_t index) const {
  if (!IsValidSessionIndex(index)) {
    return nullptr;
  }
  return sessions_[index].get();
}

bool SessionCoordinator::HasMultipleSessions() const {
  return session_count_ > 1;
}

size_t SessionCoordinator::GetActiveSessionCount() const {
  return session_count_;
}

bool SessionCoordinator::HasDuplicateSession(
    const std::string& filepath) const {
  if (filepath.empty())
    return false;

  for (const auto& session : sessions_) {
    if (session->filepath == filepath) {
      return true;
    }
  }
  return false;
}

void SessionCoordinator::DrawSessionSwitcher() {
  if (sessions_.empty())
    return;

  if (!show_session_switcher_)
    return;

  ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (!ImGui::Begin("Session Switcher", &show_session_switcher_)) {
    ImGui::End();
    return;
  }

  ImGui::Text("%s Active Sessions (%zu)", ICON_MD_TAB, session_count_);
  ImGui::Separator();

  for (size_t i = 0; i < sessions_.size(); ++i) {
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
  if (ImGui::Button(
          absl::StrFormat("%s Duplicate", ICON_MD_CONTENT_COPY).c_str())) {
    DuplicateCurrentSession();
  }

  ImGui::SameLine();
  if (HasMultipleSessions() &&
      ImGui::Button(absl::StrFormat("%s Close", ICON_MD_CLOSE).c_str())) {
    CloseCurrentSession();
  }

  ImGui::End();
}

void SessionCoordinator::DrawSessionManager() {
  if (sessions_.empty())
    return;

  if (!show_session_manager_)
    return;

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
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("Session", ImGuiTableColumnFlags_WidthStretch,
                            0.3f);
    ImGui::TableSetupColumn("ROM File", ImGuiTableColumnFlags_WidthStretch,
                            0.4f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.2f);
    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                            120.0f);
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < sessions_.size(); ++i) {
      const auto& session = sessions_[i];
      bool is_active = (i == active_session_index_);

      ImGui::PushID(static_cast<int>(i));

      ImGui::TableNextRow();

      // Session name
      ImGui::TableNextColumn();
      if (is_active) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s %s",
                           ICON_MD_RADIO_BUTTON_CHECKED,
                           GetSessionDisplayName(i).c_str());
      } else {
        ImGui::Text("%s %s", ICON_MD_RADIO_BUTTON_UNCHECKED,
                    GetSessionDisplayName(i).c_str());
      }

      // ROM file
      ImGui::TableNextColumn();
      if (session->rom.is_loaded()) {
        ImGui::Text("%s", session->filepath.c_str());
      } else {
        ImGui::TextDisabled("(No ROM loaded)");
      }

      // Status
      ImGui::TableNextColumn();
      if (session->rom.is_loaded()) {
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
  if (!show_session_rename_dialog_)
    return;

  ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_Always);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));

  if (!ImGui::Begin("Rename Session", &show_session_rename_dialog_)) {
    ImGui::End();
    return;
  }

  ImGui::Text("Rename session %zu:", session_to_rename_);
  ImGui::InputText("Name", session_rename_buffer_,
                   sizeof(session_rename_buffer_));

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
  if (sessions_.empty())
    return;

  if (ImGui::BeginTabBar("SessionTabs")) {
    for (size_t i = 0; i < sessions_.size(); ++i) {
      bool is_active = (i == active_session_index_);
      const auto& session = sessions_[i];

      std::string tab_name = GetSessionDisplayName(i);
      if (session->rom.is_loaded()) {
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

      if (ImGui::BeginPopup(
              absl::StrFormat("SessionTabContext_%zu", i).c_str())) {
        DrawSessionContextMenu(i);
        ImGui::EndPopup();
      }
    }
    ImGui::EndTabBar();
  }
}

void SessionCoordinator::DrawSessionIndicator() {
  if (!HasMultipleSessions())
    return;

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
  if (!IsValidSessionIndex(index)) {
    return "Invalid Session";
  }

  const auto& session = sessions_[index];

  if (!session->custom_name.empty()) {
    return session->custom_name;
  }

  if (session->rom.is_loaded()) {
    return absl::StrFormat(
        "Session %zu (%s)", index,
        std::filesystem::path(session->filepath).stem().string());
  }

  return absl::StrFormat("Session %zu (Empty)", index);
}

std::string SessionCoordinator::GetActiveSessionDisplayName() const {
  return GetSessionDisplayName(active_session_index_);
}

void SessionCoordinator::RenameSession(size_t index,
                                       const std::string& new_name) {
  if (!IsValidSessionIndex(index) || new_name.empty())
    return;

  sessions_[index]->custom_name = new_name;
  LOG_INFO("SessionCoordinator", "Renamed session %zu to '%s'", index,
           new_name.c_str());
}

std::string SessionCoordinator::GenerateUniqueEditorTitle(
    const std::string& editor_name, size_t session_index) const {
  if (sessions_.size() <= 1) {
    // Single session - use simple name
    return editor_name;
  }

  if (session_index >= sessions_.size()) {
    return editor_name;
  }

  // Multi-session - include session identifier
  const auto& session = sessions_[session_index];
  std::string session_name = session->custom_name.empty()
                                 ? session->rom.title()
                                 : session->custom_name;

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
  session_count_ = sessions_.size();
}

// Panel coordination across sessions
void SessionCoordinator::ShowAllPanelsInActiveSession() {
  if (panel_manager_) {
    panel_manager_->ShowAllPanelsInSession(active_session_index_);
  }
}

void SessionCoordinator::HideAllPanelsInActiveSession() {
  if (panel_manager_) {
    panel_manager_->HideAllPanelsInSession(active_session_index_);
  }
}

void SessionCoordinator::ShowPanelsInCategory(const std::string& category) {
  if (panel_manager_) {
    panel_manager_->ShowAllPanelsInCategory(active_session_index_, category);
  }
}

void SessionCoordinator::HidePanelsInCategory(const std::string& category) {
  if (panel_manager_) {
    panel_manager_->HideAllPanelsInCategory(active_session_index_, category);
  }
}

bool SessionCoordinator::IsValidSessionIndex(size_t index) const {
  return index < sessions_.size();
}

void SessionCoordinator::UpdateSessions() {
  if (sessions_.empty())
    return;

  size_t original_session_idx = active_session_index_;

  for (size_t session_idx = 0; session_idx < sessions_.size(); ++session_idx) {
    auto& session = sessions_[session_idx];
    if (!session->rom.is_loaded())
      continue;  // Skip sessions with invalid ROMs

    // Switch context
    SwitchToSession(session_idx);

    for (auto editor : session->editors.active_editors_) {
      if (*editor->active()) {
        if (editor->type() == EditorType::kOverworld) {
          auto& overworld_editor = static_cast<OverworldEditor&>(*editor);
          if (overworld_editor.jump_to_tab() != -1) {
            // Set the dungeon editor to the jump to tab
            session->editors.GetDungeonEditor()->add_room(
                overworld_editor.jump_to_tab());
            overworld_editor.jump_to_tab_ = -1;
          }
        }

        // CARD-BASED EDITORS: Don't wrap in Begin/End, they manage own windows
        bool is_card_based_editor =
            EditorManager::IsPanelBasedEditor(editor->type());

        if (is_card_based_editor) {
          // Panel-based editors create their own top-level windows
          // No parent wrapper needed - this allows independent docking
          if (editor_manager_) {
            editor_manager_->SetCurrentEditor(editor);
          }

          absl::Status status = editor->Update();

          // Route editor errors to toast manager
          if (!status.ok() && toast_manager_) {
            std::string editor_name =
                kEditorNames[static_cast<int>(editor->type())];
            toast_manager_->Show(
                absl::StrFormat("%s Error: %s", editor_name, status.message()),
                ToastType::kError, 8.0f);
          }

        } else {
          // TRADITIONAL EDITORS: Wrap in Begin/End
          std::string window_title = GenerateUniqueEditorTitle(
              kEditorNames[static_cast<int>(editor->type())], session_idx);

          // Set window to maximize on first open
          ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize,
                                   ImGuiCond_FirstUseEver);
          ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos,
                                  ImGuiCond_FirstUseEver);

          if (ImGui::Begin(window_title.c_str(), editor->active(),
                           ImGuiWindowFlags_None)) {  // Allow full docking
            // Temporarily switch context for this editor's update
            // (Already switched via SwitchToSession)
            if (editor_manager_) {
              editor_manager_->SetCurrentEditor(editor);
            }

            absl::Status status = editor->Update();

            // Route editor errors to toast manager
            if (!status.ok() && toast_manager_) {
              std::string editor_name =
                  kEditorNames[static_cast<int>(editor->type())];
              toast_manager_->Show(absl::StrFormat("%s Error: %s", editor_name,
                                                   status.message()),
                                   ToastType::kError, 8.0f);
            }
          }
          ImGui::End();
        }
      }
    }
  }

  // Restore original session context
  SwitchToSession(original_session_idx);
}

bool SessionCoordinator::IsSessionActive(size_t index) const {
  return index == active_session_index_;
}

bool SessionCoordinator::IsSessionLoaded(size_t index) const {
  return IsValidSessionIndex(index) && sessions_[index]->rom.is_loaded();
}

size_t SessionCoordinator::GetTotalSessionCount() const {
  return session_count_;
}

size_t SessionCoordinator::GetLoadedSessionCount() const {
  size_t count = 0;
  for (const auto& session : sessions_) {
    if (session->rom.is_loaded()) {
      count++;
    }
  }
  return count;
}

size_t SessionCoordinator::GetEmptySessionCount() const {
  return session_count_ - GetLoadedSessionCount();
}

absl::Status SessionCoordinator::LoadRomIntoSession(const std::string& filename,
                                                    size_t session_index) {
  if (filename.empty()) {
    return absl::InvalidArgumentError("Invalid parameters");
  }

  size_t target_index =
      (session_index == SIZE_MAX) ? active_session_index_ : session_index;
  if (!IsValidSessionIndex(target_index)) {
    return absl::InvalidArgumentError("Invalid session index");
  }

  // TODO: Implement actual ROM loading
  LOG_INFO("SessionCoordinator", "LoadRomIntoSession: %s -> session %zu",
           filename.c_str(), target_index);

  return absl::OkStatus();
}

absl::Status SessionCoordinator::SaveActiveSession(
    const std::string& filename) {
  if (!IsValidSessionIndex(active_session_index_)) {
    return absl::FailedPreconditionError("No active session");
  }

  // TODO: Implement actual ROM saving
  LOG_INFO("SessionCoordinator", "SaveActiveSession: session %zu",
           active_session_index_);

  return absl::OkStatus();
}

absl::Status SessionCoordinator::SaveSessionAs(size_t session_index,
                                               const std::string& filename) {
  if (!IsValidSessionIndex(session_index) || filename.empty()) {
    return absl::InvalidArgumentError("Invalid parameters");
  }

  // TODO: Implement actual ROM saving
  LOG_INFO("SessionCoordinator", "SaveSessionAs: session %zu -> %s",
           session_index, filename.c_str());

  return absl::OkStatus();
}

absl::StatusOr<RomSession*> SessionCoordinator::CreateSessionFromRom(
    Rom&& rom, const std::string& filepath) {
  size_t new_session_id = sessions_.size();
  sessions_.push_back(std::make_unique<RomSession>(
      std::move(rom), user_settings_, new_session_id, editor_registry_));
  auto& session = sessions_.back();
  session->filepath = filepath;

  UpdateSessionCount();
  SwitchToSession(new_session_id);

  // Notify observers
  NotifySessionCreated(new_session_id, session.get());
  NotifySessionRomLoaded(new_session_id, session.get());

  return session.get();
}

void SessionCoordinator::CleanupClosedSessions() {
  // Mark empty sessions as closed (except keep at least one)
  size_t loaded_count = 0;
  for (const auto& session : sessions_) {
    if (session->rom.is_loaded()) {
      loaded_count++;
    }
  }

  if (loaded_count > 0) {
    for (auto it = sessions_.begin(); it != sessions_.end();) {
      if (!(*it)->rom.is_loaded() && sessions_.size() > 1) {
        it = sessions_.erase(it);
      } else {
        ++it;
      }
    }
  }

  UpdateSessionCount();
  LOG_INFO("SessionCoordinator", "Cleaned up closed sessions (remaining: %zu)",
           session_count_);
}

void SessionCoordinator::ClearAllSessions() {
  if (sessions_.empty())
    return;

  // Unregister all session cards
  if (panel_manager_) {
    for (size_t i = 0; i < sessions_.size(); ++i) {
      panel_manager_->UnregisterSession(i);
    }
  }

  sessions_.clear();
  active_session_index_ = 0;
  UpdateSessionCount();

  LOG_INFO("SessionCoordinator", "Cleared all sessions");
}

void SessionCoordinator::FocusNextSession() {
  if (sessions_.empty())
    return;

  size_t next_index = (active_session_index_ + 1) % sessions_.size();
  SwitchToSession(next_index);
}

void SessionCoordinator::FocusPreviousSession() {
  if (sessions_.empty())
    return;

  size_t prev_index = (active_session_index_ == 0) ? sessions_.size() - 1
                                                   : active_session_index_ - 1;
  SwitchToSession(prev_index);
}

void SessionCoordinator::FocusFirstSession() {
  if (sessions_.empty())
    return;
  SwitchToSession(0);
}

void SessionCoordinator::FocusLastSession() {
  if (sessions_.empty())
    return;
  SwitchToSession(sessions_.size() - 1);
}

void SessionCoordinator::UpdateActiveSession() {
  if (!sessions_.empty() && active_session_index_ >= sessions_.size()) {
    active_session_index_ = sessions_.size() - 1;
  }
}

void SessionCoordinator::ValidateSessionIndex(size_t index) const {
  if (!IsValidSessionIndex(index)) {
    throw std::out_of_range(
        absl::StrFormat("Invalid session index: %zu", index));
  }
}

std::string SessionCoordinator::GenerateUniqueSessionName(
    const std::string& base_name) const {
  if (sessions_.empty())
    return base_name;

  std::string name = base_name;
  int counter = 1;

  while (true) {
    bool found = false;
    for (const auto& session : sessions_) {
      if (session->custom_name == name) {
        found = true;
        break;
      }
    }

    if (!found)
      break;

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

void SessionCoordinator::ShowSessionOperationResult(
    const std::string& operation, bool success) {
  if (toast_manager_) {
    std::string message =
        absl::StrFormat("%s %s", operation, success ? "succeeded" : "failed");
    ToastType type = success ? ToastType::kSuccess : ToastType::kError;
    toast_manager_->Show(message, type);
  }
}

void SessionCoordinator::DrawSessionTab(size_t index, bool is_active) {
  if (index >= sessions_.size())
    return;

  const auto& session = sessions_[index];

  ImVec4 color = GetSessionColor(index);
  ImGui::PushStyleColor(ImGuiCol_Text, color);

  std::string tab_name = GetSessionDisplayName(index);
  if (session->rom.is_loaded()) {
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
  if (ImGui::MenuItem(
          absl::StrFormat("%s Switch to Session", ICON_MD_TAB).c_str())) {
    SwitchToSession(index);
  }

  if (ImGui::MenuItem(absl::StrFormat("%s Rename", ICON_MD_EDIT).c_str())) {
    session_to_rename_ = index;
    strncpy(session_rename_buffer_, GetSessionDisplayName(index).c_str(),
            sizeof(session_rename_buffer_) - 1);
    session_rename_buffer_[sizeof(session_rename_buffer_) - 1] = '\0';
    show_session_rename_dialog_ = true;
  }

  if (ImGui::MenuItem(
          absl::StrFormat("%s Duplicate", ICON_MD_CONTENT_COPY).c_str())) {
    // TODO: Implement session duplication
  }

  ImGui::Separator();

  if (HasMultipleSessions() &&
      ImGui::MenuItem(
          absl::StrFormat("%s Close Session", ICON_MD_CLOSE).c_str())) {
    CloseSession(index);
  }
}

void SessionCoordinator::DrawSessionBadge(size_t index) {
  if (index >= sessions_.size())
    return;

  const auto& session = sessions_[index];
  ImVec4 color = GetSessionColor(index);

  ImGui::PushStyleColor(ImGuiCol_Text, color);

  if (session->rom.is_loaded()) {
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
  if (index >= sessions_.size())
    return ICON_MD_RADIO_BUTTON_UNCHECKED;

  const auto& session = sessions_[index];

  if (session->rom.is_loaded()) {
    return ICON_MD_CHECK_CIRCLE;
  } else {
    return ICON_MD_RADIO_BUTTON_UNCHECKED;
  }
}

bool SessionCoordinator::IsSessionEmpty(size_t index) const {
  return IsValidSessionIndex(index) && !sessions_[index]->rom.is_loaded();
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
