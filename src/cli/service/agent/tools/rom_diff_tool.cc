/**
 * @file rom_diff_tool.cc
 * @brief Implementation of ROM comparison and diff analysis tools
 */

#include "cli/service/agent/tools/rom_diff_tool.h"

#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

#include "absl/strings/str_format.h"
#include "rom/rom.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

// Known memory regions for semantic categorization
struct MemoryRegion {
  uint32_t start;
  uint32_t end;
  const char* category;
  const char* description;
};

static const std::vector<MemoryRegion> kKnownRegions = {
    // Graphics
    {0x000000, 0x00FFFF, "graphics", "Compressed graphics"},
    {0x080000, 0x08FFFF, "graphics", "More graphics data"},

    // Overworld
    {0x020000, 0x027FFF, "overworld", "Overworld tilemap data"},
    {0x0A8000, 0x0AFFFF, "overworld", "Overworld map data"},

    // Dungeon
    {0x028000, 0x02FFFF, "dungeon", "Dungeon room data"},
    {0x0B0000, 0x0B7FFF, "dungeon", "Dungeon tileset data"},

    // Sprites
    {0x09C800, 0x09DFFF, "sprites", "Sprite graphics"},
    {0x0D8000, 0x0DFFFF, "sprites", "More sprite data"},

    // Palettes
    {0x0DD218, 0x0DD4FF, "palettes", "Main palettes"},
    {0x0DE000, 0x0DEFFF, "palettes", "Sprite palettes"},

    // Text/Messages
    {0x0E0000, 0x0E7FFF, "messages", "Text and messages"},

    // Code
    {0x008000, 0x00FFFF, "code", "Bank $00 code"},
    {0x018000, 0x01FFFF, "code", "Bank $03 code"},

    // Audio
    {0x0C0000, 0x0CFFFF, "audio", "Music/SPC data"},
};

// =============================================================================
// RomDiffTool
// =============================================================================

absl::Status RomDiffTool::Execute(Rom* /*rom*/,
                                  const resources::ArgumentParser& parser,
                                  resources::OutputFormatter& formatter) {
  std::string rom1_path = parser.GetString("rom1").value();
  std::string rom2_path = parser.GetString("rom2").value();
  bool semantic = parser.HasFlag("semantic");
  std::string format = parser.GetString("format").value_or("json");

  // Load both ROMs
  Rom rom1, rom2;
  auto status1 = rom1.LoadFromFile(rom1_path);
  if (!status1.ok()) {
    return absl::InvalidArgumentError("Failed to load ROM 1: " + rom1_path);
  }

  auto status2 = rom2.LoadFromFile(rom2_path);
  if (!status2.ok()) {
    return absl::InvalidArgumentError("Failed to load ROM 2: " + rom2_path);
  }

  // Compute differences
  auto summary = ComputeDiff(rom1.vector(), rom2.vector(), semantic);

  // Output
  if (format == "json") {
    std::cout << FormatAsJson(summary);
  } else {
    std::cout << FormatAsText(summary);
  }

  formatter.AddField("status", "complete");
  return absl::OkStatus();
}

