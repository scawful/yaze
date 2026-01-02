#include "cli/handlers/tools/overworld_validate_commands.h"

#include <iostream>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/util/compression.h"
#include "cli/handlers/tools/diagnostic_types.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::cli {

namespace {

// =============================================================================
// Map Validation Result
// =============================================================================

struct MapValidationResult {
  int map_id = 0;
  bool pointers_valid = false;
  bool decompress_low_ok = false;
  bool decompress_high_ok = false;
  uint32_t snes_low = 0;
  uint32_t snes_high = 0;
  uint32_t pc_low = 0;
  uint32_t pc_high = 0;
  std::string error;
  bool skipped = false;

  std::string FormatJson() const {
    return absl::StrFormat(
        R"({"map_id":%d,"snes_low":"0x%06X","snes_high":"0x%06X",)"
        R"("pc_low":"0x%06X","pc_high":"0x%06X","pointers_valid":%s,)"
        R"("decomp_low":%s,"decomp_high":%s,"error":"%s","skipped":%s})",
        map_id, snes_low, snes_high, pc_low, pc_high,
        pointers_valid ? "true" : "false", decompress_low_ok ? "true" : "false",
        decompress_high_ok ? "true" : "false", error,
        skipped ? "true" : "false");
  }

  std::string FormatText() const {
    if (skipped) {
      return absl::StrFormat("map 0x%02X: skipped (%s)", map_id, error);
    }
    std::string status =
        (pointers_valid && decompress_low_ok && decompress_high_ok) ? "OK"
                                                                    : "FAIL";
    return absl::StrFormat(
        "map 0x%02X: %s snes_low=0x%06X snes_high=0x%06X ptr=%s dec_low=%s "
        "dec_high=%s %s",
        map_id, status, snes_low, snes_high, pointers_valid ? "OK" : "BAD",
        decompress_low_ok ? "OK" : "BAD", decompress_high_ok ? "OK" : "BAD",
        error);
  }

  bool IsValid() const {
    return !skipped && pointers_valid && decompress_low_ok &&
           decompress_high_ok;
  }
};

struct ValidationSummary {
  int total_maps = 0;
  int valid_maps = 0;
  int invalid_maps = 0;
  int skipped_maps = 0;
  int pointer_failures = 0;
  int decompress_failures = 0;
};

// =============================================================================
// Address Conversion
// =============================================================================

uint32_t SnesToPcLoRom(uint32_t snes_addr) {
  return ((snes_addr & 0x7F0000) >> 1) | (snes_addr & 0x7FFF);
}

// =============================================================================
// Validation Logic
// =============================================================================

absl::StatusOr<MapValidationResult> ValidateMapPointers(
    const zelda3::Overworld& overworld, int map_id, bool include_tail) {
  MapValidationResult result{};
  result.map_id = map_id;

  // Skip tail maps unless explicitly requested
  if (!include_tail && map_id >= zelda3::kSpecialWorldMapIdStart + 0x20) {
    result.skipped = true;
    result.error = "tail disabled";
    return result;
  }

  const auto ptr_low_base =
      overworld.version_constants().kCompressedAllMap32PointersLow;
  const auto ptr_high_base =
      overworld.version_constants().kCompressedAllMap32PointersHigh;

  auto read_ptr = [&](uint32_t base) -> uint32_t {
    uint8_t byte0 = overworld.rom()->data()[base + (3 * map_id)];
    uint8_t byte1 = overworld.rom()->data()[base + (3 * map_id) + 1];
    uint8_t byte2 = overworld.rom()->data()[base + (3 * map_id) + 2];
    return (byte2 << 16) | (byte1 << 8) | byte0;
  };

  result.snes_low = read_ptr(ptr_low_base);
  result.snes_high = read_ptr(ptr_high_base);
  result.pc_low = SnesToPcLoRom(result.snes_low);
  result.pc_high = SnesToPcLoRom(result.snes_high);

  // Basic bounds check
  result.pointers_valid = result.pc_low < overworld.rom()->size() &&
                          result.pc_high < overworld.rom()->size();

  auto try_decompress = [&](uint32_t pc_addr) -> bool {
    if (pc_addr >= overworld.rom()->size()) {
      return false;
    }
    int size = 0;
    auto buf =
        gfx::HyruleMagicDecompress(overworld.rom()->data() + pc_addr, &size, 1);
    return !buf.empty();
  };

  result.decompress_low_ok = try_decompress(result.pc_low);
  result.decompress_high_ok = try_decompress(result.pc_high);

  if (!result.pointers_valid) {
    result.error = "pointer out of bounds";
  } else if (!result.decompress_low_ok || !result.decompress_high_ok) {
    result.error = "decompression failed";
  }

  return result;
}

// =============================================================================
// Tile16 Validation
// =============================================================================

struct Tile16ValidationResult {
  bool uses_expanded = false;
  int suspicious_count = 0;
  std::vector<uint32_t> problem_addresses;
};

Tile16ValidationResult ValidateTile16Region(const Rom* rom) {
  Tile16ValidationResult result;

  // Check if ROM uses expanded tile16
  uint8_t expanded_flag = rom->data()[kMap16ExpandedFlagPos];
  result.uses_expanded = (expanded_flag != 0x0F);

  if (!result.uses_expanded) {
    return result;
  }

  // Check known problem addresses
  for (uint32_t addr : kProblemAddresses) {
    if (addr >= kMap16TilesExpanded && addr < kMap16TilesExpandedEnd) {
      uint8_t sample[16] = {0};
      for (int i = 0; i < 16 && addr + i < rom->size(); ++i) {
        sample[i] = rom->data()[addr + i];
      }

      bool looks_valid = true;
      for (int i = 0; i < 16; i += 2) {
        uint16_t val = sample[i] | (sample[i + 1] << 8);
        if ((val & 0x1F00) != 0 && (val & 0xE000) == 0) {
          looks_valid = false;
        }
      }

      if (!looks_valid) {
        result.problem_addresses.push_back(addr);
      }
    }
  }

  // Count suspicious tiles in expansion region
  for (int tile = kNumTile16Vanilla; tile < kNumTile16Expanded; ++tile) {
    uint32_t addr = kMap16TilesExpanded + (tile * 8);
    if (addr + 8 > rom->size()) {
      break;
    }

    bool all_same = true;
    uint16_t first_val = rom->data()[addr] | (rom->data()[addr + 1] << 8);
    for (int i = 1; i < 4; ++i) {
      uint16_t val =
          rom->data()[addr + i * 2] | (rom->data()[addr + i * 2 + 1] << 8);
      if (val != first_val) {
        all_same = false;
        break;
      }
    }

    if (all_same && first_val != 0x0000 && first_val != 0xFFFF) {
      result.suspicious_count++;
    }
  }

  return result;
}

}  // namespace

