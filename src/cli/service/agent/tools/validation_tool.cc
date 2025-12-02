/**
 * @file validation_tool.cc
 * @brief Implementation of ROM validation and integrity checking tools
 */

#include "cli/service/agent/tools/validation_tool.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "rom/rom.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

// =============================================================================
// ValidationToolBase
// =============================================================================

std::string ValidationToolBase::FormatIssuesAsJson(
    const std::vector<ValidationIssue>& issues) const {
  std::ostringstream json;
  json << "{\"issues\": [\n";

  for (size_t i = 0; i < issues.size(); ++i) {
    const auto& issue = issues[i];
    json << "  {\"severity\": \"" << issue.SeverityString() << "\", "
         << "\"category\": \"" << issue.category << "\", "
         << "\"message\": \"" << issue.message << "\"";
    if (issue.address != 0) {
      json << absl::StrFormat(", \"address\": \"0x%06X\"", issue.address);
    }
    json << "}";
    if (i < issues.size() - 1) json << ",";
    json << "\n";
  }

  json << "], ";

  // Summary
  int errors = 0, warnings = 0, info = 0;
  for (const auto& issue : issues) {
    switch (issue.severity) {
      case ValidationIssue::Severity::kError:
      case ValidationIssue::Severity::kCritical:
        errors++;
        break;
      case ValidationIssue::Severity::kWarning:
        warnings++;
        break;
      case ValidationIssue::Severity::kInfo:
        info++;
        break;
    }
  }
  json << "\"summary\": {\"errors\": " << errors << ", \"warnings\": " << warnings
       << ", \"info\": " << info << "}}\n";

  return json.str();
}

std::string ValidationToolBase::FormatIssuesAsText(
    const std::vector<ValidationIssue>& issues) const {
  std::ostringstream text;

  for (const auto& issue : issues) {
    std::string prefix;
    switch (issue.severity) {
      case ValidationIssue::Severity::kInfo:
        prefix = "[INFO]";
        break;
      case ValidationIssue::Severity::kWarning:
        prefix = "[WARN]";
        break;
      case ValidationIssue::Severity::kError:
        prefix = "[ERROR]";
        break;
      case ValidationIssue::Severity::kCritical:
        prefix = "[CRITICAL]";
        break;
    }

    text << prefix << " [" << issue.category << "] " << issue.message;
    if (issue.address != 0) {
      text << absl::StrFormat(" (at 0x%06X)", issue.address);
    }
    text << "\n";
  }

  // Summary
  int errors = 0, warnings = 0, info = 0;
  for (const auto& issue : issues) {
    switch (issue.severity) {
      case ValidationIssue::Severity::kError:
      case ValidationIssue::Severity::kCritical:
        errors++;
        break;
      case ValidationIssue::Severity::kWarning:
        warnings++;
        break;
      case ValidationIssue::Severity::kInfo:
        info++;
        break;
    }
  }

  text << "\nSummary: " << errors << " errors, " << warnings << " warnings, "
       << info << " info\n";

  return text.str();
}

// =============================================================================
// RomValidateTool
// =============================================================================

absl::Status RomValidateTool::Execute(Rom* rom,
                                      const resources::ArgumentParser& parser,
                                      resources::OutputFormatter& formatter) {
  std::vector<ValidationIssue> issues;

  // Run all header validations
  auto header_issues = ValidateHeader(rom);
  issues.insert(issues.end(), header_issues.begin(), header_issues.end());

  auto checksum_issues = ValidateChecksum(rom);
  issues.insert(issues.end(), checksum_issues.begin(), checksum_issues.end());

  auto size_issues = ValidateSize(rom);
  issues.insert(issues.end(), size_issues.begin(), size_issues.end());

  std::string format = parser.GetString("format").value_or("json");
  if (format == "json") {
    std::cout << FormatIssuesAsJson(issues);
  } else {
    std::cout << FormatIssuesAsText(issues);
  }

  formatter.AddField("status", "complete");
  return absl::OkStatus();
}

