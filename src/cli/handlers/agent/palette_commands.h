#ifndef YAZE_CLI_HANDLERS_AGENT_PALETTE_COMMANDS_H_
#define YAZE_CLI_HANDLERS_AGENT_PALETTE_COMMANDS_H_

#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
class Rom;

namespace cli {
namespace agent {

/**
 * @brief Get all colors from a specific palette
 * 
 * @param args Command arguments: [group, palette, format]
 * @param rom_context ROM instance to read from
 * @return absl::Status Result of the operation
 * 
 * Example: palette-get-colors --group=0 --palette=0 --format=hex
 */
absl::Status HandlePaletteGetColors(const std::vector<std::string>& args,
                                     Rom* rom_context = nullptr);

/**
 * @brief Set a specific color in a palette (creates proposal)
 * 
 * @param args Command arguments: [group, palette, color_index, color]
 * @param rom_context ROM instance to modify
 * @return absl::Status Result of the operation
 * 
 * Example: palette-set-color --group=0 --palette=0 --index=5 --color=FF0000
 */
absl::Status HandlePaletteSetColor(const std::vector<std::string>& args,
                                    Rom* rom_context = nullptr);

/**
 * @brief Analyze color usage and statistics for a palette or bitmap
 * 
 * @param args Command arguments: [target_type, target_id]
 * @param rom_context ROM instance to analyze
 * @return absl::Status Result of the operation
 * 
 * Example: palette-analyze --type=palette --id=0
 */
absl::Status HandlePaletteAnalyze(const std::vector<std::string>& args,
                                   Rom* rom_context = nullptr);

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_AGENT_PALETTE_COMMANDS_H_
