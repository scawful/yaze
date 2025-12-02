#ifndef YAZE_APP_PLATFORM_WASM_COLLABORATION_H_
#define YAZE_APP_PLATFORM_WASM_COLLABORATION_H_

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/val.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/net/wasm/emscripten_websocket.h"
#include "rom/rom.h"

namespace yaze {
namespace app {
namespace platform {

/**
 * @brief Real-time collaboration manager for WASM builds
 *
 * Enables multiple users to edit ROMs together in the browser.
 * Uses WebSocket connection for real-time synchronization.
 */
class WasmCollaboration {
 public:
  /**
   * @brief User information for collaboration session
   */
  struct User {
    std::string id;
    std::string name;
    std::string color;  // Hex color for cursor/highlights
    bool is_active = true;
    double last_activity = 0;  // Timestamp
  };

  /**
   * @brief Cursor position information
   */
  struct CursorInfo {
    std::string user_id;
    std::string editor_type;  // "overworld", "dungeon", etc.
    int x = 0;
    int y = 0;
    int map_id = -1;  // For context (which map/room)
  };

  /**
   * @brief ROM change event for synchronization
   */
  struct ChangeEvent {
    uint32_t offset;
    std::vector<uint8_t> old_data;
    std::vector<uint8_t> new_data;
    std::string user_id;
    double timestamp;
  };

  // Connection state enum
  enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting
  };

  // Callbacks for UI updates
  using UserListCallback = std::function<void(const std::vector<User>&)>;
  using ChangeCallback = std::function<void(const ChangeEvent&)>;
  using CursorCallback = std::function<void(const CursorInfo&)>;
  using StatusCallback = std::function<void(bool connected, const std::string& message)>;
  using ConnectionStateCallback = std::function<void(ConnectionState state, const std::string& message)>;

  WasmCollaboration();
  ~WasmCollaboration();

  /**
   * @brief Set the WebSocket server URL
   * @param url Full WebSocket URL (e.g., "wss://your-server.com/ws")
   * @return Status indicating success or validation failure
   *
   * For GitHub Pages deployment, you'll need a separate WebSocket server.
   * Options include:
   * - Cloudflare Workers with Durable Objects
   * - Deno Deploy
   * - Railway, Render, or other PaaS providers
   * - Self-hosted server (e.g., yaze-server on port 8765)
   *
   * URL must start with "ws://" or "wss://" to be valid.
   */
  absl::Status SetWebSocketUrl(const std::string& url) {
    if (url.empty()) {
      websocket_url_.clear();
      return absl::OkStatus();
    }
    if (url.find("ws://") != 0 && url.find("wss://") != 0) {
      return absl::InvalidArgumentError(
          "WebSocket URL must start with ws:// or wss://");
    }
    // Basic URL structure validation
    if (url.length() < 8) {  // Minimum: "ws://x" or "wss://x"
      return absl::InvalidArgumentError("WebSocket URL is too short");
    }
    websocket_url_ = url;
    return absl::OkStatus();
  }

  /**
   * @brief Get the current WebSocket server URL
   * @return Current URL or empty if not configured
   */
  std::string GetWebSocketUrl() const { return websocket_url_; }

  /**
   * @brief Initialize WebSocket URL from JavaScript configuration
   *
   * Looks for window.YAZE_CONFIG.collaborationServerUrl in the browser.
   * This allows deployment-specific configuration without recompiling.
   */
  void InitializeFromConfig();

  /**
   * @brief Check if collaboration is configured and available
   * @return true if WebSocket URL is set and valid
   */
  bool IsConfigured() const { return !websocket_url_.empty(); }

  /**
   * @brief Create a new collaboration session
   * @param session_name Name for the session
   * @param username User's display name
   * @return Room code for others to join
   */
  absl::StatusOr<std::string> CreateSession(const std::string& session_name,
                                           const std::string& username,
                                           const std::string& password = "");

