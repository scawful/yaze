#include "cli/service/agent/advanced_routing.h"

#include <algorithm>
#include <sstream>
#include <map>
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

namespace yaze {
namespace cli {
namespace agent {

AdvancedRouter::RoutedResponse AdvancedRouter::RouteHexAnalysis(
    const std::vector<uint8_t>& data,
    uint32_t address,
    const RouteContext& ctx) {
  
  RoutedResponse response;
  
  // Infer data type
  std::string data_type = InferDataType(data);
  auto patterns = ExtractPatterns(data);
  
  // Summary for user
  response.summary = absl::StrFormat(
      "Address 0x%06X contains %s (%zu bytes)",
      address, data_type, data.size());
  
  // Detailed data for agent with structure hints
  std::ostringstream detailed;
  detailed << absl::StrFormat("Raw hex at 0x%06X:\n", address);
  for (size_t i = 0; i < data.size(); i += 16) {
    detailed << absl::StrFormat("%06X: ", address + i);
    for (size_t j = i; j < std::min(i + 16, data.size()); ++j) {
      detailed << absl::StrFormat("%02X ", data[j]);
    }
    detailed << " | ";
    for (size_t j = i; j < std::min(i + 16, data.size()); ++j) {
      char c = data[j];
      detailed << (isprint(c) ? c : '.');
    }
    detailed << "\n";
  }
  
  if (!patterns.empty()) {
    detailed << "\nDetected patterns:\n";
    for (const auto& pattern : patterns) {
      detailed << "- " << pattern << "\n";
    }
  }
  
  response.detailed_data = detailed.str();
  
  // Next steps based on data type
  if (data_type.find("sprite") != std::string::npos) {
    response.next_steps = "Use resource-list --type=sprite to identify sprite IDs";
  } else if (data_type.find("tile") != std::string::npos) {
    response.next_steps = "Use overworld-find-tile to see where this tile appears";
  } else if (data_type.find("palette") != std::string::npos) {
    response.next_steps = "Use palette-get-colors to see full palette";
  } else {
    response.next_steps = "Use hex-search to find similar patterns in ROM";
  }
  
  return response;
}

AdvancedRouter::RoutedResponse AdvancedRouter::RouteMapEdit(
    const std::string& edit_intent,
    const RouteContext& ctx) {
  
  RoutedResponse response;
  
  // Parse intent and generate action sequence
  response.summary = "Preparing map edit operation";
  response.needs_approval = true;
  
  // Generate GUI automation steps
  response.gui_actions = {
    "Click(\"Overworld Editor\")",
    "Wait(500)",
    "Click(canvas, x, y)",
    "SelectTile(tile_id)",
    "Click(target_x, target_y)",
    "Wait(100)",
    "Screenshot(\"after_edit.png\")",
  };
  
  response.detailed_data = GenerateGUIScript(response.gui_actions);
  response.next_steps = "Review proposed changes, then approve or modify";
  
  return response;
}

AdvancedRouter::RoutedResponse AdvancedRouter::RoutePaletteAnalysis(
    const std::vector<uint16_t>& colors,
    const RouteContext& ctx) {
  
  RoutedResponse response;
  
  // Analyze color relationships
  int unique_colors = 0;
  std::map<uint16_t, int> color_counts;
  for (uint16_t c : colors) {
    color_counts[c]++;
  }
  unique_colors = color_counts.size();
  
  response.summary = absl::StrFormat(
      "Palette has %zu colors (%d unique)",
      colors.size(), unique_colors);
  
  // Detailed breakdown
  std::ostringstream detailed;
  detailed << "Color breakdown:\n";
  for (size_t i = 0; i < colors.size(); ++i) {
    uint16_t snes = colors[i];
    uint8_t r = (snes & 0x1F) << 3;
    uint8_t g = ((snes >> 5) & 0x1F) << 3;
    uint8_t b = ((snes >> 10) & 0x1F) << 3;
    detailed << absl::StrFormat("  [%zu] $%04X = #%02X%02X%02X\n", i, snes, r, g, b);
  }
  
  if (color_counts.size() < colors.size()) {
    detailed << "\nDuplicates found - optimization possible\n";
  }
  
  response.detailed_data = detailed.str();
  response.next_steps = "Use palette-set-color to modify colors";
  
  return response;
}

AdvancedRouter::RoutedResponse AdvancedRouter::SynthesizeMultiToolResponse(
    const std::vector<std::string>& tool_results,
    const RouteContext& ctx) {
  
  RoutedResponse response;
  
  // Combine results intelligently
  response.summary = absl::StrFormat("Analyzed %zu data sources", tool_results.size());
  response.detailed_data = absl::StrJoin(tool_results, "\n---\n");
  
  // Generate insights
  response.next_steps = "Analysis complete. " + ctx.user_intent;
  
  return response;
}

std::string AdvancedRouter::GenerateGUIScript(
    const std::vector<std::string>& actions) {
  std::ostringstream script;
  script << "# Generated GUI Automation Script\n";
  script << "test: \"Automated Edit\"\n";
  script << "steps:\n";
  for (const auto& action : actions) {
    script << "  - " << action << "\n";
  }
  return script.str();
}

std::string AdvancedRouter::InferDataType(const std::vector<uint8_t>& data) {
  if (data.size() == 8) return "tile16 data";
  if (data.size() % 3 == 0 && data.size() <= 48) return "sprite data";
  if (data.size() == 32) return "palette data (16 colors)";
  if (data.size() > 1000) return "compressed data block";
  return "unknown data";
}

std::vector<std::string> AdvancedRouter::ExtractPatterns(
    const std::vector<uint8_t>& data) {
  std::vector<std::string> patterns;
  
  // Check for repeating bytes
  if (data.size() > 2) {
    bool all_same = true;
    for (size_t i = 1; i < data.size(); ++i) {
      if (data[i] != data[0]) {
        all_same = false;
        break;
      }
    }
    if (all_same) {
      patterns.push_back(absl::StrFormat("Repeating byte: 0x%02X", data[0]));
    }
  }
  
  // Check for ascending/descending sequences
  if (data.size() > 3) {
    bool ascending = true, descending = true;
    for (size_t i = 1; i < data.size(); ++i) {
      if (data[i] != data[i-1] + 1) ascending = false;
      if (data[i] != data[i-1] - 1) descending = false;
    }
    if (ascending) patterns.push_back("Ascending sequence");
    if (descending) patterns.push_back("Descending sequence");
  }
  
  return patterns;
}

std::string AdvancedRouter::FormatForAgent(const std::string& raw_data) {
  // Format data for easy agent consumption
  return "```\n" + raw_data + "\n```";
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