DiffSummary RomDiffTool::ComputeDiff(const std::vector<uint8_t>& rom1,
                                     const std::vector<uint8_t>& rom2,
                                     bool semantic) {
  DiffSummary summary;

  size_t min_size = std::min(rom1.size(), rom2.size());
  size_t max_size = std::max(rom1.size(), rom2.size());

  // Find contiguous regions of differences
  bool in_diff = false;
  uint32_t diff_start = 0;
  std::vector<uint8_t> old_bytes, new_bytes;

  for (size_t i = 0; i < min_size; ++i) {
    if (rom1[i] != rom2[i]) {
      if (!in_diff) {
        in_diff = true;
        diff_start = static_cast<uint32_t>(i);
        old_bytes.clear();
        new_bytes.clear();
      }
      old_bytes.push_back(rom1[i]);
      new_bytes.push_back(rom2[i]);
      summary.total_bytes_changed++;
    } else if (in_diff) {
      // End of diff region
      RomDiff diff;
      diff.address = diff_start;
      diff.length = old_bytes.size();
      diff.old_value = old_bytes;
      diff.new_value = new_bytes;

      if (semantic) {
        diff.category = CategorizeAddress(diff_start);
        diff.description = DescribeChange(diff_start, old_bytes, new_bytes);
      } else {
        diff.category = "data";
        diff.description =
            absl::StrFormat("%zu bytes changed", old_bytes.size());
      }

      summary.diffs.push_back(diff);
      summary.changes_by_category[diff.category]++;
      in_diff = false;
    }
  }

  // Handle final diff region if we ended while in a diff
  if (in_diff) {
    RomDiff diff;
    diff.address = diff_start;
    diff.length = old_bytes.size();
    diff.old_value = old_bytes;
    diff.new_value = new_bytes;
    diff.category = semantic ? CategorizeAddress(diff_start) : "data";
    diff.description = DescribeChange(diff_start, old_bytes, new_bytes);
    summary.diffs.push_back(diff);
    summary.changes_by_category[diff.category]++;
  }

  // Handle size difference
  if (rom1.size() != rom2.size()) {
    summary.changes_by_category["size"] = 1;
    if (rom2.size() > rom1.size()) {
      summary.total_bytes_changed += (rom2.size() - rom1.size());
    }
  }

  summary.num_regions = summary.diffs.size();
  return summary;
}

std::string RomDiffTool::CategorizeAddress(uint32_t address) const {
  for (const auto& region : kKnownRegions) {
    if (address >= region.start && address <= region.end) {
      return region.category;
    }
  }
  return "unknown";
}

std::string RomDiffTool::DescribeChange(
    uint32_t address, const std::vector<uint8_t>& old_val,
    const std::vector<uint8_t>& new_val) const {
  std::string category = CategorizeAddress(address);

  if (category == "graphics") {
    return absl::StrFormat("Graphics data changed (%zu bytes)", old_val.size());
  } else if (category == "palettes") {
    return absl::StrFormat("Palette data changed (%zu bytes)", old_val.size());
  } else if (category == "code") {
    return absl::StrFormat("Code modified (%zu bytes)", old_val.size());
  } else if (category == "sprites") {
    return absl::StrFormat("Sprite data changed (%zu bytes)", old_val.size());
  } else if (category == "messages") {
    return absl::StrFormat("Message/text data changed (%zu bytes)",
                           old_val.size());
  } else if (category == "dungeon") {
    return absl::StrFormat("Dungeon data changed (%zu bytes)", old_val.size());
  } else if (category == "overworld") {
    return absl::StrFormat("Overworld data changed (%zu bytes)",
                           old_val.size());
  }

  return absl::StrFormat("%zu bytes changed", old_val.size());
}

std::string RomDiffTool::FormatAsJson(const DiffSummary& summary) const {
  std::ostringstream json;

  json << "{\n";
  json << "  \"total_bytes_changed\": " << summary.total_bytes_changed << ",\n";
  json << "  \"num_regions\": " << summary.num_regions << ",\n";

  // Changes by category
  json << "  \"changes_by_category\": {";
  bool first = true;
  for (const auto& [cat, count] : summary.changes_by_category) {
    if (!first)
      json << ", ";
    json << "\"" << cat << "\": " << count;
    first = false;
  }
  json << "},\n";

  // Individual diffs (limit to first 50 for JSON output)
  json << "  \"diffs\": [\n";
  size_t max_diffs = std::min(size_t(50), summary.diffs.size());
  for (size_t i = 0; i < max_diffs; ++i) {
    const auto& diff = summary.diffs[i];
    json << "    {";
    json << "\"address\": \"" << absl::StrFormat("0x%06X", diff.address)
         << "\", ";
    json << "\"length\": " << diff.length << ", ";
    json << "\"category\": \"" << diff.category << "\", ";
    json << "\"description\": \"" << diff.description << "\"";
    json << "}";
    if (i < max_diffs - 1)
      json << ",";
    json << "\n";
  }

  if (summary.diffs.size() > 50) {
    json << "    // ... and " << (summary.diffs.size() - 50) << " more\n";
  }

  json << "  ]\n";
  json << "}\n";

  return json.str();
}

