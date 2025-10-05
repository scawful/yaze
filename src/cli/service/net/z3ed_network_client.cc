#include "cli/service/net/z3ed_network_client.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#endif

namespace yaze {
namespace cli {
namespace net {

#ifdef YAZE_WITH_JSON

// Implementation using httplib for cross-platform WebSocket support
class Z3edNetworkClient::Impl {
 public:
  Impl() : connected_(false), in_session_(false) {}
  
  ~Impl() {
    Disconnect();
  }
  
  absl::Status Connect(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
      return absl::AlreadyExistsError("Already connected");
    }
    
    host_ = host;
    port_ = port;
    
    try {
      // Create HTTP client for WebSocket fallback
      client_ = std::make_unique<httplib::Client>(host, port);
      client_->set_connection_timeout(5, 0);
      client_->set_read_timeout(30, 0);
      
      // Test connection
      auto res = client_->Get("/health");
      if (!res || res->status != 200) {
        return absl::UnavailableError("Server not responding");
      }
      
      connected_ = true;
      return absl::OkStatus();
      
    } catch (const std::exception& e) {
      return absl::UnavailableError(
          absl::StrCat("Connection failed: ", e.what()));
    }
  }
  
  void Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = false;
    in_session_ = false;
    client_.reset();
  }
  
  absl::Status JoinSession(const std::string& session_code,
                           const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
      return absl::FailedPreconditionError("Not connected");
    }
    
    try {
      nlohmann::json message = {
        {"type", "join_session"},
        {"payload", {
          {"session_code", session_code},
          {"username", username}
        }}
      };
      
      auto res = client_->Post("/message", message.dump(), "application/json");
      
      if (!res || res->status != 200) {
        return absl::InternalError("Failed to join session");
      }
      
      in_session_ = true;
      session_code_ = session_code;
      username_ = username;
      
      return absl::OkStatus();
      
    } catch (const std::exception& e) {
      return absl::InternalError(absl::StrCat("Join failed: ", e.what()));
    }
  }
  
  absl::Status SubmitProposal(const std::string& description,
                               const std::string& proposal_json,
                               const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_ || !in_session_) {
      return absl::FailedPreconditionError("Not in a session");
    }
    
    try {
      nlohmann::json proposal_data = nlohmann::json::parse(proposal_json);
      proposal_data["description"] = description;
      
      nlohmann::json message = {
        {"type", "proposal_share"},
        {"payload", {
          {"sender", username},
          {"proposal_data", proposal_data}
        }}
      };
      
      auto res = client_->Post("/message", message.dump(), "application/json");
      
      if (!res || res->status != 200) {
        return absl::InternalError("Failed to submit proposal");
      }
      
      // Extract proposal ID from response if available
      if (!res->body.empty()) {
        try {
          auto response_json = nlohmann::json::parse(res->body);
          if (response_json.contains("proposal_id")) {
            last_proposal_id_ = response_json["proposal_id"];
          }
        } catch (...) {
          // Response parsing failed, continue
        }
      }
      
      return absl::OkStatus();
      
    } catch (const std::exception& e) {
      return absl::InternalError(
          absl::StrCat("Proposal submission failed: ", e.what()));
    }
  }
  
  absl::StatusOr<std::string> GetProposalStatus(const std::string& proposal_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
      return absl::FailedPreconditionError("Not connected");
    }
    
    try {
      // Query server for proposal status
      auto res = client_->Get(
          absl::StrFormat("/proposal/%s/status", proposal_id).c_str());
      
      if (!res || res->status != 200) {
        return absl::NotFoundError("Proposal not found");
      }
      
      auto response = nlohmann::json::parse(res->body);
      return response["status"].get<std::string>();
      
    } catch (const std::exception& e) {
      return absl::InternalError(
          absl::StrCat("Status check failed: ", e.what()));
    }
  }
  
  absl::StatusOr<bool> WaitForApproval(const std::string& proposal_id,
                                        int timeout_seconds) {
    auto deadline = absl::Now() + absl::Seconds(timeout_seconds);
    
    while (absl::Now() < deadline) {
      auto status_result = GetProposalStatus(proposal_id);
      
      if (!status_result.ok()) {
        return status_result.status();
      }
      
      std::string status = *status_result;
      
      if (status == "approved" || status == "applied") {
        return true;
      } else if (status == "rejected") {
        return false;
      }
      
      // Poll every second
      absl::SleepFor(absl::Seconds(1));
    }
    
    return absl::DeadlineExceededError("Approval timeout");
  }
  
  absl::Status SendMessage(const std::string& message,
                           const std::string& sender) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_ || !in_session_) {
      return absl::FailedPreconditionError("Not in a session");
    }
    
    try {
      nlohmann::json msg = {
        {"type", "chat_message"},
        {"payload", {
          {"message", message},
          {"sender", sender}
        }}
      };
      
      auto res = client_->Post("/message", msg.dump(), "application/json");
      
      if (!res || res->status != 200) {
        return absl::InternalError("Failed to send message");
      }
      
      return absl::OkStatus();
      
    } catch (const std::exception& e) {
      return absl::InternalError(absl::StrCat("Send failed: ", e.what()));
    }
  }
  
  absl::StatusOr<std::string> QueryAI(const std::string& query,
                                       const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_ || !in_session_) {
      return absl::FailedPreconditionError("Not in a session");
    }
    
    try {
      nlohmann::json message = {
        {"type", "ai_query"},
        {"payload", {
          {"query", query},
          {"username", username}
        }}
      };
      
      auto res = client_->Post("/message", message.dump(), "application/json");
      
      if (!res || res->status != 200) {
        return absl::InternalError("AI query failed");
      }
      
      // Wait for response (in a real implementation, this would use callbacks)
      // For now, return placeholder
      return std::string("AI agent endpoint not configured");
      
    } catch (const std::exception& e) {
      return absl::InternalError(absl::StrCat("AI query failed: ", e.what()));
    }
  }
  
  bool IsConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
  }
  
  std::string GetLastProposalId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_proposal_id_;
  }
  
 private:
  mutable std::mutex mutex_;
  std::unique_ptr<httplib::Client> client_;
  
  std::string host_;
  int port_;
  bool connected_;
  bool in_session_;
  
  std::string session_code_;
  std::string username_;
  std::string last_proposal_id_;
};

