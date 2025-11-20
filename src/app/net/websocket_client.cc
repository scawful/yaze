#include "app/net/websocket_client.h"

#include <chrono>
#include <mutex>
#include <thread>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

// Cross-platform WebSocket support using httplib
#ifdef YAZE_WITH_JSON
#ifndef _WIN32
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include "httplib.h"
#endif

namespace yaze {

namespace net {

#ifdef YAZE_WITH_JSON

// Platform-independent WebSocket implementation using httplib
class WebSocketClient::Impl {
 public:
  Impl() : connected_(false), should_stop_(false) {}

  ~Impl() { Disconnect(); }

  absl::Status Connect(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (connected_) {
      return absl::AlreadyExistsError("Already connected");
    }

    host_ = host;
    port_ = port;

    try {
      // httplib WebSocket connection (cross-platform)
      std::string url = absl::StrFormat("ws://%s:%d", host, port);

      // Create WebSocket connection
      client_ = std::make_unique<httplib::Client>(host, port);
      client_->set_connection_timeout(5, 0);  // 5 seconds
      client_->set_read_timeout(30, 0);       // 30 seconds

      connected_ = true;
      should_stop_ = false;

      // Start receive thread
      receive_thread_ = std::thread([this]() { ReceiveLoop(); });

      return absl::OkStatus();

    } catch (const std::exception& e) {
      return absl::UnavailableError(
          absl::StrCat("Failed to connect: ", e.what()));
    }
  }

  void Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!connected_)
      return;

    should_stop_ = true;
    connected_ = false;

    if (receive_thread_.joinable()) {
      receive_thread_.join();
    }

    client_.reset();
  }

  absl::Status Send(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!connected_) {
      return absl::FailedPreconditionError("Not connected");
    }

    try {
      // In a real implementation, this would use WebSocket send
      // For now, we'll use HTTP POST as fallback
      auto res = client_->Post("/message", message, "application/json");

      if (!res) {
        return absl::UnavailableError("Failed to send message");
      }

      if (res->status != 200) {
        return absl::InternalError(
            absl::StrFormat("Server error: %d", res->status));
      }

      return absl::OkStatus();

    } catch (const std::exception& e) {
      return absl::InternalError(absl::StrCat("Send failed: ", e.what()));
    }
  }

  void SetMessageCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    message_callback_ = callback;
  }

  void SetErrorCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_callback_ = callback;
  }

  bool IsConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
  }

 private:
  void ReceiveLoop() {
    while (!should_stop_) {
      try {
        // Poll for messages (platform-independent)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // In a real WebSocket implementation, this would receive messages
        // For now, this is a placeholder for the receive loop

      } catch (const std::exception& e) {
        if (error_callback_) {
          error_callback_(e.what());
        }
      }
    }
  }

  mutable std::mutex mutex_;
  std::unique_ptr<httplib::Client> client_;
  std::thread receive_thread_;

  std::string host_;
  int port_;
  bool connected_;
  bool should_stop_;

  std::function<void(const std::string&)> message_callback_;
  std::function<void(const std::string&)> error_callback_;
};

#else

// Stub implementation when JSON is not available
class WebSocketClient::Impl {
 public:
  absl::Status Connect(const std::string&, int) {
    return absl::UnimplementedError("WebSocket support requires JSON library");
  }
  void Disconnect() {}
  absl::Status Send(const std::string&) {
    return absl::UnimplementedError("WebSocket support requires JSON library");
  }
  void SetMessageCallback(std::function<void(const std::string&)>) {}
  void SetErrorCallback(std::function<void(const std::string&)>) {}
  bool IsConnected() const { return false; }
};

#endif  // YAZE_WITH_JSON

// ============================================================================
// WebSocketClient Implementation
// ============================================================================

WebSocketClient::WebSocketClient()
    : impl_(std::make_unique<Impl>()), state_(ConnectionState::kDisconnected) {}

WebSocketClient::~WebSocketClient() {
  Disconnect();
}

absl::Status WebSocketClient::Connect(const std::string& host, int port) {
  auto status = impl_->Connect(host, port);

  if (status.ok()) {
    SetState(ConnectionState::kConnected);
  } else {
    SetState(ConnectionState::kError);
  }

  return status;
}

void WebSocketClient::Disconnect() {
  impl_->Disconnect();
  SetState(ConnectionState::kDisconnected);
  current_session_ = SessionInfo{};
}