std::vector<ValidationIssue> RomValidateTool::ValidateHeader(Rom* rom) {
  std::vector<ValidationIssue> issues;

  // Check ROM title
  std::string title = rom->title();
  if (title.empty()) {
    issues.push_back({ValidationIssue::Severity::kWarning, "header",
                      "ROM title is empty", 0x7FC0});
  } else {
    issues.push_back({ValidationIssue::Severity::kInfo, "header",
                      "ROM title: " + title, 0x7FC0});
  }

  // Check map mode
  auto map_mode = rom->ReadByte(0x7FD5);
  if (map_mode.ok()) {
    uint8_t mode = *map_mode;
    if ((mode & 0x01) == 0) {
      issues.push_back({ValidationIssue::Severity::kInfo, "header",
                        "ROM is LoROM mapping", 0x7FD5});
    } else {
      issues.push_back({ValidationIssue::Severity::kInfo, "header",
                        "ROM is HiROM mapping", 0x7FD5});
    }
  }

  // Check ROM type
  auto rom_type = rom->ReadByte(0x7FD6);
  if (rom_type.ok()) {
    uint8_t type = *rom_type;
    if (type == 0x00) {
      issues.push_back(
          {ValidationIssue::Severity::kInfo, "header", "ROM only", 0x7FD6});
    } else if (type == 0x02) {
      issues.push_back({ValidationIssue::Severity::kInfo, "header",
                        "ROM + SRAM", 0x7FD6});
    }
  }

  return issues;
}

std::vector<ValidationIssue> RomValidateTool::ValidateChecksum(Rom* rom) {
  std::vector<ValidationIssue> issues;

  auto checksum = rom->ReadWord(0x7FDC);
  auto complement = rom->ReadWord(0x7FDE);

  if (checksum.ok() && complement.ok()) {
    uint16_t sum = *checksum;
    uint16_t comp = *complement;

    // Checksum + Complement should equal 0xFFFF
    if ((sum + comp) == 0xFFFF) {
      issues.push_back({ValidationIssue::Severity::kInfo, "checksum",
                        absl::StrFormat("Header checksum valid (0x%04X)", sum),
                        0x7FDC});
    } else {
      issues.push_back(
          {ValidationIssue::Severity::kError, "checksum",
           absl::StrFormat(
               "Header checksum invalid (0x%04X + 0x%04X != 0xFFFF)", sum,
               comp),
           0x7FDC});
    }

    // Calculate actual checksum
    uint32_t actual_sum = 0;
    for (size_t i = 0; i < rom->size(); ++i) {
      actual_sum += (*rom)[i];
    }
    actual_sum = (actual_sum & 0xFFFF);

    if (static_cast<uint16_t>(actual_sum) == sum) {
      issues.push_back({ValidationIssue::Severity::kInfo, "checksum",
                        "Calculated checksum matches header", 0});
    } else {
      issues.push_back(
          {ValidationIssue::Severity::kWarning, "checksum",
           absl::StrFormat(
               "Calculated checksum (0x%04X) differs from header (0x%04X)",
               actual_sum, sum),
           0});
    }
  } else {
    issues.push_back({ValidationIssue::Severity::kError, "checksum",
                      "Failed to read checksum from header", 0x7FDC});
  }

  return issues;
}

std::vector<ValidationIssue> RomValidateTool::ValidateSize(Rom* rom) {
  std::vector<ValidationIssue> issues;

  size_t size = rom->size();

  // Check for common sizes
  if (size == 0x100000) {  // 1MB
    issues.push_back({ValidationIssue::Severity::kInfo, "size",
                      "ROM size: 1MB (standard)", 0});
  } else if (size == 0x200000) {  // 2MB
    issues.push_back({ValidationIssue::Severity::kInfo, "size",
                      "ROM size: 2MB (expanded)", 0});
  } else if (size == 0x400000) {  // 4MB
    issues.push_back({ValidationIssue::Severity::kInfo, "size",
                      "ROM size: 4MB (fully expanded)", 0});
  } else {
    issues.push_back(
        {ValidationIssue::Severity::kWarning, "size",
         absl::StrFormat("Unusual ROM size: %zu bytes", size), 0});
  }

  // Check for header presence
  if ((size & 0x200) != 0) {
    issues.push_back({ValidationIssue::Severity::kInfo, "size",
                      "ROM has 512-byte copier header", 0});
  }

  return issues;
}

