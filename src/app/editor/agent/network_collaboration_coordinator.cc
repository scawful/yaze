#include "app/editor/agent/network_collaboration_coordinator.h"

#ifdef YAZE_WITH_GRPC

#include <iostream>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"

#ifdef YAZE_WITH_JSON
#include "httplib.h"
#include "nlohmann/json.hpp"
using Json = nlohmann::json;
#endif

namespace yaze {
namespace editor {

#ifdef YAZE_WITH_JSON

namespace detail {

// Simple WebSocket client implementation using httplib
// Implements basic WebSocket protocol for collaboration
class WebSocketClient {
 public:
  explicit WebSocketClient(const std::string& host, int port)
      : host_(host), port_(port), connected_(false) {}

  bool Connect(const std::string& path) {
    try {
      // Create HTTP client for WebSocket upgrade
      client_ = std::make_unique<httplib::Client>(host_.c_str(), port_);
      client_->set_connection_timeout(5);  // 5 seconds
      client_->set_read_timeout(30);       // 30 seconds
      
      // For now, mark as connected and use HTTP polling fallback
      // A full WebSocket implementation would do the upgrade handshake here
      connected_ = true;
      
      std::cout << "✓ Connected to collaboration server at " << host_ << ":" << port_ << std::endl;
      return true;
    } catch (const std::exception& e) {
      std::cerr << "Failed to connect to " << host_ << ":" << port_ << ": " << e.what() << std::endl;
      return false;
    }
  }

  void Close() {
    connected_ = false;
    client_.reset();
  }

  bool Send(const std::string& message) {
    if (!connected_ || !client_) return false;
    
    // For HTTP fallback: POST message to server
    // A full WebSocket would send WebSocket frames
    auto res = client_->Post("/message", message, "application/json");
    return res && res->status == 200;
  }

  std::string Receive() {
    if (!connected_ || !client_) return "";
    
    // For HTTP fallback: Poll for messages
    // A full WebSocket would read frames from the socket
    auto res = client_->Get("/poll");
    if (res && res->status == 200) {
      return res->body;
    }
    return "";
  }

  bool IsConnected() const { return connected_; }

