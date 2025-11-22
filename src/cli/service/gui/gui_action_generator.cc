#include "cli/service/gui/gui_action_generator.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace cli {
namespace gui {

absl::StatusOr<std::string> GuiActionGenerator::GenerateTestScript(
    const std::vector<ai::AIAction>& actions) {
#ifdef YAZE_WITH_JSON
  auto json_result = GenerateTestJSON(actions);
  if (!json_result.ok()) {
    return json_result.status();
  }
  return json_result->dump(2);  // Pretty-print with 2-space indent
#else
  return absl::UnimplementedError(
      "JSON support required for test script generation");
#endif
}

#ifdef YAZE_WITH_JSON
absl::StatusOr<nlohmann::json> GuiActionGenerator::GenerateTestJSON(
    const std::vector<ai::AIAction>& actions) {
  nlohmann::json test_script;
  test_script["test_name"] = "ai_generated_test";
  test_script["description"] = "Automatically generated from AI actions";
  test_script["steps"] = nlohmann::json::array();

  for (size_t i = 0; i < actions.size(); ++i) {
    nlohmann::json step = ActionToJSON(actions[i]);
    step["step_number"] = i + 1;
    test_script["steps"].push_back(step);
  }

  return test_script;
}

nlohmann::json GuiActionGenerator::ActionToJSON(const ai::AIAction& action) {
  nlohmann::json step;

  switch (action.type) {
    case ai::AIActionType::kOpenEditor: {
      step["action"] = "click";
      auto it = action.parameters.find("editor");
      if (it != action.parameters.end()) {
        step["target"] = absl::StrCat("button:", it->second, " Editor");
        step["wait_after"] = 500;  // Wait 500ms for editor to open
      }
      break;
    }

    case ai::AIActionType::kSelectTile: {
      step["action"] = "click";
      auto it = action.parameters.find("tile_id");
      if (it != action.parameters.end()) {
        int tile_id = std::stoi(it->second);
        // Calculate position in tile selector (8 tiles per row, 16x16 pixels
        // each)
        int tile_x = (tile_id % 8) * 16 + 8;  // Center of tile
        int tile_y = (tile_id / 8) * 16 + 8;

        step["target"] = "canvas:tile16_selector";
        step["position"] = {{"x", tile_x}, {"y", tile_y}};
        step["wait_after"] = 200;
      }
      break;
    }

    case ai::AIActionType::kPlaceTile: {
      step["action"] = "click";
      auto x_it = action.parameters.find("x");
      auto y_it = action.parameters.find("y");

      if (x_it != action.parameters.end() && y_it != action.parameters.end()) {
        // Convert map coordinates to screen coordinates
        // Assuming 16x16 tile size and some offset for the canvas
        int screen_x = std::stoi(x_it->second) * 16 + 8;
        int screen_y = std::stoi(y_it->second) * 16 + 8;

        step["target"] = "canvas:overworld_map";
        step["position"] = {{"x", screen_x}, {"y", screen_y}};
        step["wait_after"] = 200;
      }
      break;
    }

    case ai::AIActionType::kSaveTile: {
      step["action"] = "click";
      step["target"] = "button:Save to ROM";
      step["wait_after"] = 300;
      break;
    }

    case ai::AIActionType::kClickButton: {
      step["action"] = "click";
      auto it = action.parameters.find("button");
      if (it != action.parameters.end()) {
        step["target"] = absl::StrCat("button:", it->second);
        step["wait_after"] = 200;
      }
      break;
    }

    case ai::AIActionType::kWait: {
      step["action"] = "wait";
      auto it = action.parameters.find("duration_ms");
      int duration =
          it != action.parameters.end() ? std::stoi(it->second) : 500;
      step["duration_ms"] = duration;
      break;
    }

    case ai::AIActionType::kScreenshot: {
      step["action"] = "screenshot";
      auto it = action.parameters.find("filename");
      if (it != action.parameters.end()) {
        step["filename"] = it->second;
      } else {
        step["filename"] = "verification.png";
      }
      break;
    }

    case ai::AIActionType::kVerifyTile: {
      step["action"] = "verify";
      step["target"] = "tile_placement";
      // Add verification parameters from action.parameters
      for (const auto& [key, value] : action.parameters) {
        step[key] = value;
      }
      break;
    }

    case ai::AIActionType::kInvalidAction:
      step["action"] = "error";
      step["message"] = "Invalid action type";
      break;
  }

  return step;
}
#endif

std::string GuiActionGenerator::ActionToTestStep(const ai::AIAction& action,
                                                 int step_number) {
  switch (action.type) {
    case ai::AIActionType::kOpenEditor:
      return GenerateOpenEditorStep(action);
    case ai::AIActionType::kSelectTile:
      return GenerateSelectTileStep(action);
    case ai::AIActionType::kPlaceTile:
      return GeneratePlaceTileStep(action);
    case ai::AIActionType::kSaveTile:
      return GenerateSaveTileStep(action);
    case ai::AIActionType::kClickButton:
      return GenerateClickButtonStep(action);
    case ai::AIActionType::kWait:
      return GenerateWaitStep(action);
    case ai::AIActionType::kScreenshot:
      return GenerateScreenshotStep(action);
    default:
      return absl::StrFormat("# Step %d: Unknown action", step_number);
  }
}

std::string GuiActionGenerator::GenerateOpenEditorStep(
    const ai::AIAction& action) {
  auto it = action.parameters.find("editor");
  if (it != action.parameters.end()) {
    return absl::StrFormat("Click button:'%s Editor'\nWait 500ms", it->second);
  }
  return "Click button:'Editor'\nWait 500ms";
}

std::string GuiActionGenerator::GenerateSelectTileStep(
    const ai::AIAction& action) {
  auto it = action.parameters.find("tile_id");
  if (it != action.parameters.end()) {
    int tile_id = std::stoi(it->second);
    int tile_x = (tile_id % 8) * 16 + 8;
    int tile_y = (tile_id / 8) * 16 + 8;
    return absl::StrFormat(
        "Click canvas:'tile16_selector' at (%d, %d)\nWait 200ms", tile_x,
        tile_y);
  }
  return "Click canvas:'tile16_selector'\nWait 200ms";
}

std::string GuiActionGenerator::GeneratePlaceTileStep(
    const ai::AIAction& action) {
  auto x_it = action.parameters.find("x");
  auto y_it = action.parameters.find("y");

  if (x_it != action.parameters.end() && y_it != action.parameters.end()) {
    int screen_x = std::stoi(x_it->second) * 16 + 8;
    int screen_y = std::stoi(y_it->second) * 16 + 8;
    return absl::StrFormat(
        "Click canvas:'overworld_map' at (%d, %d)\nWait 200ms", screen_x,
        screen_y);
  }
  return "Click canvas:'overworld_map'\nWait 200ms";
}

std::string GuiActionGenerator::GenerateSaveTileStep(
    const ai::AIAction& action) {
  return "Click button:'Save to ROM'\nWait 300ms";
}

std::string GuiActionGenerator::GenerateClickButtonStep(
    const ai::AIAction& action) {
  auto it = action.parameters.find("button");
  if (it != action.parameters.end()) {
    return absl::StrFormat("Click button:'%s'\nWait 200ms", it->second);
  }
  return "Click button\nWait 200ms";
}

std::string GuiActionGenerator::GenerateWaitStep(const ai::AIAction& action) {
  auto it = action.parameters.find("duration_ms");
  int duration = it != action.parameters.end() ? std::stoi(it->second) : 500;
  return absl::StrFormat("Wait %dms", duration);
}

std::string GuiActionGenerator::GenerateScreenshotStep(
    const ai::AIAction& action) {
  auto it = action.parameters.find("filename");
  if (it != action.parameters.end()) {
    return absl::StrFormat("Screenshot '%s'", it->second);
  }
  return "Screenshot 'verification.png'";
}

}  // namespace gui
}  // namespace cli
}  // namespace yaze