// =============================================================================
// DataValidateTool
// =============================================================================

absl::Status DataValidateTool::Execute(Rom* rom,
                                       const resources::ArgumentParser& parser,
                                       resources::OutputFormatter& formatter) {
  std::vector<ValidationIssue> issues;
  std::string type = parser.GetString("type").value();

  if (type == "sprites" || type == "all") {
    auto sprite_issues = ValidateSprites(rom);
    issues.insert(issues.end(), sprite_issues.begin(), sprite_issues.end());
  }

  if (type == "tiles" || type == "all") {
    auto tile_issues = ValidateTiles(rom);
    issues.insert(issues.end(), tile_issues.begin(), tile_issues.end());
  }

  if (type == "palettes" || type == "all") {
    auto palette_issues = ValidatePalettes(rom);
    issues.insert(issues.end(), palette_issues.begin(), palette_issues.end());
  }

  if (type == "entrances" || type == "all") {
    auto entrance_issues = ValidateEntrances(rom);
    issues.insert(issues.end(), entrance_issues.begin(), entrance_issues.end());
  }

  std::string format = parser.GetString("format").value_or("json");
  if (format == "json") {
    std::cout << FormatIssuesAsJson(issues);
  } else {
    std::cout << FormatIssuesAsText(issues);
  }

  formatter.AddField("status", "complete");
  return absl::OkStatus();
}

std::vector<ValidationIssue> DataValidateTool::ValidateSprites(Rom* rom) {
  std::vector<ValidationIssue> issues;

  // Check overworld sprite data
  constexpr uint32_t kOverworldSpriteBase = 0x09C901;
  constexpr int kNumMaps = 64;  // Check first 64 maps

  int invalid_sprites = 0;
  for (int map = 0; map < kNumMaps; ++map) {
    auto ptr_low = rom->ReadByte(0x09C881 + map);
    auto ptr_high = rom->ReadByte(0x09C8C1 + map);
    if (ptr_low.ok() && ptr_high.ok()) {
      uint32_t addr = kOverworldSpriteBase + (*ptr_low | (*ptr_high << 8));
      if (addr >= rom->size()) {
        invalid_sprites++;
      }
    }
  }

  if (invalid_sprites > 0) {
    issues.push_back(
        {ValidationIssue::Severity::kError, "sprites",
         absl::StrFormat("%d overworld maps have invalid sprite pointers",
                         invalid_sprites),
         0});
  } else {
    issues.push_back({ValidationIssue::Severity::kInfo, "sprites",
                      "All overworld sprite pointers valid", 0});
  }

  return issues;
}

std::vector<ValidationIssue> DataValidateTool::ValidateTiles(Rom* rom) {
  std::vector<ValidationIssue> issues;

  // Basic tile data validation
  // Check that tile graphics pointers are valid
  constexpr uint32_t kTileGfxPtr = 0x00E800;

  auto gfx_ptr = rom->ReadWord(kTileGfxPtr);
  if (gfx_ptr.ok()) {
    issues.push_back({ValidationIssue::Severity::kInfo, "tiles",
                      "Tile graphics pointer accessible", kTileGfxPtr});
  } else {
    issues.push_back({ValidationIssue::Severity::kError, "tiles",
                      "Failed to read tile graphics pointer", kTileGfxPtr});
  }

  return issues;
}