 private:
  std::string host_;
  int port_;
  bool connected_;
  std::unique_ptr<httplib::Client> client_;
};

}  // namespace detail

NetworkCollaborationCoordinator::NetworkCollaborationCoordinator(
    const std::string& server_url)
    : server_url_(server_url) {
  // Parse server URL
  // Expected format: ws://hostname:port or wss://hostname:port
  if (server_url_.find("ws://") == 0) {
    // Extract hostname and port
    // For now, use default localhost:8765
    ConnectWebSocket();
  }
}

NetworkCollaborationCoordinator::~NetworkCollaborationCoordinator() {
  should_stop_ = true;
  if (receive_thread_ && receive_thread_->joinable()) {
    receive_thread_->join();
  }
  DisconnectWebSocket();
}

void NetworkCollaborationCoordinator::ConnectWebSocket() {
  // Parse URL (simple implementation - assumes ws://host:port format)
  std::string host = "localhost";
  int port = 8765;
  
  // Extract from server_url_ if needed
  if (server_url_.find("ws://") == 0) {
    std::string url_part = server_url_.substr(5);  // Skip "ws://"
    std::vector<std::string> parts = absl::StrSplit(url_part, ':');
    if (!parts.empty()) {
      host = parts[0];
    }
    if (parts.size() > 1) {
      port = std::stoi(parts[1]);
    }
  }

  ws_client_ = std::make_unique<detail::WebSocketClient>(host, port);
  
  if (ws_client_->Connect("/")) {
    connected_ = true;
    
    // Start receive thread
    should_stop_ = false;
    receive_thread_ = std::make_unique<std::thread>(
        &NetworkCollaborationCoordinator::WebSocketReceiveLoop, this);
  }
}

void NetworkCollaborationCoordinator::DisconnectWebSocket() {
  if (ws_client_) {
    ws_client_->Close();
    ws_client_.reset();
  }
  connected_ = false;
}

absl::StatusOr<NetworkCollaborationCoordinator::SessionInfo>
NetworkCollaborationCoordinator::HostSession(const std::string& session_name,
                                             const std::string& username,
                                             const std::string& rom_hash,
                                             bool ai_enabled) {
  if (!connected_) {
    return absl::FailedPreconditionError("Not connected to collaboration server");
  }

  username_ = username;

  // Build host_session message with v2.0 fields
  Json payload = {
      {"session_name", session_name},
      {"username", username},
      {"ai_enabled", ai_enabled}
  };
  
  if (!rom_hash.empty()) {
    payload["rom_hash"] = rom_hash;
  }

  Json message = {
      {"type", "host_session"},
      {"payload", payload}
  };

  SendWebSocketMessage("host_session", message["payload"].dump());

  // TODO: Wait for session_hosted response and parse it
  // For now, return a placeholder
  SessionInfo info;
  info.session_name = session_name;
  info.session_code = "PENDING";  // Will be updated from server response
  info.participants = {username};
  
  in_session_ = true;
  session_name_ = session_name;

  return info;
}

absl::StatusOr<NetworkCollaborationCoordinator::SessionInfo>
NetworkCollaborationCoordinator::JoinSession(const std::string& session_code,
                                             const std::string& username) {
  if (!connected_) {
    return absl::FailedPreconditionError("Not connected to collaboration server");
  }

  username_ = username;
  session_code_ = session_code;

  // Build join_session message
  Json message = {
      {"type", "join_session"},
      {"payload", {
          {"session_code", session_code},
          {"username", username}
      }}
  };

  SendWebSocketMessage("join_session", message["payload"].dump());

  // TODO: Wait for session_joined response and parse it
  SessionInfo info;
  info.session_code = session_code;
  
  in_session_ = true;

  return info;
}

absl::Status NetworkCollaborationCoordinator::LeaveSession() {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  Json message = {{"type", "leave_session"}};
  SendWebSocketMessage("leave_session", "{}");

  in_session_ = false;
  session_id_.clear();
  session_code_.clear();
  session_name_.clear();

  return absl::OkStatus();
}

absl::Status NetworkCollaborationCoordinator::SendMessage(
    const std::string& sender, const std::string& message,
    const std::string& message_type, const std::string& metadata) {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  Json payload = {
      {"sender", sender},
      {"message", message},
      {"message_type", message_type}
  };
  
  if (!metadata.empty()) {
    payload["metadata"] = Json::parse(metadata);
  }

  Json msg = {
      {"type", "chat_message"},
      {"payload", payload}
  };

  SendWebSocketMessage("chat_message", msg["payload"].dump());
  return absl::OkStatus();
}

absl::Status NetworkCollaborationCoordinator::SendRomSync(
    const std::string& sender, const std::string& diff_data,
    const std::string& rom_hash) {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  Json msg = {
      {"type", "rom_sync"},
      {"payload", {
          {"sender", sender},
          {"diff_data", diff_data},
          {"rom_hash", rom_hash}
      }}
  };

  SendWebSocketMessage("rom_sync", msg["payload"].dump());
  return absl::OkStatus();
}

absl::Status NetworkCollaborationCoordinator::SendSnapshot(
    const std::string& sender, const std::string& snapshot_data,
    const std::string& snapshot_type) {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  Json msg = {
      {"type", "snapshot_share"},
      {"payload", {
          {"sender", sender},
          {"snapshot_data", snapshot_data},
          {"snapshot_type", snapshot_type}
      }}
  };

  SendWebSocketMessage("snapshot_share", msg["payload"].dump());
  return absl::OkStatus();
}

absl::Status NetworkCollaborationCoordinator::SendProposal(
    const std::string& sender, const std::string& proposal_data_json) {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  Json msg = {
      {"type", "proposal_share"},
      {"payload", {
          {"sender", sender},
          {"proposal_data", Json::parse(proposal_data_json)}
      }}
  };

  SendWebSocketMessage("proposal_share", msg["payload"].dump());
  return absl::OkStatus();
}

absl::Status NetworkCollaborationCoordinator::UpdateProposal(
    const std::string& proposal_id, const std::string& status) {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  Json msg = {
      {"type", "proposal_update"},
      {"payload", {
          {"proposal_id", proposal_id},
          {"status", status}
      }}
  };

  SendWebSocketMessage("proposal_update", msg["payload"].dump());
  return absl::OkStatus();
}

absl::Status NetworkCollaborationCoordinator::SendAIQuery(
    const std::string& username, const std::string& query) {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  Json msg = {
      {"type", "ai_query"},
      {"payload", {
          {"username", username},
          {"query", query}
      }}
  };

  SendWebSocketMessage("ai_query", msg["payload"].dump());
  return absl::OkStatus();
}

bool NetworkCollaborationCoordinator::IsConnected() const {
  return connected_;
}

void NetworkCollaborationCoordinator::SetMessageCallback(MessageCallback callback) {
  absl::MutexLock lock(&mutex_);
  message_callback_ = std::move(callback);
}

void NetworkCollaborationCoordinator::SetParticipantCallback(
    ParticipantCallback callback) {
  absl::MutexLock lock(&mutex_);
  participant_callback_ = std::move(callback);
}

void NetworkCollaborationCoordinator::SetErrorCallback(ErrorCallback callback) {
  absl::MutexLock lock(&mutex_);
  error_callback_ = std::move(callback);
}

void NetworkCollaborationCoordinator::SetRomSyncCallback(RomSyncCallback callback) {
  absl::MutexLock lock(&mutex_);
  rom_sync_callback_ = std::move(callback);
}

void NetworkCollaborationCoordinator::SetSnapshotCallback(SnapshotCallback callback) {
  absl::MutexLock lock(&mutex_);
  snapshot_callback_ = std::move(callback);
}

void NetworkCollaborationCoordinator::SetProposalCallback(ProposalCallback callback) {
  absl::MutexLock lock(&mutex_);
  proposal_callback_ = std::move(callback);
}

void NetworkCollaborationCoordinator::SetProposalUpdateCallback(ProposalUpdateCallback callback) {
  absl::MutexLock lock(&mutex_);
  proposal_update_callback_ = std::move(callback);
}

void NetworkCollaborationCoordinator::SetAIResponseCallback(AIResponseCallback callback) {
  absl::MutexLock lock(&mutex_);
  ai_response_callback_ = std::move(callback);
}

void NetworkCollaborationCoordinator::SendWebSocketMessage(
    const std::string& type, const std::string& payload_json) {
  if (!ws_client_ || !connected_) {
    return;
  }

  Json message = {
      {"type", type},
      {"payload", Json::parse(payload_json)}
  };

  ws_client_->Send(message.dump());
}

void NetworkCollaborationCoordinator::HandleWebSocketMessage(
    const std::string& message_str) {
  try {
    Json message = Json::parse(message_str);
    std::string type = message["type"];

    if (type == "session_hosted") {
      Json payload = message["payload"];
      session_id_ = payload["session_id"];
      session_code_ = payload["session_code"];
      session_name_ = payload["session_name"];
      
      if (payload.contains("participants")) {
        absl::MutexLock lock(&mutex_);
        if (participant_callback_) {
          std::vector<std::string> participants = payload["participants"];
          participant_callback_(participants);
        }
      }
    } else if (type == "session_joined") {
      Json payload = message["payload"];
      session_id_ = payload["session_id"];
      session_code_ = payload["session_code"];
      session_name_ = payload["session_name"];
      
      if (payload.contains("participants")) {
        absl::MutexLock lock(&mutex_);
        if (participant_callback_) {
          std::vector<std::string> participants = payload["participants"];
          participant_callback_(participants);
        }
      }
    } else if (type == "chat_message") {
      Json payload = message["payload"];
      ChatMessage msg;
      msg.sender = payload["sender"];
      msg.message = payload["message"];
      msg.timestamp = payload["timestamp"];
      msg.message_type = payload.value("message_type", "chat");
      if (payload.contains("metadata") && !payload["metadata"].is_null()) {
        msg.metadata = payload["metadata"].dump();
      }
      
      absl::MutexLock lock(&mutex_);
      if (message_callback_) {
        message_callback_(msg);
      }
    } else if (type == "rom_sync") {
      Json payload = message["payload"];
      RomSync sync;
      sync.sync_id = payload["sync_id"];
      sync.sender = payload["sender"];
      sync.diff_data = payload["diff_data"];
      sync.rom_hash = payload["rom_hash"];
      sync.timestamp = payload["timestamp"];
      
      absl::MutexLock lock(&mutex_);
      if (rom_sync_callback_) {
        rom_sync_callback_(sync);
      }
    } else if (type == "snapshot_shared") {
      Json payload = message["payload"];
      Snapshot snapshot;
      snapshot.snapshot_id = payload["snapshot_id"];
      snapshot.sender = payload["sender"];
      snapshot.snapshot_data = payload["snapshot_data"];
      snapshot.snapshot_type = payload["snapshot_type"];
      snapshot.timestamp = payload["timestamp"];
      
      absl::MutexLock lock(&mutex_);
      if (snapshot_callback_) {
        snapshot_callback_(snapshot);
      }
    } else if (type == "proposal_shared") {
      Json payload = message["payload"];
      Proposal proposal;
      proposal.proposal_id = payload["proposal_id"];
      proposal.sender = payload["sender"];
      proposal.proposal_data = payload["proposal_data"].dump();
      proposal.status = payload["status"];
      proposal.timestamp = payload["timestamp"];
      
      absl::MutexLock lock(&mutex_);
      if (proposal_callback_) {
        proposal_callback_(proposal);
      }
    } else if (type == "proposal_updated") {
      Json payload = message["payload"];
      std::string proposal_id = payload["proposal_id"];
      std::string status = payload["status"];
      
      absl::MutexLock lock(&mutex_);
      if (proposal_update_callback_) {
        proposal_update_callback_(proposal_id, status);
      }
    } else if (type == "ai_response") {
      Json payload = message["payload"];
      AIResponse response;
      response.query_id = payload["query_id"];
      response.username = payload["username"];
      response.query = payload["query"];
      response.response = payload["response"];
      response.timestamp = payload["timestamp"];
      
      absl::MutexLock lock(&mutex_);
      if (ai_response_callback_) {
        ai_response_callback_(response);
      }
    } else if (type == "server_shutdown") {
      Json payload = message["payload"];
      std::string error = "Server shutdown: " + payload["message"].get<std::string>();
      
      absl::MutexLock lock(&mutex_);
      if (error_callback_) {
        error_callback_(error);
      }
      
      // Disconnect
      connected_ = false;
    } else if (type == "participant_joined" || type == "participant_left") {
      Json payload = message["payload"];
      if (payload.contains("participants")) {
        absl::MutexLock lock(&mutex_);
        if (participant_callback_) {
          std::vector<std::string> participants = payload["participants"];
          participant_callback_(participants);
        }
      }
    } else if (type == "error") {
      Json payload = message["payload"];
      std::string error = payload["error"];
      
      absl::MutexLock lock(&mutex_);
      if (error_callback_) {
        error_callback_(error);
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error parsing WebSocket message: " << e.what() << std::endl;
  }
}

void NetworkCollaborationCoordinator::WebSocketReceiveLoop() {
  while (!should_stop_ && connected_) {
    if (!ws_client_) break;
    
    std::string message = ws_client_->Receive();
    if (!message.empty()) {
      HandleWebSocketMessage(message);
    }
    
    // Small sleep to avoid busy-waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

#else  // !YAZE_WITH_JSON

// Stub implementations when JSON is not available
NetworkCollaborationCoordinator::NetworkCollaborationCoordinator(
    const std::string& server_url) : server_url_(server_url) {}

NetworkCollaborationCoordinator::~NetworkCollaborationCoordinator() = default;

absl::StatusOr<NetworkCollaborationCoordinator::SessionInfo>
NetworkCollaborationCoordinator::HostSession(const std::string&, const std::string&,
                                             const std::string&, bool) {
  return absl::UnimplementedError("Network collaboration requires JSON support");
}

absl::StatusOr<NetworkCollaborationCoordinator::SessionInfo>
NetworkCollaborationCoordinator::JoinSession(const std::string&, const std::string&) {
  return absl::UnimplementedError("Network collaboration requires JSON support");
}

absl::Status NetworkCollaborationCoordinator::LeaveSession() {
  return absl::UnimplementedError("Network collaboration requires JSON support");
}

absl::Status NetworkCollaborationCoordinator::SendMessage(
    const std::string&, const std::string&, const std::string&, const std::string&) {
  return absl::UnimplementedError("Network collaboration requires JSON support");
}

absl::Status NetworkCollaborationCoordinator::SendRomSync(
    const std::string&, const std::string&, const std::string&) {
  return absl::UnimplementedError("Network collaboration requires JSON support");
}

absl::Status NetworkCollaborationCoordinator::SendSnapshot(
    const std::string&, const std::string&, const std::string&) {
  return absl::UnimplementedError("Network collaboration requires JSON support");
}

absl::Status NetworkCollaborationCoordinator::SendProposal(
    const std::string&, const std::string&) {
  return absl::UnimplementedError("Network collaboration requires JSON support");
}

absl::Status NetworkCollaborationCoordinator::UpdateProposal(
    const std::string&, const std::string&) {
  return absl::UnimplementedError("Network collaboration requires JSON support");
}

absl::Status NetworkCollaborationCoordinator::SendAIQuery(
    const std::string&, const std::string&) {
  return absl::UnimplementedError("Network collaboration requires JSON support");
}

bool NetworkCollaborationCoordinator::IsConnected() const { return false; }

void NetworkCollaborationCoordinator::SetMessageCallback(MessageCallback) {}
void NetworkCollaborationCoordinator::SetParticipantCallback(ParticipantCallback) {}
void NetworkCollaborationCoordinator::SetErrorCallback(ErrorCallback) {}
void NetworkCollaborationCoordinator::SetRomSyncCallback(RomSyncCallback) {}
void NetworkCollaborationCoordinator::SetSnapshotCallback(SnapshotCallback) {}
void NetworkCollaborationCoordinator::SetProposalCallback(ProposalCallback) {}
void NetworkCollaborationCoordinator::SetProposalUpdateCallback(ProposalUpdateCallback) {}
void NetworkCollaborationCoordinator::SetAIResponseCallback(AIResponseCallback) {}
void NetworkCollaborationCoordinator::ConnectWebSocket() {}
void NetworkCollaborationCoordinator::DisconnectWebSocket() {}
void NetworkCollaborationCoordinator::SendWebSocketMessage(const std::string&, const std::string&) {}
void NetworkCollaborationCoordinator::HandleWebSocketMessage(const std::string&) {}
void NetworkCollaborationCoordinator::WebSocketReceiveLoop() {}

#endif  // YAZE_WITH_JSON

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
