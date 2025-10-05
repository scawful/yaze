#include "cli/handlers/net/net_commands.h"

#include <iostream>
#include <thread>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace cli {
namespace net {

namespace {

// Global network client for CLI operations
std::unique_ptr<Z3edNetworkClient> g_network_client;

void EnsureClient() {
  if (!g_network_client) {
    g_network_client = std::make_unique<Z3edNetworkClient>();
  }
}

}  // namespace

absl::Status HandleNetConnect(const std::vector<std::string>& args) {
  EnsureClient();
  
  std::string host = "localhost";
  int port = 8765;
  
  // Parse arguments
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--host" && i + 1 < args.size()) {
      host = args[i + 1];
      ++i;
    } else if (args[i] == "--port" && i + 1 < args.size()) {
      port = std::stoi(args[i + 1]);
      ++i;
    }
  }
  
  std::cout << "Connecting to " << host << ":" << port << "..." << std::endl;
  
  auto status = g_network_client->Connect(host, port);
  
  if (status.ok()) {
    std::cout << "✓ Connected to yaze-server" << std::endl;
  } else {
    std::cerr << "✗ Connection failed: " << status.message() << std::endl;
  }
  
  return status;
}

absl::Status HandleNetJoin(const std::vector<std::string>& args) {
  EnsureClient();
  
  if (!g_network_client->IsConnected()) {
    return absl::FailedPreconditionError(
        "Not connected. Run: z3ed net connect");
  }
  
  std::string session_code;
  std::string username;
  
  // Parse arguments
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--code" && i + 1 < args.size()) {
      session_code = args[i + 1];
      ++i;
    } else if (args[i] == "--username" && i + 1 < args.size()) {
      username = args[i + 1];
      ++i;
    }
  }
  
  if (session_code.empty() || username.empty()) {
    return absl::InvalidArgumentError(
        "Usage: z3ed net join --code <CODE> --username <NAME>");
  }
  
  std::cout << "Joining session " << session_code << " as " << username << "..." 
            << std::endl;
  
  auto status = g_network_client->JoinSession(session_code, username);
  
  if (status.ok()) {
    std::cout << "✓ Joined session successfully" << std::endl;
  } else {
    std::cerr << "✗ Failed to join: " << status.message() << std::endl;
  }
  
  return status;
}

absl::Status HandleNetLeave(const std::vector<std::string>& args) {
  EnsureClient();
  
  if (!g_network_client->IsConnected()) {
    return absl::FailedPreconditionError("Not connected");
  }
  
  std::cout << "Leaving session..." << std::endl;
  
  g_network_client->Disconnect();
  
  std::cout << "✓ Left session" << std::endl;
  
  return absl::OkStatus();
}

absl::Status HandleNetProposal(const std::vector<std::string>& args) {
  EnsureClient();
  
  if (!g_network_client->IsConnected()) {
    return absl::FailedPreconditionError(
        "Not connected. Run: z3ed net connect");
  }
  
  if (args.empty()) {
    std::cout << "Usage:\n";
    std::cout << "  z3ed net proposal submit --description <DESC> --data <JSON>\n";
    std::cout << "  z3ed net proposal status --id <ID>\n";
    std::cout << "  z3ed net proposal wait --id <ID> [--timeout <SEC>]\n";
    return absl::OkStatus();
  }
  
  std::string subcommand = args[0];
  std::vector<std::string> subargs(args.begin() + 1, args.end());
  
  if (subcommand == "submit") {
    return HandleProposalSubmit(subargs);
  } else if (subcommand == "status") {
    return HandleProposalStatus(subargs);
  } else if (subcommand == "wait") {
    return HandleProposalWait(subargs);
  } else {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unknown proposal subcommand: %s", subcommand));
  }
}

absl::Status HandleProposalSubmit(const std::vector<std::string>& args) {
  std::string description;
  std::string data_json;
  std::string username = "cli_user";  // Default
  
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--description" && i + 1 < args.size()) {
      description = args[i + 1];
      ++i;
    } else if (args[i] == "--data" && i + 1 < args.size()) {
      data_json = args[i + 1];
      ++i;
    } else if (args[i] == "--username" && i + 1 < args.size()) {
      username = args[i + 1];
      ++i;
    }
  }
  
  if (description.empty() || data_json.empty()) {
    return absl::InvalidArgumentError(
        "Usage: z3ed net proposal submit --description <DESC> --data <JSON>");
  }
  
  std::cout << "Submitting proposal..." << std::endl;
  std::cout << "  Description: " << description << std::endl;
  
  auto status = g_network_client->SubmitProposal(
      description,
      data_json,
      username
  );
  
  if (status.ok()) {
    std::cout << "✓ Proposal submitted" << std::endl;
    std::cout << "  Waiting for approval from host..." << std::endl;
  } else {
    std::cerr << "✗ Failed to submit: " << status.message() << std::endl;
  }
  
  return status;
}

absl::Status HandleProposalStatus(const std::vector<std::string>& args) {
  std::string proposal_id;
  
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--id" && i + 1 < args.size()) {
      proposal_id = args[i + 1];
      ++i;
    }
  }
  
  if (proposal_id.empty()) {
    return absl::InvalidArgumentError(
        "Usage: z3ed net proposal status --id <ID>");
  }
  
  auto status_result = g_network_client->GetProposalStatus(proposal_id);
  
  if (status_result.ok()) {
    std::cout << "Proposal " << proposal_id.substr(0, 8) << "..." << std::endl;
    std::cout << "  Status: " << *status_result << std::endl;
  } else {
    std::cerr << "✗ Failed to get status: " << status_result.status().message() 
              << std::endl;
  }
  
  return status_result.status();
}

absl::Status HandleProposalWait(const std::vector<std::string>& args) {
  std::string proposal_id;
  int timeout_seconds = 60;
  
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--id" && i + 1 < args.size()) {
      proposal_id = args[i + 1];
      ++i;
    } else if (args[i] == "--timeout" && i + 1 < args.size()) {
      timeout_seconds = std::stoi(args[i + 1]);
      ++i;
    }
  }
  
  if (proposal_id.empty()) {
    return absl::InvalidArgumentError(
        "Usage: z3ed net proposal wait --id <ID> [--timeout <SEC>]");
  }
  
  std::cout << "Waiting for approval (timeout: " << timeout_seconds << "s)..." 
            << std::endl;
  
  auto approved_result = g_network_client->WaitForApproval(
      proposal_id,
      timeout_seconds
  );
  
  if (approved_result.ok()) {
    if (*approved_result) {
      std::cout << "✓ Proposal approved!" << std::endl;
    } else {
      std::cout << "✗ Proposal rejected" << std::endl;
    }
  } else {
    std::cerr << "✗ Error: " << approved_result.status().message() << std::endl;
  }
  
  return approved_result.status();
}

absl::Status HandleNetStatus(const std::vector<std::string>& args) {
  EnsureClient();
  
  std::cout << "Network Status:" << std::endl;
  std::cout << "  Connected: " 
            << (g_network_client->IsConnected() ? "Yes" : "No") << std::endl;
  
  return absl::OkStatus();
}

}  // namespace net
}  // namespace cli
}  // namespace yaze
