#ifndef YAZE_SRC_CLI_SERVICE_AGENT_PROPOSAL_EXECUTOR_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_PROPOSAL_EXECUTOR_H_

#include <filesystem>
#include <string>

#include "absl/status/statusor.h"
#include "cli/service/ai/common.h"
#include "cli/service/planning/proposal_registry.h"

namespace yaze {

class Rom;

namespace cli {
namespace agent {

struct ProposalCreationRequest {
  std::string prompt;
  const AgentResponse* response = nullptr;
  Rom* rom = nullptr;
  std::string sandbox_label;
  std::string ai_provider;
};

struct ProposalCreationResult {
  ProposalRegistry::ProposalMetadata metadata;
  std::filesystem::path proposal_json_path;
  int executed_commands = 0;
  int change_count = 0;
};

absl::StatusOr<ProposalCreationResult> CreateProposalFromAgentResponse(
    const ProposalCreationRequest& request);

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_PROPOSAL_EXECUTOR_H_
