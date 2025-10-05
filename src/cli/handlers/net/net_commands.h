#ifndef YAZE_CLI_HANDLERS_NET_NET_COMMANDS_H_
#define YAZE_CLI_HANDLERS_NET_NET_COMMANDS_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "cli/service/net/z3ed_network_client.h"

namespace yaze {
namespace cli {
namespace net {

/**
 * Handle 'z3ed net connect' command
 */
absl::Status HandleNetConnect(const std::vector<std::string>& args);

/**
 * Handle 'z3ed net join' command
 */
absl::Status HandleNetJoin(const std::vector<std::string>& args);

/**
 * Handle 'z3ed net leave' command
 */
absl::Status HandleNetLeave(const std::vector<std::string>& args);

/**
 * Handle 'z3ed net proposal' command
 */
absl::Status HandleNetProposal(const std::vector<std::string>& args);

/**
 * Handle 'z3ed net status' command
 */
absl::Status HandleNetStatus(const std::vector<std::string>& args);

// Proposal subcommands
absl::Status HandleProposalSubmit(const std::vector<std::string>& args);
absl::Status HandleProposalStatus(const std::vector<std::string>& args);
absl::Status HandleProposalWait(const std::vector<std::string>& args);

}  // namespace net
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_NET_NET_COMMANDS_H_
