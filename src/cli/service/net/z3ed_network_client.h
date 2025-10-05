#ifndef YAZE_CLI_SERVICE_NET_Z3ED_NETWORK_CLIENT_H_
#define YAZE_CLI_SERVICE_NET_Z3ED_NETWORK_CLIENT_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace cli {
namespace net {

/**
 * @class Z3edNetworkClient
 * @brief Simplified WebSocket client for z3ed CLI
 * 
 * Provides command-line friendly interface for:
 * - Connecting to yaze-server
 * - Submitting proposals from CLI
 * - Checking approval status
 * - Simple chat messages
 */
class Z3edNetworkClient {
 public:
  Z3edNetworkClient();
  ~Z3edNetworkClient();
  
  /**
   * Connect to server
   */
  absl::Status Connect(const std::string& host, int port = 8765);
  
  /**
   * Join session
   */
  absl::Status JoinSession(
      const std::string& session_code,
      const std::string& username);
  
  /**
   * Submit proposal
   * @param description Human-readable description
   * @param proposal_json JSON string with proposal details
   */
  absl::Status SubmitProposal(
      const std::string& description,
      const std::string& proposal_json,
      const std::string& username);
  
  /**
   * Check proposal status
   */
  absl::StatusOr<std::string> GetProposalStatus(
      const std::string& proposal_id);
  
  /**
   * Wait for proposal approval (blocking)
   * @param timeout_seconds How long to wait
   */
  absl::StatusOr<bool> WaitForApproval(
      const std::string& proposal_id,
      int timeout_seconds = 60);
  
  /**
   * Send chat message
   */
  absl::Status SendMessage(
      const std::string& message,
      const std::string& sender);
  
  /**
   * Query AI agent (if enabled)
   */
  absl::StatusOr<std::string> QueryAI(
      const std::string& query,
      const std::string& username);
  
  /**
   * Disconnect
   */
  void Disconnect();
  
  /**
   * Check if connected
   */
  bool IsConnected() const;
  
 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace net
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_NET_Z3ED_NETWORK_CLIENT_H_
