#ifndef YAZE_APP_EDITOR_SYSTEM_NETWORK_COLLABORATION_COORDINATOR_H_
#define YAZE_APP_EDITOR_SYSTEM_NETWORK_COLLABORATION_COORDINATOR_H_

#ifdef YAZE_WITH_GRPC  // Reuse gRPC build flag for network features

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"

namespace yaze {
namespace editor {

// Forward declarations to avoid including httplib in header
namespace detail {
class WebSocketClient;
}

// Coordinates network-based collaboration via WebSocket connections
class NetworkCollaborationCoordinator {
 public:
  struct SessionInfo {
    std::string session_id;
    std::string session_code;
    std::string session_name;
    std::vector<std::string> participants;
  };

  struct ChatMessage {
    std::string sender;
    std::string message;
    int64_t timestamp;
  };

  // Callbacks for handling incoming events
  using MessageCallback = std::function<void(const ChatMessage&)>;
  using ParticipantCallback = std::function<void(const std::vector<std::string>&)>;
  using ErrorCallback = std::function<void(const std::string&)>;

  explicit NetworkCollaborationCoordinator(const std::string& server_url);
  ~NetworkCollaborationCoordinator();

  // Session management
  absl::StatusOr<SessionInfo> HostSession(const std::string& session_name,
                                          const std::string& username);
  absl::StatusOr<SessionInfo> JoinSession(const std::string& session_code,
                                          const std::string& username);
  absl::Status LeaveSession();

  // Send chat message to current session
  absl::Status SendMessage(const std::string& sender, const std::string& message);

  // Connection status
  bool IsConnected() const;
  bool InSession() const { return in_session_; }
  const std::string& session_code() const { return session_code_; }
  const std::string& session_name() const { return session_name_; }

  // Event callbacks
  void SetMessageCallback(MessageCallback callback);
  void SetParticipantCallback(ParticipantCallback callback);
  void SetErrorCallback(ErrorCallback callback);

 private:
  void ConnectWebSocket();
  void DisconnectWebSocket();
  void SendWebSocketMessage(const std::string& type, const std::string& payload_json);
  void HandleWebSocketMessage(const std::string& message);
  void WebSocketReceiveLoop();

  std::string server_url_;
  std::string username_;
  std::string session_id_;
  std::string session_code_;
  std::string session_name_;
  bool in_session_ = false;
  
  std::unique_ptr<detail::WebSocketClient> ws_client_;
  std::atomic<bool> connected_{false};
  std::atomic<bool> should_stop_{false};
  std::unique_ptr<std::thread> receive_thread_;
  
  mutable absl::Mutex mutex_;
  MessageCallback message_callback_ ABSL_GUARDED_BY(mutex_);
  ParticipantCallback participant_callback_ ABSL_GUARDED_BY(mutex_);
  ErrorCallback error_callback_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_EDITOR_SYSTEM_NETWORK_COLLABORATION_COORDINATOR_H_
