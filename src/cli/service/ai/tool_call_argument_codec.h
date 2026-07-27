#ifndef YAZE_CLI_SERVICE_AI_TOOL_CALL_ARGUMENT_CODEC_H_
#define YAZE_CLI_SERVICE_AI_TOOL_CALL_ARGUMENT_CODEC_H_

#include <cstdint>
#include <map>
#include <string>

#include "nlohmann/json.hpp"

namespace yaze::cli::ai {

// Convert provider-native JSON tool arguments into the string map consumed by
// ToolDispatcher. Preserve integer spelling so CLI integer parsers receive
// "7", not "7.000000", and preserve booleans for flag arguments.
inline std::map<std::string, std::string> DecodeToolCallArguments(
    const nlohmann::json& arguments) {
  std::map<std::string, std::string> decoded;
  if (!arguments.is_object()) {
    return decoded;
  }

  for (const auto& [key, value] : arguments.items()) {
    if (value.is_string()) {
      decoded[key] = value.get<std::string>();
    } else if (value.is_boolean()) {
      decoded[key] = value.get<bool>() ? "true" : "false";
    } else if (value.is_number_unsigned()) {
      decoded[key] = std::to_string(value.get<uint64_t>());
    } else if (value.is_number_integer()) {
      decoded[key] = std::to_string(value.get<int64_t>());
    } else if (value.is_number_float()) {
      decoded[key] = value.dump();
    }
  }
  return decoded;
}

}  // namespace yaze::cli::ai

#endif  // YAZE_CLI_SERVICE_AI_TOOL_CALL_ARGUMENT_CODEC_H_
