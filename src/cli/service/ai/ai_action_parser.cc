#include "cli/service/ai/ai_action_parser.h"

#include <algorithm>
#include <regex>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"

namespace yaze {
namespace cli {
namespace ai {

namespace {

// Helper to convert hex string to int
int ParseHexOrDecimal(const std::string& str) {
  if (absl::StartsWith(str, "0x") || absl::StartsWith(str, "0X")) {
    return std::stoi(str, nullptr, 16);
  }
  return std::stoi(str);
}

// Helper to extract coordinates like "(5, 7)" or "5,7" or "x=5 y=7"
bool ExtractCoordinates(const std::string& text, int* x, int* y) {
  // Pattern: (X, Y) or X,Y or x=X y=Y
  std::regex coord_pattern(R"(\(?(\d+)\s*,\s*(\d+)\)?)");
  std::smatch match;
  
  if (std::regex_search(text, match, coord_pattern) && match.size() >= 3) {
    *x = std::stoi(match[1].str());
    *y = std::stoi(match[2].str());
    return true;
  }
  
  // Try x=X y=Y format
  std::regex xy_pattern(R"(x\s*=\s*(\d+).*y\s*=\s*(\d+))", std::regex::icase);
  if (std::regex_search(text, match, xy_pattern) && match.size() >= 3) {
    *x = std::stoi(match[1].str());
    *y = std::stoi(match[2].str());
    return true;
  }
  
  return false;
}

}  // namespace

absl::StatusOr<std::vector<AIAction>> AIActionParser::ParseCommand(
    const std::string& command) {
  std::vector<AIAction> actions;
  
  std::string cmd_lower = command;
  std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(), ::tolower);
  
  // Try to match different patterns
  std::map<std::string, std::string> params;
  
  // Pattern 1: "Place tile X at position (Y, Z)"
  if (MatchesPlaceTilePattern(command, &params)) {
    // Actions: Select tile, place tile
    actions.push_back(AIAction(AIActionType::kSelectTile, params));
    actions.push_back(AIAction(AIActionType::kPlaceTile, params));
    actions.push_back(AIAction(AIActionType::kSaveTile, {}));
    return actions;
  }
  
  // Pattern 2: "Select tile X"
  if (MatchesSelectTilePattern(command, &params)) {
    actions.push_back(AIAction(AIActionType::kSelectTile, params));
    return actions;
  }
  
  // Pattern 3: "Open overworld editor"
  if (MatchesOpenEditorPattern(command, &params)) {
    actions.push_back(AIAction(AIActionType::kOpenEditor, params));
    return actions;
  }
  
  // Pattern 4: Simple button clicks
  if (absl::StrContains(cmd_lower, "click") || absl::StrContains(cmd_lower, "press")) {
    std::regex button_pattern(R"((save|load|export|import|open)\s+(\w+))", std::regex::icase);
    std::smatch match;
    if (std::regex_search(command, match, button_pattern)) {
      params["button"] = match[1].str() + " " + match[2].str();
      actions.push_back(AIAction(AIActionType::kClickButton, params));
      return actions;
    }
  }
  
  return absl::InvalidArgumentError(
      absl::StrCat("Could not parse AI command: ", command));
}

std::string AIActionParser::ActionToString(const AIAction& action) {
  switch (action.type) {
    case AIActionType::kOpenEditor: {
      auto it = action.parameters.find("editor");
      if (it != action.parameters.end()) {
        return absl::StrCat("Open ", it->second, " editor");
      }
      return "Open editor";
    }
    
    case AIActionType::kSelectTile: {
      auto it = action.parameters.find("tile_id");
      if (it != action.parameters.end()) {
        return absl::StrCat("Select tile ", it->second);
      }
      return "Select tile";
    }
    
    case AIActionType::kPlaceTile: {
      auto x_it = action.parameters.find("x");
      auto y_it = action.parameters.find("y");
      if (x_it != action.parameters.end() && y_it != action.parameters.end()) {
        return absl::StrCat("Place tile at position (", x_it->second, ", ", y_it->second, ")");
      }
      return "Place tile";
    }
    
    case AIActionType::kSaveTile:
      return "Save changes to ROM";
    
    case AIActionType::kVerifyTile:
      return "Verify tile placement";
    
    case AIActionType::kClickButton: {
      auto it = action.parameters.find("button");
      if (it != action.parameters.end()) {
        return absl::StrCat("Click ", it->second, " button");
      }
      return "Click button";
    }
    
    case AIActionType::kWait:
      return "Wait";
    
    case AIActionType::kScreenshot:
      return "Take screenshot";
    
    case AIActionType::kInvalidAction:
      return "Invalid action";
  }
  
  return "Unknown action";
}

bool AIActionParser::MatchesPlaceTilePattern(
    const std::string& command,
    std::map<std::string, std::string>* params) {
  std::string cmd_lower = command;
  std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(), ::tolower);
  
