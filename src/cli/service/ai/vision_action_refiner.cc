#include "cli/service/ai/vision_action_refiner.h"

#include <algorithm>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "cli/service/ai/gemini_ai_service.h"

namespace yaze {
namespace cli {
namespace ai {

VisionActionRefiner::VisionActionRefiner(GeminiAIService* gemini_service)
    : gemini_service_(gemini_service) {
  if (!gemini_service_) {
    throw std::invalid_argument("Gemini service cannot be null");
  }
}

absl::StatusOr<VisionAnalysisResult> VisionActionRefiner::AnalyzeScreenshot(
    const std::filesystem::path& screenshot_path, const std::string& context) {
  if (!std::filesystem::exists(screenshot_path)) {
    return absl::NotFoundError(
        absl::StrCat("Screenshot not found: ", screenshot_path.string()));
  }

  std::string prompt = BuildAnalysisPrompt(context);

  auto response = gemini_service_->GenerateMultimodalResponse(
      screenshot_path.string(), prompt);

  if (!response.ok()) {
    return response.status();
  }

  return ParseAnalysisResponse(response->text_response);
}

absl::StatusOr<VisionAnalysisResult> VisionActionRefiner::VerifyAction(
    const AIAction& action, const std::filesystem::path& before_screenshot,
    const std::filesystem::path& after_screenshot) {
  if (!std::filesystem::exists(before_screenshot)) {
    return absl::NotFoundError("Before screenshot not found");
  }

  if (!std::filesystem::exists(after_screenshot)) {
    return absl::NotFoundError("After screenshot not found");
  }

  // First, analyze the after screenshot
  std::string verification_prompt = BuildVerificationPrompt(action);

  auto after_response = gemini_service_->GenerateMultimodalResponse(
      after_screenshot.string(), verification_prompt);

  if (!after_response.ok()) {
    return after_response.status();
  }

  return ParseVerificationResponse(after_response->text_response, action);
}

absl::StatusOr<ActionRefinement> VisionActionRefiner::RefineAction(
    const AIAction& original_action, const VisionAnalysisResult& analysis) {
  ActionRefinement refinement;

  // If action was successful, no refinement needed
  if (analysis.action_successful) {
    return refinement;
  }

  // Determine refinement strategy based on error
  std::string error_lower = analysis.error_message;
  std::transform(error_lower.begin(), error_lower.end(), error_lower.begin(),
                 ::tolower);

  if (error_lower.find("not found") != std::string::npos ||
      error_lower.find("missing") != std::string::npos) {
    refinement.needs_different_approach = true;
    refinement.reasoning =
        "UI element not found, may need to open different editor";
  } else if (error_lower.find("wrong") != std::string::npos ||
             error_lower.find("incorrect") != std::string::npos) {
    refinement.needs_retry = true;
    refinement.reasoning =
        "Action executed on wrong element, adjusting parameters";

    // Try to extract corrected parameters from suggestions
    for (const auto& suggestion : analysis.suggestions) {
      // Parse suggestions for parameter corrections
      // e.g., "Try position (6, 8) instead"
      if (suggestion.find("position") != std::string::npos) {
        // Extract coordinates
        size_t pos = suggestion.find('(');
        if (pos != std::string::npos) {
          size_t end = suggestion.find(')', pos);
          if (end != std::string::npos) {
            std::string coords = suggestion.substr(pos + 1, end - pos - 1);
            std::vector<std::string> parts = absl::StrSplit(coords, ',');
            if (parts.size() == 2) {
              refinement.adjusted_parameters["x"] =
                  std::string(absl::StripAsciiWhitespace(parts[0]));
              refinement.adjusted_parameters["y"] =
                  std::string(absl::StripAsciiWhitespace(parts[1]));
            }
          }
        }
      }
    }
  } else {
    refinement.needs_retry = true;
    refinement.reasoning = "Generic failure, will retry with same parameters";
  }

  return refinement;
}

absl::StatusOr<std::map<std::string, std::string>>
VisionActionRefiner::LocateUIElement(
    const std::filesystem::path& screenshot_path,
    const std::string& element_name) {
  std::string prompt = BuildElementLocationPrompt(element_name);

  auto response = gemini_service_->GenerateMultimodalResponse(
      screenshot_path.string(), prompt);

  if (!response.ok()) {
    return response.status();
  }

  std::map<std::string, std::string> location;

  // Parse location from response
  // Expected format: "The element is located at position (X, Y)"
  // or "The element is in the top-right corner"
  std::string text = response->text_response;
  std::transform(text.begin(), text.end(), text.begin(), ::tolower);

  if (text.find("not found") != std::string::npos ||
      text.find("not visible") != std::string::npos) {
    location["found"] = "false";
    location["description"] = response->text_response;
  } else {
    location["found"] = "true";
    location["description"] = response->text_response;

    // Try to extract coordinates
    size_t pos = text.find('(');
    if (pos != std::string::npos) {
      size_t end = text.find(')', pos);
      if (end != std::string::npos) {
        std::string coords = text.substr(pos + 1, end - pos - 1);
        std::vector<std::string> parts = absl::StrSplit(coords, ',');
        if (parts.size() == 2) {
          location["x"] = std::string(absl::StripAsciiWhitespace(parts[0]));
          location["y"] = std::string(absl::StripAsciiWhitespace(parts[1]));
        }
      }
    }
  }

  return location;
}

absl::StatusOr<std::vector<std::string>>
VisionActionRefiner::ExtractVisibleWidgets(
    const std::filesystem::path& screenshot_path) {
  std::string prompt = BuildWidgetExtractionPrompt();

  auto response = gemini_service_->GenerateMultimodalResponse(
      screenshot_path.string(), prompt);

  if (!response.ok()) {
    return response.status();
  }

  // Parse widget list from response
  std::vector<std::string> widgets;
  std::stringstream ss(response->text_response);
  std::string line;

  while (std::getline(ss, line)) {
    // Skip empty lines
    if (line.empty() ||
        line.find_first_not_of(" \t\n\r") == std::string::npos) {
      continue;
    }

    // Remove list markers (-, *, 1., etc.)
    size_t start = 0;
    if (line[0] == '-' || line[0] == '*') {
      start = 1;
    } else if (std::isdigit(line[0])) {
      start = line.find('.');
      if (start != std::string::npos) {
        start++;
      } else {
        start = 0;
      }
    }

    absl::string_view widget_view =
        absl::StripAsciiWhitespace(absl::string_view(line).substr(start));

    if (!widget_view.empty()) {
      widgets.push_back(std::string(widget_view));
    }
  }

  return widgets;
}

// Private helper methods

std::string VisionActionRefiner::BuildAnalysisPrompt(
    const std::string& context) {
  std::string base_prompt =
      "Analyze this screenshot of the YAZE ROM editor GUI. "
      "Identify all visible UI elements, windows, and widgets. "
      "List them in order of importance.";

  if (!context.empty()) {
    return absl::StrCat(base_prompt, "\n\nContext: ", context);
  }

  return base_prompt;
}

std::string VisionActionRefiner::BuildVerificationPrompt(
    const AIAction& action) {
  std::string action_desc = AIActionParser::ActionToString(action);

  return absl::StrCat(
      "This screenshot was taken after attempting to perform the following "
      "action: ",
      action_desc,
      "\n\nDid the action succeed? Look for visual evidence that the action "
      "completed. "
      "Respond with:\n"
      "SUCCESS: <description of what changed>\n"
      "or\n"
      "FAILURE: <description of what went wrong>");
}

std::string VisionActionRefiner::BuildElementLocationPrompt(
    const std::string& element_name) {
  return absl::StrCat("Locate the '", element_name,
                      "' UI element in this screenshot. "
                      "If found, describe its position (coordinates if "
                      "possible, or relative position). "
                      "If not found, state 'NOT FOUND'.");
}

std::string VisionActionRefiner::BuildWidgetExtractionPrompt() {
  return "List all visible UI widgets, buttons, windows, and interactive "
         "elements "
         "in this screenshot. Format as a bulleted list, one element per line.";
}

VisionAnalysisResult VisionActionRefiner::ParseAnalysisResponse(
    const std::string& response) {
  VisionAnalysisResult result;
  result.description = response;

  // Extract widgets from description
  // Look for common patterns like "- Button", "1. Window", etc.
  std::stringstream ss(response);
  std::string line;

  while (std::getline(ss, line)) {
    // Check if line contains a widget mention
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("button") != std::string::npos ||
        lower.find("window") != std::string::npos ||
        lower.find("panel") != std::string::npos ||
        lower.find("selector") != std::string::npos ||
        lower.find("editor") != std::string::npos) {
      result.widgets.push_back(std::string(absl::StripAsciiWhitespace(line)));
    }

    // Extract suggestions
    if (lower.find("suggest") != std::string::npos ||
        lower.find("try") != std::string::npos ||
        lower.find("could") != std::string::npos) {
      result.suggestions.push_back(
          std::string(absl::StripAsciiWhitespace(line)));
    }
  }

