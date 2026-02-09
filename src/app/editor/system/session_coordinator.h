#ifndef YAZE_APP_EDITOR_SYSTEM_SESSION_COORDINATOR_H_
#define YAZE_APP_EDITOR_SYSTEM_SESSION_COORDINATOR_H_

#include <deque>
#include <string>
#include <vector>
#include <memory>

#include "absl/status/status.h"
#include "app/editor/core/event_bus.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/ui/toast_manager.h"
#include "app/editor/session_types.h"
#include "rom/rom.h"
#include "imgui/imgui.h"

// Forward declarations
namespace yaze {
class Rom;
namespace editor {
class EditorManager;
class EditorRegistry;
class EditorSet;
class PanelManager;
}  // namespace editor
}  // namespace yaze

namespace yaze {
namespace editor {

// Forward declarations
class EditorSet;
class ToastManager;

/**
 * @class SessionObserver
 * @brief Observer interface for session state changes
 *
 * @deprecated Use EventBus subscriptions instead. Subscribe to:
 * - SessionSwitchedEvent for session changes
 * - SessionCreatedEvent for new sessions
 * - SessionClosedEvent for closed sessions
 * - RomLoadedEvent for ROM loads
 *
 * Example migration:
 * @code
 * // Old pattern:
 * class MyClass : public SessionObserver {
 *   void OnSessionSwitched(size_t idx, RomSession* s) override { ... }
 * };
 *
 * // New pattern:
 * event_bus->Subscribe<SessionSwitchedEvent>([](const auto& e) {
 *   // Handle session switch using e.new_index, e.session
 * });
 * @endcode
 *
 * This interface will be removed in a future release.
 */
class [[deprecated("Use EventBus subscriptions instead - see class documentation")]]
SessionObserver {
 public:
  virtual ~SessionObserver() = default;

  /// Called when the active session changes
  virtual void OnSessionSwitched(size_t new_index, RomSession* session) = 0;

  /// Called when a new session is created
  virtual void OnSessionCreated(size_t index, RomSession* session) = 0;

  /// Called when a session is closed
  virtual void OnSessionClosed(size_t index) = 0;

  /// Called when a ROM is loaded into a session
  virtual void OnSessionRomLoaded(size_t index, RomSession* session) {}
};

/**
 * @class SessionCoordinator
 * @brief High-level orchestrator for multi-session UI
 *
 * Manages session list UI, coordinates card visibility across sessions,
 * handles session activation/deactivation, and provides session-aware editor
 * queries.
 *
 * This class lives in the ui/ layer and can depend on both system and gui
 * components.
 */
class SessionCoordinator {
 public:
  explicit SessionCoordinator(PanelManager* panel_manager,
                              ToastManager* toast_manager,
                              UserSettings* user_settings);
  ~SessionCoordinator() = default;

  void SetEditorManager(EditorManager* manager) { editor_manager_ = manager; }
  void SetEditorRegistry(EditorRegistry* registry) {
    editor_registry_ = registry;
  }

  /// Set the EventBus for publishing session lifecycle events.
  /// When set, session events will be published alongside observer notifications.
  void SetEventBus(EventBus* bus) { event_bus_ = bus; }

  // Observer management
  void AddObserver(SessionObserver* observer);
  void RemoveObserver(SessionObserver* observer);

  // Session lifecycle management
  void CreateNewSession();
  void DuplicateCurrentSession();
  void CloseCurrentSession();
  void CloseSession(size_t index);
  void RemoveSession(size_t index);
  void SwitchToSession(size_t index);
  void UpdateSessions();

  // Session activation and queries
  void ActivateSession(size_t index);
  size_t GetActiveSessionIndex() const;
  void* GetActiveSession() const;
  RomSession* GetActiveRomSession() const;
  Rom* GetCurrentRom() const;
  zelda3::GameData* GetCurrentGameData() const;
  EditorSet* GetCurrentEditorSet() const;
  void* GetSession(size_t index) const;
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
  std::string GenerateUniqueEditorTitle(const std::string& editor_name,
                                        size_t session_index) const;

  // Session state management
  void SetActiveSessionIndex(size_t index);
  void UpdateSessionCount();

  // Panel coordination across sessions
  void ShowAllPanelsInActiveSession();
  void HideAllPanelsInActiveSession();
  void ShowPanelsInCategory(const std::string& category);
  void HidePanelsInCategory(const std::string& category);

  // Session validation
  bool IsValidSessionIndex(size_t index) const;
  bool IsSessionActive(size_t index) const;
  bool IsSessionLoaded(size_t index) const;

  // Session statistics
  size_t GetTotalSessionCount() const;
  size_t GetLoadedSessionCount() const;
  size_t GetEmptySessionCount() const;

  // Session operations with error handling
  absl::Status LoadRomIntoSession(const std::string& filename,
                                  size_t session_index = SIZE_MAX);
  absl::Status SaveActiveSession(const std::string& filename = "");
  absl::Status SaveSessionAs(size_t session_index, const std::string& filename);
  absl::StatusOr<RomSession*> CreateSessionFromRom(Rom&& rom,
                                                   const std::string& filepath);

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
  void ToggleSessionSwitcher() {
    show_session_switcher_ = !show_session_switcher_;
  }
  bool IsSessionSwitcherVisible() const { return show_session_switcher_; }

  void ShowSessionManager() { show_session_manager_ = true; }
  void HideSessionManager() { show_session_manager_ = false; }
  void ToggleSessionManager() {
    show_session_manager_ = !show_session_manager_;
  }
  bool IsSessionManagerVisible() const { return show_session_manager_; }

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

 private:
  // Observer notification helpers
  void NotifySessionSwitched(size_t index, RomSession* session);
  void NotifySessionCreated(size_t index, RomSession* session);
  void NotifySessionClosed(size_t index);
  void NotifySessionRomLoaded(size_t index, RomSession* session);

  // Core dependencies
  EditorManager* editor_manager_ = nullptr;
  EditorRegistry* editor_registry_ = nullptr;
  EventBus* event_bus_ = nullptr;  // For publishing session lifecycle events
  std::vector<std::unique_ptr<RomSession>> sessions_;
  std::vector<SessionObserver*> observers_;
  PanelManager* panel_manager_;
  ToastManager* toast_manager_;
  UserSettings* user_settings_;

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
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_SESSION_COORDINATOR_H_