std::string RomDiffTool::FormatAsText(const DiffSummary& summary) const {
  std::ostringstream text;

  text << "ROM Comparison Summary\n";
  text << "======================\n\n";
  text << "Total bytes changed: " << summary.total_bytes_changed << "\n";
  text << "Number of diff regions: " << summary.num_regions << "\n\n";

  text << "Changes by category:\n";
  for (const auto& [cat, count] : summary.changes_by_category) {
    text << "  " << cat << ": " << count << " regions\n";
  }
  text << "\n";

  text << "Diff regions (first 20):\n";
  size_t max_diffs = std::min(size_t(20), summary.diffs.size());
  for (size_t i = 0; i < max_diffs; ++i) {
    const auto& diff = summary.diffs[i];
    text << absl::StrFormat("  %06X: %s (%zu bytes)\n", diff.address,
                            diff.description, diff.length);
  }

  if (summary.diffs.size() > 20) {
    text << "  ... and " << (summary.diffs.size() - 20) << " more\n";
  }

  return text.str();
}

// =============================================================================
// RomChangesTool
// =============================================================================

absl::Status RomChangesTool::Execute(Rom* /*rom*/,
                                     const resources::ArgumentParser& parser,
                                     resources::OutputFormatter& formatter) {
  std::string rom1_path = parser.GetString("rom1").value();
  std::string rom2_path = parser.GetString("rom2").value();
  std::string format = parser.GetString("format").value_or("json");

  // Load both ROMs
  Rom rom1, rom2;
  auto status1 = rom1.LoadFromFile(rom1_path);
  if (!status1.ok()) {
    return absl::InvalidArgumentError("Failed to load ROM 1: " + rom1_path);
  }

  auto status2 = rom2.LoadFromFile(rom2_path);
  if (!status2.ok()) {
    return absl::InvalidArgumentError("Failed to load ROM 2: " + rom2_path);
  }

  std::ostringstream output;

  if (format == "json") {
    output << "{\n";
    output << "  \"rom1\": {\"path\": \"" << rom1_path << "\", "
           << "\"size\": " << rom1.size() << ", "
           << "\"title\": \"" << rom1.title() << "\"},\n";
    output << "  \"rom2\": {\"path\": \"" << rom2_path << "\", "
           << "\"size\": " << rom2.size() << ", "
           << "\"title\": \"" << rom2.title() << "\"},\n";

    // Size comparison
    if (rom1.size() != rom2.size()) {
      output << "  \"size_change\": "
             << static_cast<int64_t>(rom2.size()) -
                    static_cast<int64_t>(rom1.size())
             << ",\n";
    }

    // Title comparison
    output << "  \"title_changed\": "
           << (rom1.title() != rom2.title() ? "true" : "false") << ",\n";

    // Basic change detection
    int total_diffs = 0;
    size_t min_size = std::min(rom1.size(), rom2.size());
    for (size_t i = 0; i < min_size; ++i) {
      if (rom1[i] != rom2[i])
        total_diffs++;
    }

    output << "  \"bytes_different\": " << total_diffs << ",\n";
    output << "  \"percent_changed\": "
           << absl::StrFormat("%.2f", (total_diffs * 100.0) / min_size) << "\n";
    output << "}\n";
  } else {
    output << "ROM Comparison: Changes Analysis\n";
    output << "================================\n\n";
    output << "ROM 1: " << rom1_path << " (" << rom1.size() << " bytes)\n";
    output << "ROM 2: " << rom2_path << " (" << rom2.size() << " bytes)\n\n";

    if (rom1.size() != rom2.size()) {
      output << "Size change: "
             << (static_cast<int64_t>(rom2.size()) -
                 static_cast<int64_t>(rom1.size()))
             << " bytes\n";
    }

    if (rom1.title() != rom2.title()) {
      output << "Title changed: '" << rom1.title() << "' -> '" << rom2.title()
             << "'\n";
    }

    int total_diffs = 0;
    size_t min_size = std::min(rom1.size(), rom2.size());
    for (size_t i = 0; i < min_size; ++i) {
      if (rom1[i] != rom2[i])
        total_diffs++;
    }

    output << "Bytes different: " << total_diffs << " ("
           << absl::StrFormat("%.2f%%", (total_diffs * 100.0) / min_size)
           << ")\n";
  }

  std::cout << output.str();
  formatter.AddField("status", "complete");
  return absl::OkStatus();
}

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
