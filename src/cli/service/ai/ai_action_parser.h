#ifndef YAZE_CLI_SERVICE_AI_AI_ACTION_PARSER_H_
#define YAZE_CLI_SERVICE_AI_AI_ACTION_PARSER_H_

#include <map>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze {
namespace cli {
namespace ai {

/**
 * @enum AIActionType
 * @brief Types of actions the AI can request
 */
enum class AIActionType {
  kOpenEditor,   // Open a specific editor window
  kSelectTile,   // Select a tile from the tile16 selector
  kPlaceTile,    // Place a tile at a specific position
  kSaveTile,     // Save tile changes to ROM
  kVerifyTile,   // Verify a tile was placed correctly
  kClickButton,  // Click a specific button
  kWait,         // Wait for a duration or condition
  kScreenshot,   // Take a screenshot for verification
  kInvalidAction
};

/**
 * @struct AIAction
 * @brief Represents a single action to be performed in the GUI
 */
struct AIAction {
  AIActionType type;
  std::map<std::string, std::string> parameters;

  AIAction() : type(AIActionType::kInvalidAction) {}
  AIAction(AIActionType t) : type(t) {}
  AIAction(AIActionType t, const std::map<std::string, std::string>& params)
      : type(t), parameters(params) {}
};

/**
 * @class AIActionParser
 * @brief Parses natural language commands into structured GUI actions
 * 
 * Understands commands like:
 * - "Place tile 0x42 at overworld position (5, 7)"
 * - "Open the overworld editor"
 * - "Select tile 100 from the tile selector"
 */
class AIActionParser {
 public:
  /**
   * Parse a natural language command into a sequence of AI actions
   * @param command The command to parse
   * @return Vector of actions, or error status
   */
  static absl::StatusOr<std::vector<AIAction>> ParseCommand(
      const std::string& command);

  /**
   * Convert an action back to a human-readable string
   */
  static std::string ActionToString(const AIAction& action);

 private:
  static AIActionType ParseActionType(const std::string& verb);
  static std::map<std::string, std::string> ExtractParameters(
      const std::string& command, AIActionType type);

  // Pattern matchers for different command types
  static bool MatchesPlaceTilePattern(
      const std::string& command, std::map<std::string, std::string>* params);
  static bool MatchesSelectTilePattern(
      const std::string& command, std::map<std::string, std::string>* params);
  static bool MatchesOpenEditorPattern(
      const std::string& command, std::map<std::string, std::string>* params);
};

}  // namespace ai
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AI_AI_ACTION_PARSER_H_
