#ifndef YAZE_CLI_HANDLERS_AGENT_HEX_COMMANDS_H_
#define YAZE_CLI_HANDLERS_AGENT_HEX_COMMANDS_H_

#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
class Rom;

namespace cli {
namespace agent {

/**
 * @brief Read bytes from ROM at a specific address
 * 
 * @param args Command arguments: [address, length, format]
 * @param rom_context ROM instance to read from
 * @return absl::Status Result of the operation
 * 
 * Example: hex-read --address=0x1C800 --length=16 --format=both
 */
absl::Status HandleHexRead(const std::vector<std::string>& args,
                            Rom* rom_context = nullptr);

/**
 * @brief Write bytes to ROM at a specific address (creates proposal)
 * 
 * @param args Command arguments: [address, data]
 * @param rom_context ROM instance to write to
 * @return absl::Status Result of the operation
 * 
 * Example: hex-write --address=0x1C800 --data="FF 00 12 34"
 */
absl::Status HandleHexWrite(const std::vector<std::string>& args,
                             Rom* rom_context = nullptr);

/**
 * @brief Search for a byte pattern in ROM
 * 
 * @param args Command arguments: [pattern, start_address, end_address]
 * @param rom_context ROM instance to search in
 * @return absl::Status Result of the operation
 * 
 * Example: hex-search --pattern="FF 00 ?? 12" --start=0x00000
 */
absl::Status HandleHexSearch(const std::vector<std::string>& args,
                              Rom* rom_context = nullptr);

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_AGENT_HEX_COMMANDS_H_