std::vector<ValidationIssue> DataValidateTool::ValidatePalettes(Rom* rom) {
  std::vector<ValidationIssue> issues;

  // Check palette data integrity
  constexpr uint32_t kPaletteBase = 0x0DD218;
  constexpr int kNumPalettes = 8;
  constexpr int kPaletteSize = 32;  // 16 colors * 2 bytes

  int valid_palettes = 0;
  for (int i = 0; i < kNumPalettes; ++i) {
    uint32_t addr = kPaletteBase + (i * kPaletteSize);
    if (addr + kPaletteSize <= rom->size()) {
      valid_palettes++;
    }
  }

  if (valid_palettes == kNumPalettes) {
    issues.push_back({ValidationIssue::Severity::kInfo, "palettes",
                      "All main palettes accessible", kPaletteBase});
  } else {
    issues.push_back(
        {ValidationIssue::Severity::kError, "palettes",
         absl::StrFormat("Only %d/%d palettes accessible", valid_palettes,
                         kNumPalettes),
         kPaletteBase});
  }

  return issues;
}

std::vector<ValidationIssue> DataValidateTool::ValidateEntrances(Rom* rom) {
  std::vector<ValidationIssue> issues;

  // Check entrance data
  constexpr uint32_t kEntranceBase = 0x02C577;
  constexpr int kNumEntrances = 0x85;  // 133 entrances

  int valid_entrances = 0;
  for (int i = 0; i < kNumEntrances; ++i) {
    auto room_id = rom->ReadWord(kEntranceBase + (i * 2));
    if (room_id.ok()) {
      if (*room_id < 296) {  // Valid room IDs are 0-295
        valid_entrances++;
      }
    }
  }

  if (valid_entrances == kNumEntrances) {
    issues.push_back({ValidationIssue::Severity::kInfo, "entrances",
                      "All entrance room IDs valid", kEntranceBase});
  } else {
    issues.push_back(
        {ValidationIssue::Severity::kWarning, "entrances",
         absl::StrFormat("%d/%d entrances have valid room IDs",
                         valid_entrances, kNumEntrances),
         kEntranceBase});
  }

  return issues;
}

// =============================================================================
// PatchCheckTool
// =============================================================================

absl::Status PatchCheckTool::Execute(Rom* rom,
                                     const resources::ArgumentParser& parser,
                                     resources::OutputFormatter& formatter) {
  std::string patch_path = parser.GetString("patch").value();

  std::vector<ValidationIssue> issues;

  // Check free space availability
  auto space_issues = CheckFreeSpace(rom);
  issues.insert(issues.end(), space_issues.begin(), space_issues.end());

  // Check for hook conflicts
  auto hook_issues = CheckHooks(rom, patch_path);
  issues.insert(issues.end(), hook_issues.begin(), hook_issues.end());

  std::string format = parser.GetString("format").value_or("json");
  if (format == "json") {
    std::cout << FormatIssuesAsJson(issues);
  } else {
    std::cout << FormatIssuesAsText(issues);
  }

  formatter.AddField("status", "complete");
  return absl::OkStatus();
}

std::vector<ValidationIssue> PatchCheckTool::CheckFreeSpace(Rom* rom) {
  std::vector<ValidationIssue> issues;

  // Check common free space regions
  struct FreeSpaceRegion {
    uint32_t start;
    uint32_t end;
    const char* name;
  };

  std::vector<FreeSpaceRegion> regions = {
      {0x1F8000, 0x1FFFFF, "Bank $3F"},
      {0x0FFF00, 0x0FFFFF, "Bank $1F end"},
  };

  for (const auto& region : regions) {
    if (region.end > rom->size()) {
      issues.push_back({ValidationIssue::Severity::kInfo, "free_space",
                        absl::StrFormat("%s not available (ROM too small)",
                                        region.name),
                        region.start});
      continue;
    }

    // Check if region is mostly 0xFF (free space marker)
    int free_bytes = 0;
    for (uint32_t addr = region.start; addr < region.end; ++addr) {
      if ((*rom)[addr] == 0xFF || (*rom)[addr] == 0x00) {
        free_bytes++;
      }
    }

    int region_size = region.end - region.start;
    int free_percent = (free_bytes * 100) / region_size;

    if (free_percent > 80) {
      issues.push_back(
          {ValidationIssue::Severity::kInfo, "free_space",
           absl::StrFormat("%s: %d%% free (%d bytes)", region.name,
                           free_percent, free_bytes),
           region.start});
    } else {
      issues.push_back(
          {ValidationIssue::Severity::kWarning, "free_space",
           absl::StrFormat("%s: only %d%% free (%d bytes)", region.name,
                           free_percent, free_bytes),
           region.start});
    }
  }

  return issues;
}

