#ifndef YAZE_APP_NET_WEBSOCKET_CLIENT_H_
#define YAZE_APP_NET_WEBSOCKET_CLIENT_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze {

namespace net {

/**
 * @enum ConnectionState
 * @brief WebSocket connection states
 */
enum class ConnectionState {
  kDisconnected,
  kConnecting,
  kConnected,
  kReconnecting,
  kError
};

/**
 * @struct SessionInfo
 * @brief Information about the current collaboration session
 */
struct SessionInfo {
  std::string session_id;
  std::string session_code;
  std::string session_name;
  std::string host;
  std::vector<std::string> participants;
  std::string rom_hash;
  bool ai_enabled;
};

/**
 * @class WebSocketClient
 * @brief WebSocket client for connecting to yaze-server
 * 
 * Provides:
 * - Connection management with auto-reconnect
 * - Session hosting and joining
 * - Message sending/receiving
 * - Event callbacks for different message types
 */
class WebSocketClient {
 public:
  // Message type callbacks
  using MessageCallback = std::function<void(const nlohmann::json&)>;
  using ErrorCallback = std::function<void(const std::string&)>;
  using StateCallback = std::function<void(ConnectionState)>;
  
  WebSocketClient();
  ~WebSocketClient();
  
  /**
   * Connect to yaze-server
   * @param host Server hostname/IP
   * @param port Server port (default: 8765)
   */
  absl::Status Connect(const std::string& host, int port = 8765);
  
  /**
   * Disconnect from server
   */
  void Disconnect();
  
  /**
   * Host a new collaboration session
   */
  absl::StatusOr<SessionInfo> HostSession(
      const std::string& session_name,
      const std::string& username,
      const std::string& rom_hash,
      bool ai_enabled = true);
  
  /**
   * Join an existing session
   */
  absl::StatusOr<SessionInfo> JoinSession(
      const std::string& session_code,
      const std::string& username);
  
  /**
   * Leave current session
   */
  absl::Status LeaveSession();
  
  /**
   * Send chat message
   */
  absl::Status SendChatMessage(
      const std::string& message,
      const std::string& sender);
  
  /**
   * Send ROM sync
   */
  absl::Status SendRomSync(
      const std::string& diff_data,
      const std::string& rom_hash,
      const std::string& sender);
  
  /**
   * Share snapshot
   */
  absl::Status ShareSnapshot(
      const std::string& snapshot_data,
      const std::string& snapshot_type,
      const std::string& sender);
  
  /**
   * Share proposal for approval
   */
  absl::Status ShareProposal(
      const nlohmann::json& proposal_data,
      const std::string& sender);
  
  /**
   * Vote on proposal (approve/reject)
   */
  absl::Status VoteOnProposal(
      const std::string& proposal_id,
      bool approved,
      const std::string& username);
  
  /**
   * Update proposal status
   */
  absl::Status UpdateProposalStatus(
      const std::string& proposal_id,
      const std::string& status);
  
  /**
   * Send AI query
   */
  absl::Status SendAIQuery(
      const std::string& query,
      const std::string& username);
  
  /**
   * Register callback for specific message type
   */
  void OnMessage(const std::string& type, MessageCallback callback);
  
  /**
   * Register callback for errors
   */
  void OnError(ErrorCallback callback);
  
  /**
   * Register callback for connection state changes
   */
  void OnStateChange(StateCallback callback);
  
  /**
   * Get current connection state
   */
  ConnectionState GetState() const { return state_; }
  
  /**
   * Get current session info (if in a session)
   */
  absl::StatusOr<SessionInfo> GetSessionInfo() const;
  
  /**
   * Check if connected
   */
  bool IsConnected() const { return state_ == ConnectionState::kConnected; }
  
  /**
   * Check if in a session
   */
  bool InSession() const { return !current_session_.session_id.empty(); }
  
 private:
  // Implementation details (using native WebSocket or library)
  class Impl;
  std::unique_ptr<Impl> impl_;
  
  ConnectionState state_;
  SessionInfo current_session_;
  
  // Callbacks
  std::map<std::string, std::vector<MessageCallback>> message_callbacks_;
  std::vector<ErrorCallback> error_callbacks_;
  std::vector<StateCallback> state_callbacks_;
  
  // Internal message handling
  void HandleMessage(const std::string& message);
  void HandleError(const std::string& error);
  void SetState(ConnectionState state);
  
  // Send raw message
  absl::Status SendRaw(const nlohmann::json& message);
};

}  // namespace net

}  // namespace yaze

#endif  // YAZE_APP_NET_WEBSOCKET_CLIENT_H_
