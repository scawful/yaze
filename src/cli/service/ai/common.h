#ifndef YAZE_SRC_CLI_SERVICE_AI_COMMON_H_
#define YAZE_SRC_CLI_SERVICE_AI_COMMON_H_

#include <string>
#include <vector>

namespace yaze {
namespace cli {

// A structured response from an AI service.
struct AgentResponse {
  // A natural language response to the user.
  std::string text_response;

  // A list of z3ed commands to be executed.
  std::vector<std::string> commands;

  // The AI's explanation of its thought process.
  std::string reasoning;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AI_COMMON_H_