#else

// Stub implementation when JSON is not available
class Z3edNetworkClient::Impl {
 public:
  absl::Status Connect(const std::string&, int) {
    return absl::UnimplementedError("Network support requires JSON library");
  }
  void Disconnect() {}
  absl::Status JoinSession(const std::string&, const std::string&) {
    return absl::UnimplementedError("Network support requires JSON library");
  }
  absl::Status SubmitProposal(const std::string&, const std::string&, const std::string&) {
    return absl::UnimplementedError("Network support requires JSON library");
  }
  absl::StatusOr<std::string> GetProposalStatus(const std::string&) {
    return absl::UnimplementedError("Network support requires JSON library");
  }
  absl::StatusOr<bool> WaitForApproval(const std::string&, int) {
    return absl::UnimplementedError("Network support requires JSON library");
  }
  absl::Status SendMessage(const std::string&, const std::string&) {
    return absl::UnimplementedError("Network support requires JSON library");
  }
  absl::StatusOr<std::string> QueryAI(const std::string&, const std::string&) {
    return absl::UnimplementedError("Network support requires JSON library");
  }
  bool IsConnected() const { return false; }
  std::string GetLastProposalId() const { return ""; }
};

#endif  // YAZE_WITH_JSON

// ============================================================================
// Z3edNetworkClient Implementation
// ============================================================================

Z3edNetworkClient::Z3edNetworkClient()
    : impl_(std::make_unique<Impl>()) {
}

Z3edNetworkClient::~Z3edNetworkClient() = default;

absl::Status Z3edNetworkClient::Connect(const std::string& host, int port) {
  return impl_->Connect(host, port);
}

absl::Status Z3edNetworkClient::JoinSession(
    const std::string& session_code,
    const std::string& username) {
  return impl_->JoinSession(session_code, username);
}

absl::Status Z3edNetworkClient::SubmitProposal(
    const std::string& description,
    const std::string& proposal_json,
    const std::string& username) {
  return impl_->SubmitProposal(description, proposal_json, username);
}

absl::StatusOr<std::string> Z3edNetworkClient::GetProposalStatus(
    const std::string& proposal_id) {
  return impl_->GetProposalStatus(proposal_id);
}

absl::StatusOr<bool> Z3edNetworkClient::WaitForApproval(
    const std::string& proposal_id,
    int timeout_seconds) {
  return impl_->WaitForApproval(proposal_id, timeout_seconds);
}

absl::Status Z3edNetworkClient::SendMessage(
    const std::string& message,
    const std::string& sender) {
  return impl_->SendMessage(message, sender);
}

absl::StatusOr<std::string> Z3edNetworkClient::QueryAI(
    const std::string& query,
    const std::string& username) {
  return impl_->QueryAI(query, username);
}

void Z3edNetworkClient::Disconnect() {
  impl_->Disconnect();
}

bool Z3edNetworkClient::IsConnected() const {
  return impl_->IsConnected();
}

}  // namespace net
}  // namespace cli
}  // namespace yaze