absl::StatusOr<SessionInfo> WebSocketClient::HostSession(
    const std::string& session_name, const std::string& username,
    const std::string& rom_hash, bool ai_enabled) {
#ifdef YAZE_WITH_JSON
  if (!IsConnected()) {
    return absl::FailedPreconditionError("Not connected to server");
  }

  nlohmann::json message = {{"type", "host_session"},
                            {"payload",
                             {{"session_name", session_name},
                              {"username", username},
                              {"rom_hash", rom_hash},
                              {"ai_enabled", ai_enabled}}}};

  auto status = SendRaw(message);
  if (!status.ok()) {
    return status;
  }

  // In a real implementation, we'd wait for the server response
  // For now, return a placeholder
  SessionInfo session;
  session.session_name = session_name;
  session.host = username;
  session.rom_hash = rom_hash;
  session.ai_enabled = ai_enabled;

  current_session_ = session;
  return session;
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

absl::StatusOr<SessionInfo> WebSocketClient::JoinSession(
    const std::string& session_code, const std::string& username) {
#ifdef YAZE_WITH_JSON
  if (!IsConnected()) {
    return absl::FailedPreconditionError("Not connected to server");
  }

  nlohmann::json message = {
      {"type", "join_session"},
      {"payload", {{"session_code", session_code}, {"username", username}}}};

  auto status = SendRaw(message);
  if (!status.ok()) {
    return status;
  }

  // Placeholder - would wait for server response
  SessionInfo session;
  session.session_code = session_code;

  current_session_ = session;
  return session;
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

absl::Status WebSocketClient::LeaveSession() {
#ifdef YAZE_WITH_JSON
  if (!InSession()) {
    return absl::FailedPreconditionError("Not in a session");
  }

  nlohmann::json message = {{"type", "leave_session"}, {"payload", {}}};

  auto status = SendRaw(message);
  current_session_ = SessionInfo{};
  return status;
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

absl::Status WebSocketClient::SendChatMessage(const std::string& message,
                                              const std::string& sender) {
#ifdef YAZE_WITH_JSON
  nlohmann::json msg = {
      {"type", "chat_message"},
      {"payload", {{"message", message}, {"sender", sender}}}};

  return SendRaw(msg);
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

absl::Status WebSocketClient::SendRomSync(const std::string& diff_data,
                                          const std::string& rom_hash,
                                          const std::string& sender) {
#ifdef YAZE_WITH_JSON
  nlohmann::json message = {
      {"type", "rom_sync"},
      {"payload",
       {{"diff_data", diff_data}, {"rom_hash", rom_hash}, {"sender", sender}}}};

  return SendRaw(message);
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

absl::Status WebSocketClient::ShareProposal(const nlohmann::json& proposal_data,
                                            const std::string& sender) {
#ifdef YAZE_WITH_JSON
  nlohmann::json message = {
      {"type", "proposal_share"},
      {"payload", {{"sender", sender}, {"proposal_data", proposal_data}}}};

  return SendRaw(message);
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

absl::Status WebSocketClient::VoteOnProposal(const std::string& proposal_id,
                                             bool approved,
                                             const std::string& username) {
#ifdef YAZE_WITH_JSON
  nlohmann::json message = {{"type", "proposal_vote"},
                            {"payload",
                             {{"proposal_id", proposal_id},
                              {"approved", approved},
                              {"username", username}}}};

  return SendRaw(message);
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

absl::Status WebSocketClient::UpdateProposalStatus(
    const std::string& proposal_id, const std::string& status) {
#ifdef YAZE_WITH_JSON
  nlohmann::json message = {
      {"type", "proposal_update"},
      {"payload", {{"proposal_id", proposal_id}, {"status", status}}}};

  return SendRaw(message);
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

void WebSocketClient::OnMessage(const std::string& type,
                                MessageCallback callback) {
  message_callbacks_[type].push_back(callback);
}

void WebSocketClient::OnError(ErrorCallback callback) {
  error_callbacks_.push_back(callback);
}

void WebSocketClient::OnStateChange(StateCallback callback) {
  state_callbacks_.push_back(callback);
}

absl::StatusOr<SessionInfo> WebSocketClient::GetSessionInfo() const {
  if (!InSession()) {
    return absl::FailedPreconditionError("Not in a session");
  }
  return current_session_;
}

// Private methods

void WebSocketClient::HandleMessage(const std::string& message) {
#ifdef YAZE_WITH_JSON
  try {
    auto json = nlohmann::json::parse(message);
    std::string type = json["type"];

    auto it = message_callbacks_.find(type);
    if (it != message_callbacks_.end()) {
      for (auto& callback : it->second) {
        callback(json["payload"]);
      }
    }
  } catch (const std::exception& e) {
    HandleError(absl::StrCat("Failed to parse message: ", e.what()));
  }
#endif
}

void WebSocketClient::HandleError(const std::string& error) {
  for (auto& callback : error_callbacks_) {
    callback(error);
  }
}

void WebSocketClient::SetState(ConnectionState state) {
  if (state_ != state) {
    state_ = state;
    for (auto& callback : state_callbacks_) {
      callback(state);
    }
  }
}

absl::Status WebSocketClient::SendRaw(const nlohmann::json& message) {
#ifdef YAZE_WITH_JSON
  try {
    std::string msg_str = message.dump();
    return impl_->Send(msg_str);
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("Failed to serialize: ", e.what()));
  }
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

}  // namespace net

}  // namespace yaze
