#ifndef YAZE_APP_EDITOR_UI_SESSION_COORDINATOR_H_
#define YAZE_APP_EDITOR_UI_SESSION_COORDINATOR_H_

#include <deque>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/system/session_card_registry.h"
#include "app/editor/system/toast_manager.h"
#include "app/rom.h"
#include "imgui/imgui.h"

// Forward declarations
namespace yaze {
namespace editor {
class EditorManager;
}
}

namespace yaze {
namespace editor {

// Forward declarations
class EditorSet;
class ToastManager;

/**
 * @class SessionCoordinator
 * @brief High-level orchestrator for multi-session UI
 * 
 * Manages session list UI, coordinates card visibility across sessions,
 * handles session activation/deactivation, and provides session-aware editor queries.
 * 
 * This class lives in the ui/ layer and can depend on both system and gui components.
 */
class SessionCoordinator {
 public:
  explicit SessionCoordinator(void* sessions_ptr,
                             SessionCardRegistry* card_registry,
                             ToastManager* toast_manager);
  ~SessionCoordinator() = default;

  // Session lifecycle management
  void CreateNewSession();
  void DuplicateCurrentSession();
  void CloseCurrentSession();
  void CloseSession(size_t index);
  void RemoveSession(size_t index);
  void SwitchToSession(size_t index);
  
  // Session activation and queries
  void ActivateSession(size_t index);
  size_t GetActiveSessionIndex() const;
  void* GetActiveSession();
  void* GetSession(size_t index);
  bool HasMultipleSessions() const;
  size_t GetActiveSessionCount() const;
  bool HasDuplicateSession(const std::string& filepath) const;
  
  // Session UI components
  void DrawSessionSwitcher();
  void DrawSessionManager();
  void DrawSessionRenameDialog();
  void DrawSessionTabs();
  void DrawSessionIndicator();
  
  // Session information
  std::string GetSessionDisplayName(size_t index) const;
  std::string GetActiveSessionDisplayName() const;
  void RenameSession(size_t index, const std::string& new_name);
  std::string GenerateUniqueEditorTitle(const std::string& editor_name, size_t session_index) const;
  
  // Session state management
  void SetActiveSessionIndex(size_t index);
  void UpdateSessionCount();
  
  // Card coordination across sessions
  void ShowAllCardsInActiveSession();
  void HideAllCardsInActiveSession();
  void ShowCardsInCategory(const std::string& category);
  void HideCardsInCategory(const std::string& category);
  
  // Session validation
  bool IsValidSessionIndex(size_t index) const;
  bool IsSessionActive(size_t index) const;
  bool IsSessionLoaded(size_t index) const;
  
  // Session statistics
  size_t GetTotalSessionCount() const;
  size_t GetLoadedSessionCount() const;
  size_t GetEmptySessionCount() const;
  
  // Session operations with error handling
  absl::Status LoadRomIntoSession(const std::string& filename, size_t session_index = SIZE_MAX);
  absl::Status SaveActiveSession(const std::string& filename = "");
  absl::Status SaveSessionAs(size_t session_index, const std::string& filename);
  
  // Session cleanup
  void CleanupClosedSessions();
  void ClearAllSessions();
  
  // Session navigation
  void FocusNextSession();
  void FocusPreviousSession();
  void FocusFirstSession();
  void FocusLastSession();
  
  // Session UI state
  void ShowSessionSwitcher() { show_session_switcher_ = true; }
  void HideSessionSwitcher() { show_session_switcher_ = false; }
  void ToggleSessionSwitcher() { show_session_switcher_ = !show_session_switcher_; }
  bool IsSessionSwitcherVisible() const { return show_session_switcher_; }
  
  void ShowSessionManager() { show_session_manager_ = true; }
  void HideSessionManager() { show_session_manager_ = false; }
  void ToggleSessionManager() { show_session_manager_ = !show_session_manager_; }
  bool IsSessionManagerVisible() const { return show_session_manager_; }

 private:
  // Core dependencies
  void* sessions_ptr_;  // std::deque<EditorManager::RomSession>*
  SessionCardRegistry* card_registry_;
  ToastManager* toast_manager_;
  
  // Session state
  size_t active_session_index_ = 0;
  size_t session_count_ = 0;
  
  // UI state
  bool show_session_switcher_ = false;
  bool show_session_manager_ = false;
  bool show_session_rename_dialog_ = false;
  size_t session_to_rename_ = 0;
  char session_rename_buffer_[256] = {};
  
  // Session limits
  static constexpr size_t kMaxSessions = 8;
  static constexpr size_t kMinSessions = 1;
  
  // Helper methods
  void UpdateActiveSession();
  void ValidateSessionIndex(size_t index) const;
  std::string GenerateUniqueSessionName(const std::string& base_name) const;
  void ShowSessionLimitWarning();
  void ShowSessionOperationResult(const std::string& operation, bool success);
  
  // UI helper methods
  void DrawSessionTab(size_t index, bool is_active);
  void DrawSessionContextMenu(size_t index);
  void DrawSessionBadge(size_t index);
  ImVec4 GetSessionColor(size_t index) const;
  std::string GetSessionIcon(size_t index) const;
  
  // Session validation helpers
  bool IsSessionEmpty(size_t index) const;
  bool IsSessionClosed(size_t index) const;
  bool IsSessionModified(size_t index) const;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_SESSION_COORDINATOR_H_
