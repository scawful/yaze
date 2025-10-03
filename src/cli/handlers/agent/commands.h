#ifndef YAZE_CLI_HANDLERS_AGENT_COMMANDS_H_
#define YAZE_CLI_HANDLERS_AGENT_COMMANDS_H_

#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
class Rom;

namespace cli {
namespace agent {

absl::Status HandleRunCommand(const std::vector<std::string>& args,
							  Rom& rom);
absl::Status HandlePlanCommand(const std::vector<std::string>& args);
absl::Status HandleDiffCommand(Rom& rom,
							   const std::vector<std::string>& args);
absl::Status HandleAcceptCommand(const std::vector<std::string>& args, Rom& rom);
absl::Status HandleTestCommand(const std::vector<std::string>& args);
absl::Status HandleGuiCommand(const std::vector<std::string>& args);
absl::Status HandleLearnCommand();
absl::Status HandleListCommand();
absl::Status HandleCommitCommand(Rom& rom);
absl::Status HandleRevertCommand(Rom& rom);
absl::Status HandleDescribeCommand(const std::vector<std::string>& arg_vec);
absl::Status HandleResourceListCommand(const std::vector<std::string>& arg_vec);
absl::Status HandleDungeonListSpritesCommand(
    const std::vector<std::string>& arg_vec);

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_AGENT_COMMANDS_H_
