#ifndef YAZE_APP_EDITOR_AGENT_NETWORK_COLLABORATION_COORDINATOR_H_
#define YAZE_APP_EDITOR_AGENT_NETWORK_COLLABORATION_COORDINATOR_H_

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
    std::string message_type;  // "chat", "system", "ai"
    std::string metadata;      // JSON metadata
  };

  struct RomSync {
    std::string sync_id;
    std::string sender;
    std::string diff_data;  // Base64 encoded
    std::string rom_hash;
    int64_t timestamp;
  };

  struct Snapshot {
    std::string snapshot_id;
    std::string sender;
    std::string snapshot_data;  // Base64 encoded
    std::string snapshot_type;
    int64_t timestamp;
  };

  struct Proposal {
    std::string proposal_id;
    std::string sender;
    std::string proposal_data;  // JSON data
    std::string status;         // "pending", "accepted", "rejected"
    int64_t timestamp;
  };

  struct AIResponse {
    std::string query_id;
    std::string username;
    std::string query;
    std::string response;
    int64_t timestamp;
  };

  // Callbacks for handling incoming events
  using MessageCallback = std::function<void(const ChatMessage&)>;
  using ParticipantCallback = std::function<void(const std::vector<std::string>&)>;
  using ErrorCallback = std::function<void(const std::string&)>;
  using RomSyncCallback = std::function<void(const RomSync&)>;
  using SnapshotCallback = std::function<void(const Snapshot&)>;
  using ProposalCallback = std::function<void(const Proposal&)>;
  using ProposalUpdateCallback = std::function<void(const std::string&, const std::string&)>;
  using AIResponseCallback = std::function<void(const AIResponse&)>;

  explicit NetworkCollaborationCoordinator(const std::string& server_url);
  ~NetworkCollaborationCoordinator();

  // Session management
  absl::StatusOr<SessionInfo> HostSession(const std::string& session_name,
                                          const std::string& username,
                                          const std::string& rom_hash = "",
                                          bool ai_enabled = true);
  absl::StatusOr<SessionInfo> JoinSession(const std::string& session_code,
                                          const std::string& username);
  absl::Status LeaveSession();

  // Communication methods
  absl::Status SendChatMessage(const std::string& sender, 
                          const std::string& message,
                          const std::string& message_type = "chat",
                          const std::string& metadata = "");
  
  // Advanced features
  absl::Status SendRomSync(const std::string& sender,
                          const std::string& diff_data,
                          const std::string& rom_hash);
  
  absl::Status SendSnapshot(const std::string& sender,
                           const std::string& snapshot_data,
                           const std::string& snapshot_type);
  
  absl::Status SendProposal(const std::string& sender,
                           const std::string& proposal_data_json);
  
  absl::Status UpdateProposal(const std::string& proposal_id,
                             const std::string& status);
  
  absl::Status SendAIQuery(const std::string& username,
                          const std::string& query);

  // Connection status
  bool IsConnected() const;
  bool InSession() const { return in_session_; }
  const std::string& session_code() const { return session_code_; }
  const std::string& session_name() const { return session_name_; }

  // Event callbacks
  void SetMessageCallback(MessageCallback callback);
  void SetParticipantCallback(ParticipantCallback callback);
  void SetErrorCallback(ErrorCallback callback);
  void SetRomSyncCallback(RomSyncCallback callback);
  void SetSnapshotCallback(SnapshotCallback callback);
  void SetProposalCallback(ProposalCallback callback);
  void SetProposalUpdateCallback(ProposalUpdateCallback callback);
  void SetAIResponseCallback(AIResponseCallback callback);

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
  RomSyncCallback rom_sync_callback_ ABSL_GUARDED_BY(mutex_);
  SnapshotCallback snapshot_callback_ ABSL_GUARDED_BY(mutex_);
  ProposalCallback proposal_callback_ ABSL_GUARDED_BY(mutex_);
  ProposalUpdateCallback proposal_update_callback_ ABSL_GUARDED_BY(mutex_);
  AIResponseCallback ai_response_callback_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_EDITOR_AGENT_NETWORK_COLLABORATION_COORDINATOR_H_