absl::Status OverworldValidateCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  bool include_tail = parser.HasFlag("include-tail");
  bool check_tile16 = parser.HasFlag("check-tile16");
  bool verbose = parser.HasFlag("verbose");
  bool is_json = formatter.IsJson();

  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));

  // Validate maps
  int max_map = include_tail ? 0xC0 : zelda3::kNumOverworldMaps;
  std::vector<MapValidationResult> results;
  results.reserve(max_map);

  ValidationSummary summary;
  summary.total_maps = max_map;

  for (int i = 0; i < max_map; ++i) {
    ASSIGN_OR_RETURN(auto result,
                     ValidateMapPointers(overworld, i, include_tail));
    results.push_back(result);

    if (result.skipped) {
      summary.skipped_maps++;
    } else if (result.IsValid()) {
      summary.valid_maps++;
    } else {
      summary.invalid_maps++;
      if (!result.pointers_valid) {
        summary.pointer_failures++;
      }
      if (!result.decompress_low_ok || !result.decompress_high_ok) {
        summary.decompress_failures++;
      }
    }
  }

  // Output maps array
  formatter.BeginArray("maps");
  for (const auto& result : results) {
    if (is_json) {
      formatter.AddArrayItem(result.FormatJson());
    } else if (verbose || !result.IsValid()) {
      // In text mode, only show failures unless verbose
      formatter.AddArrayItem(result.FormatText());
    }
  }
  formatter.EndArray();

  // Output summary
  formatter.AddField("total_maps", summary.total_maps);
  formatter.AddField("valid_maps", summary.valid_maps);
  formatter.AddField("invalid_maps", summary.invalid_maps);
  formatter.AddField("skipped_maps", summary.skipped_maps);
  formatter.AddField("pointer_failures", summary.pointer_failures);
  formatter.AddField("decompress_failures", summary.decompress_failures);

  // Tile16 validation
  if (check_tile16) {
    auto tile16_result = ValidateTile16Region(rom);
    formatter.AddField("tile16_uses_expanded", tile16_result.uses_expanded);

    if (tile16_result.uses_expanded) {
      formatter.AddField(
          "tile16_problem_addresses",
          static_cast<int>(tile16_result.problem_addresses.size()));
      formatter.AddField("tile16_suspicious_count",
                         tile16_result.suspicious_count);

      if (!is_json && !tile16_result.problem_addresses.empty()) {
        std::cout << "\n=== Tile16 Problems ===\n";
        for (uint32_t addr : tile16_result.problem_addresses) {
          int tile_idx = (addr - kMap16TilesExpanded) / 8;
          std::cout << absl::StrFormat("  0x%06X (tile16 #%d): SUSPICIOUS\n",
                                       addr, tile_idx);
        }
      }
    }
  }

  // Text mode summary
  if (!is_json) {
    std::cout << "\n=== Validation Summary ===\n";
    std::cout << absl::StrFormat("  Total maps: %d\n", summary.total_maps);
    std::cout << absl::StrFormat("  Valid: %d\n", summary.valid_maps);
    std::cout << absl::StrFormat("  Invalid: %d\n", summary.invalid_maps);
    std::cout << absl::StrFormat("  Skipped: %d\n", summary.skipped_maps);
    if (summary.invalid_maps > 0) {
      std::cout << absl::StrFormat("  Pointer failures: %d\n",
                                   summary.pointer_failures);
      std::cout << absl::StrFormat("  Decompress failures: %d\n",
                                   summary.decompress_failures);
    }
  }

  return absl::OkStatus();
}

}  // namespace yaze::cli