  /**
   * @brief Join an existing collaboration session
   * @param room_code 6-character room code
   * @param username User's display name
   * @return Status of connection attempt
   */
  absl::Status JoinSession(const std::string& room_code,
                          const std::string& username,
                          const std::string& password = "");

  /**
   * @brief Leave current collaboration session
   */
  absl::Status LeaveSession();

  /**
   * @brief Broadcast a ROM change to all peers
   * @param offset ROM offset that changed
   * @param old_data Original data
   * @param new_data New data
   */
  absl::Status BroadcastChange(uint32_t offset,
                               const std::vector<uint8_t>& old_data,
                               const std::vector<uint8_t>& new_data);

  /**
   * @brief Send cursor position update
   * @param editor_type Current editor ("overworld", "dungeon", etc.)
   * @param x X position in editor
   * @param y Y position in editor
   * @param map_id Optional map/room ID for context
   */
  absl::Status SendCursorPosition(const std::string& editor_type,
                                  int x, int y, int map_id = -1);

  /**
   * @brief Set ROM reference for applying changes
   * @param rom Pointer to the ROM being edited
   */
  void SetRom(Rom* rom) { rom_ = rom; }

  /**
   * @brief Register callback for ROM changes from peers
   * @param callback Function to call when changes arrive
   */
  void SetChangeCallback(ChangeCallback callback) {
    change_callback_ = callback;
  }

  /**
   * @brief Register callback for user list updates
   * @param callback Function to call when user list changes
   */
  void SetUserListCallback(UserListCallback callback) {
    user_list_callback_ = callback;
  }

  /**
   * @brief Register callback for cursor position updates
   * @param callback Function to call when cursor positions update
   */
  void SetCursorCallback(CursorCallback callback) {
    cursor_callback_ = callback;
  }

  /**
   * @brief Register callback for connection status changes
   * @param callback Function to call on status changes
   */
  void SetStatusCallback(StatusCallback callback) {
    status_callback_ = callback;
  }

  /**
   * @brief Register callback for connection state changes
   * @param callback Function to call on connection state changes
   */
  void SetConnectionStateCallback(ConnectionStateCallback callback) {
    connection_state_callback_ = callback;
  }

  /**
   * @brief Get current connection state
   * @return Current connection state
   */
  ConnectionState GetConnectionState() const {
    return connection_state_;
  }

  /**
   * @brief Get list of connected users
   * @return Vector of active users
   */
  std::vector<User> GetConnectedUsers() const;

  /**
   * @brief Check if currently connected to a session
   * @return true if connected
   */
  bool IsConnected() const;

  /**
   * @brief Whether we're currently applying a remote change (used to avoid rebroadcast)
   */
  bool IsApplyingRemoteChange() const { return applying_remote_change_; }

  /**
   * @brief Get current room code
   * @return Room code or empty string if not connected
   */
  std::string GetRoomCode() const { return room_code_; }

  /**
   * @brief Get session name
   * @return Session name or empty string if not connected
   */
  std::string GetSessionName() const { return session_name_; }

  /**
   * @brief Get current user id (stable per session)
   */
  std::string GetUserId() const { return user_id_; }

  /**
   * @brief Enable/disable automatic conflict resolution
   * @param enable true to enable auto-resolution
   */
  void SetAutoResolveConflicts(bool enable) {
    auto_resolve_conflicts_ = enable;
  }

  /**
   * @brief Process pending changes queue
   * Called periodically to apply remote changes
   */
  void ProcessPendingChanges();

 private:
  // WebSocket message handlers
  void HandleMessage(const std::string& message);
  void HandleCreateResponse(const emscripten::val& data);
  void HandleJoinResponse(const emscripten::val& data);
  void HandleUserList(const emscripten::val& data);
  void HandleChange(const emscripten::val& data);
  void HandleCursor(const emscripten::val& data);
  void HandleError(const emscripten::val& data);