std::vector<ValidationIssue> PatchCheckTool::CheckHooks(
    Rom* rom, const std::string& patch_path) {
  std::vector<ValidationIssue> issues;

  // Check if patch file exists
  std::ifstream patch_file(patch_path);
  if (!patch_file.good()) {
    issues.push_back({ValidationIssue::Severity::kError, "patch",
                      "Patch file not found: " + patch_path, 0});
    return issues;
  }

  // Check common hook locations for existing modifications
  struct HookLocation {
    uint32_t address;
    const char* name;
    uint8_t original_byte;
  };

  std::vector<HookLocation> hooks = {
      {0x008027, "Reset vector", 0x8C},
      {0x008040, "NMI vector", 0x5C},
      {0x0080B5, "IRQ vector", 0x8B},
  };

  for (const auto& hook : hooks) {
    if (hook.address < rom->size()) {
      auto byte = rom->ReadByte(hook.address);
      if (byte.ok() && *byte != hook.original_byte) {
        issues.push_back(
            {ValidationIssue::Severity::kWarning, "hooks",
             absl::StrFormat("%s already modified (0x%02X != 0x%02X)",
                             hook.name, *byte, hook.original_byte),
             hook.address});
      }
    }
  }

  issues.push_back({ValidationIssue::Severity::kInfo, "patch",
                    "Patch file exists: " + patch_path, 0});

  return issues;
}

// =============================================================================
// ValidateAllTool
// =============================================================================

absl::Status ValidateAllTool::Execute(Rom* rom,
                                      const resources::ArgumentParser& parser,
                                      resources::OutputFormatter& formatter) {
  std::vector<ValidationIssue> all_issues;
  bool strict = parser.HasFlag("strict");

  // Run ROM validation
  RomValidateTool rom_validator;
  auto header_issues = rom_validator.ValidateHeader(rom);
  all_issues.insert(all_issues.end(), header_issues.begin(),
                    header_issues.end());
  auto checksum_issues = rom_validator.ValidateChecksum(rom);
  all_issues.insert(all_issues.end(), checksum_issues.begin(),
                    checksum_issues.end());
  auto size_issues = rom_validator.ValidateSize(rom);
  all_issues.insert(all_issues.end(), size_issues.begin(), size_issues.end());

  // Run data validation
  DataValidateTool data_validator;
  auto sprite_issues = data_validator.ValidateSprites(rom);
  all_issues.insert(all_issues.end(), sprite_issues.begin(),
                    sprite_issues.end());
  auto tile_issues = data_validator.ValidateTiles(rom);
  all_issues.insert(all_issues.end(), tile_issues.begin(), tile_issues.end());
  auto palette_issues = data_validator.ValidatePalettes(rom);
  all_issues.insert(all_issues.end(), palette_issues.begin(),
                    palette_issues.end());
  auto entrance_issues = data_validator.ValidateEntrances(rom);
  all_issues.insert(all_issues.end(), entrance_issues.begin(),
                    entrance_issues.end());

  // Check for critical issues in strict mode
  if (strict) {
    for (const auto& issue : all_issues) {
      if (issue.severity == ValidationIssue::Severity::kCritical ||
          issue.severity == ValidationIssue::Severity::kError) {
        return absl::InvalidArgumentError(
            "Validation failed: " + issue.message);
      }
    }
  }

  std::string format = parser.GetString("format").value_or("json");
  if (format == "json") {
    std::cout << FormatIssuesAsJson(all_issues);
  } else {
    std::cout << FormatIssuesAsText(all_issues);
  }

  formatter.AddField("status", "complete");
  return absl::OkStatus();
}

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze

