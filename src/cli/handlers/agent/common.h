#ifndef YAZE_CLI_HANDLERS_AGENT_COMMON_H_
#define YAZE_CLI_HANDLERS_AGENT_COMMON_H_

#include <optional>
#include <string>

#include "absl/time/time.h"
#include "cli/service/gui_automation_client.h"

namespace yaze {
namespace cli {
namespace agent {

std::string HarnessAddress(const std::string& host, int port);
std::string JsonEscape(absl::string_view value);
std::string YamlQuote(absl::string_view value);
std::string FormatOptionalTime(const std::optional<absl::Time>& time);
std::string OptionalTimeToIso(const std::optional<absl::Time>& time);
std::string OptionalTimeToJson(const std::optional<absl::Time>& time);
std::string OptionalTimeToYaml(const std::optional<absl::Time>& time);
const char* TestRunStatusToString(TestRunStatus status);
bool IsTerminalStatus(TestRunStatus status);
std::optional<TestRunStatus> ParseStatusFilter(absl::string_view value);
std::optional<WidgetTypeFilter> ParseWidgetTypeFilter(absl::string_view value);

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_AGENT_COMMON_H_