  // Utility methods
  std::string GenerateUserId();
  std::string GenerateUserColor();
  void UpdateUserActivity(const std::string& user_id);
  void CheckUserTimeouts();
  bool IsChangeValid(const ChangeEvent& change);
  void ApplyRemoteChange(const ChangeEvent& change);

  // Reconnection management
  void InitiateReconnection();
  void AttemptReconnection();
  void ResetReconnectionState();
  void UpdateConnectionState(ConnectionState new_state, const std::string& message);
  void QueueMessageWhileDisconnected(const std::string& message);

  // Connection management
  std::unique_ptr<net::EmscriptenWebSocket> websocket_;
  bool is_connected_ = false;
  ConnectionState connection_state_ = ConnectionState::Disconnected;
  std::string websocket_url_;  // Set via SetWebSocketUrl() or environment

  // Reconnection state
  int reconnection_attempts_ = 0;
  int max_reconnection_attempts_ = 10;
  double reconnection_delay_seconds_ = 1.0;  // Initial delay
  double max_reconnection_delay_ = 30.0;  // Max delay between attempts
  bool should_reconnect_ = false;
  std::string stored_password_;  // Store password for reconnection

  // Session state
  std::string room_code_;
  std::string session_name_;
  std::string user_id_;
  std::string username_;
  std::string user_color_;

  // Connected users
  std::map<std::string, User> users_;
  mutable std::mutex users_mutex_;

  // Remote cursors
  std::map<std::string, CursorInfo> cursors_;
  mutable std::mutex cursors_mutex_;

  // Change queue for conflict resolution
  std::vector<ChangeEvent> pending_changes_;
  mutable std::mutex changes_mutex_;

  // Message queue for disconnected state
  std::vector<std::string> queued_messages_;
  mutable std::mutex message_queue_mutex_;
  size_t max_queued_messages_ = 100;  // Limit queue size

  // Configuration
  bool auto_resolve_conflicts_ = true;

  // Callbacks
  UserListCallback user_list_callback_;
  ChangeCallback change_callback_;
  CursorCallback cursor_callback_;
  StatusCallback status_callback_;
  ConnectionStateCallback connection_state_callback_;

  // ROM reference for applying changes
  Rom* rom_ = nullptr;

  // Rate limiting
  double last_cursor_send_ = 0;

  // Guard to prevent echoing remote writes back to the server
  bool applying_remote_change_ = false;
};

// Singleton accessor used by JS bindings and the WASM main loop
WasmCollaboration& GetWasmCollaborationInstance();

}  // namespace platform
}  // namespace app
}  // namespace yaze

#else  // !__EMSCRIPTEN__

// Stub for non-WASM builds
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include <string>
#include <vector>

namespace yaze {
namespace app {
namespace platform {

class WasmCollaboration {
 public:
  struct User {
    std::string id;
    std::string name;
    std::string color;
    bool is_active = true;
  };

  struct CursorInfo {
    std::string user_id;
    std::string editor_type;
    int x = 0;
    int y = 0;
    int map_id = -1;
  };

  struct ChangeEvent {
    uint32_t offset;
    std::vector<uint8_t> old_data;
    std::vector<uint8_t> new_data;
    std::string user_id;
    double timestamp;
  };

  absl::StatusOr<std::string> CreateSession(const std::string&,
                                           const std::string&) {
    return absl::UnimplementedError("Collaboration requires WASM build");
  }

  absl::Status JoinSession(const std::string&, const std::string&) {
    return absl::UnimplementedError("Collaboration requires WASM build");
  }

  absl::Status LeaveSession() {
    return absl::UnimplementedError("Collaboration requires WASM build");
  }

  absl::Status BroadcastChange(uint32_t, const std::vector<uint8_t>&,
                               const std::vector<uint8_t>&) {
    return absl::UnimplementedError("Collaboration requires WASM build");
  }

  std::vector<User> GetConnectedUsers() const { return {}; }
  bool IsConnected() const { return false; }
  std::string GetRoomCode() const { return ""; }
  std::string GetSessionName() const { return ""; }
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_COLLABORATION_H_