  return result;
}

VisionAnalysisResult VisionActionRefiner::ParseVerificationResponse(
    const std::string& response, const AIAction& action) {
  VisionAnalysisResult result;
  result.description = response;

  std::string response_upper = response;
  std::transform(response_upper.begin(), response_upper.end(),
                 response_upper.begin(), ::toupper);

  if (response_upper.find("SUCCESS") != std::string::npos) {
    result.action_successful = true;

    // Extract success description
    size_t pos = response_upper.find("SUCCESS:");
    if (pos != std::string::npos) {
      std::string desc = response.substr(pos + 8);
      result.description = std::string(absl::StripAsciiWhitespace(desc));
    }
  } else if (response_upper.find("FAILURE") != std::string::npos) {
    result.action_successful = false;

    // Extract failure description
    size_t pos = response_upper.find("FAILURE:");
    if (pos != std::string::npos) {
      std::string desc = response.substr(pos + 8);
      result.error_message = std::string(absl::StripAsciiWhitespace(desc));
    } else {
      result.error_message = "Action failed (details in description)";
    }
  } else {
    // Ambiguous response, assume failure
    result.action_successful = false;
    result.error_message =
        "Could not determine action success from vision analysis";
  }

  return result;
}

}  // namespace ai
}  // namespace cli
}  // namespace yaze
