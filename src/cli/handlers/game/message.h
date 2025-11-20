#ifndef YAZE_CLI_HANDLERS_MESSAGE_H_
#define YAZE_CLI_HANDLERS_MESSAGE_H_

#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
class Rom;

namespace cli {
namespace message {

// Message inspection handlers for agent tool calls

/**
 * @brief List all messages in the ROM
 * @param arg_vec Command arguments: [--format <json|text>] [--range <start-end>]
 * @param rom_context Optional ROM context to avoid reloading
 */
absl::Status HandleMessageListCommand(const std::vector<std::string>& arg_vec,
                                      Rom* rom_context = nullptr);

/**
 * @brief Read a specific message by ID
 * @param arg_vec Command arguments: --id <message_id> [--format <json|text>]
 * @param rom_context Optional ROM context to avoid reloading
 */
absl::Status HandleMessageReadCommand(const std::vector<std::string>& arg_vec,
                                      Rom* rom_context = nullptr);

/**
 * @brief Search for messages containing specific text
 * @param arg_vec Command arguments: --query <text> [--format <json|text>]
 * @param rom_context Optional ROM context to avoid reloading
 */
absl::Status HandleMessageSearchCommand(const std::vector<std::string>& arg_vec,
                                        Rom* rom_context = nullptr);

/**
 * @brief Get message statistics and overview
 * @param arg_vec Command arguments: [--format <json|text>]
 * @param rom_context Optional ROM context to avoid reloading
 */
absl::Status HandleMessageStatsCommand(const std::vector<std::string>& arg_vec,
                                       Rom* rom_context = nullptr);

}  // namespace message
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_MESSAGE_H_