  if (!absl::StrContains(cmd_lower, "place") && 
      !absl::StrContains(cmd_lower, "put") &&
      !absl::StrContains(cmd_lower, "set")) {
    return false;
  }
  
  if (!absl::StrContains(cmd_lower, "tile")) {
    return false;
  }
  
  // Extract tile ID
  std::regex tile_pattern(R"(tile\s+(?:id\s+)?(0x[0-9a-fA-F]+|\d+))", std::regex::icase);
  std::smatch match;
  
  if (std::regex_search(command, match, tile_pattern)) {
    try {
      int tile_id = ParseHexOrDecimal(match[1].str());
      (*params)["tile_id"] = std::to_string(tile_id);
    } catch (...) {
      return false;
    }
  } else {
    return false;
  }
  
  // Extract coordinates
  int x, y;
  if (ExtractCoordinates(command, &x, &y)) {
    (*params)["x"] = std::to_string(x);
    (*params)["y"] = std::to_string(y);
  } else {
    return false;
  }
  
  // Extract map ID if specified
  std::regex map_pattern(R"((?:map|overworld)\s+(?:id\s+)?(\d+))", std::regex::icase);
  if (std::regex_search(command, match, map_pattern)) {
    (*params)["map_id"] = match[1].str();
  } else {
    (*params)["map_id"] = "0";  // Default to map 0
  }
  
  return true;
}

bool AIActionParser::MatchesSelectTilePattern(
    const std::string& command,
    std::map<std::string, std::string>* params) {
  std::string cmd_lower = command;
  std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(), ::tolower);
  
  if (!absl::StrContains(cmd_lower, "select") && 
      !absl::StrContains(cmd_lower, "choose") &&
      !absl::StrContains(cmd_lower, "pick")) {
    return false;
  }
  
  if (!absl::StrContains(cmd_lower, "tile")) {
    return false;
  }
  
  // Extract tile ID
  std::regex tile_pattern(R"(tile\s+(?:id\s+)?(0x[0-9a-fA-F]+|\d+))", std::regex::icase);
  std::smatch match;
  
  if (std::regex_search(command, match, tile_pattern)) {
    try {
      int tile_id = ParseHexOrDecimal(match[1].str());
      (*params)["tile_id"] = std::to_string(tile_id);
      return true;
    } catch (...) {
      return false;
    }
  }
  
  return false;
}

bool AIActionParser::MatchesOpenEditorPattern(
    const std::string& command,
    std::map<std::string, std::string>* params) {
  std::string cmd_lower = command;
  std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(), ::tolower);
  
  if (!absl::StrContains(cmd_lower, "open") && 
      !absl::StrContains(cmd_lower, "launch") &&
      !absl::StrContains(cmd_lower, "start")) {
    return false;
  }
  
  if (absl::StrContains(cmd_lower, "overworld")) {
    (*params)["editor"] = "overworld";
    return true;
  }
  
  if (absl::StrContains(cmd_lower, "dungeon")) {
    (*params)["editor"] = "dungeon";
    return true;
  }
  
  if (absl::StrContains(cmd_lower, "sprite")) {
    (*params)["editor"] = "sprite";
    return true;
  }
  
  if (absl::StrContains(cmd_lower, "tile16") || absl::StrContains(cmd_lower, "tile 16")) {
    (*params)["editor"] = "tile16";
    return true;
  }
  
  return false;
}

}  // namespace ai
}  // namespace cli
}  // namespace yaze
