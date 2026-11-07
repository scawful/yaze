#include "cli/handlers/tools/overworld_validate_commands.h"

#include <iostream>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "cli/handlers/tools/diagnostic_types.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_allocation.h"
#include "zelda3/overworld/overworld_terrain_io.h"

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
    const zelda3::Overworld& overworld,
    const zelda3::OverworldTerrainPointerTables& tables, int map_id,
    bool include_tail) {
  MapValidationResult result{};
  result.map_id = map_id;

  // Skip tail maps unless explicitly requested
  if (!include_tail && map_id >= zelda3::kSpecialWorldMapIdStart + 0x20) {
    result.skipped = true;
    result.error = "tail disabled";
    return result;
  }
  // --include-tail requests inspection, not an increase in storage capacity.
  if (map_id >= tables.map_count) {
    result.skipped = true;
    result.error = "tail not materialized";
    return result;
  }
  const size_t offset = static_cast<size_t>(map_id) * 3;
  const size_t rom_size = overworld.rom()->size();
  if (rom_size < 3 ||
      static_cast<size_t>(tables.low_table_pc) + offset > rom_size - 3 ||
      static_cast<size_t>(tables.high_table_pc) + offset > rom_size - 3) {
    result.error = "pointer table out of bounds";
    return result;
  }

  auto read_ptr = [&](uint32_t base) -> uint32_t {
    uint8_t byte0 = overworld.rom()->data()[base + (3 * map_id)];
    uint8_t byte1 = overworld.rom()->data()[base + (3 * map_id) + 1];
    uint8_t byte2 = overworld.rom()->data()[base + (3 * map_id) + 2];
    return (byte2 << 16) | (byte1 << 8) | byte0;
  };

  result.snes_low = read_ptr(tables.low_table_pc);
  result.snes_high = read_ptr(tables.high_table_pc);
  result.pc_low = SnesToPcLoRom(result.snes_low);
  result.pc_high = SnesToPcLoRom(result.snes_high);

  // Basic bounds check
  const auto valid_pointer = [&](uint32_t snes, uint32_t pc) {
    const auto bank = snes >> 16;
    return (snes & 0x8000) != 0 && bank != 0x7E && bank != 0x7F &&
           pc < rom_size;
  };
  result.pointers_valid = valid_pointer(result.snes_low, result.pc_low) &&
                          valid_pointer(result.snes_high, result.pc_high);

  auto try_decompress = [&](uint32_t pc_addr) -> bool {
    if (pc_addr >= overworld.rom()->size()) {
      return false;
    }
    const auto scan =
        zelda3::ScanMapStream(overworld.rom()->data(), rom_size, pc_addr, 256);
    return scan.error.empty() && scan.terminated && scan.decoded_bytes == 256;
  };

  if (result.pointers_valid) {
    result.decompress_low_ok = try_decompress(result.pc_low);
    result.decompress_high_ok = try_decompress(result.pc_high);
  }

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
};

Tile16ValidationResult ValidateTile16Region(const Rom* rom) {
  Tile16ValidationResult result;

  // Check if ROM uses expanded tile16
  uint8_t expanded_flag = rom->data()[kMap16ExpandedFlagPos];
  result.uses_expanded = (expanded_flag != 0x0F);

  if (!result.uses_expanded) {
    return result;
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
  const auto& layout = overworld.resolved_layout();
  zelda3::OverworldTerrainPointerTables tables{
      static_cast<uint32_t>(layout.terrain_high_table_pc.value),
      static_cast<uint32_t>(layout.terrain_low_table_pc.value),
      layout.materialized_model_slots};
  if (rom::IsNativeSpecialOverworldLayout(layout.lyra_layout)) {
    ASSIGN_OR_RETURN(const auto storage,
                     zelda3::ResolveNativeOverworldTerrainStorage(*rom));
    tables = {storage.high_table_pc, storage.low_table_pc, storage.map_count};
  }

  // Validate maps
  int max_map = include_tail ? 0xC0 : zelda3::kNumOverworldMaps;
  std::vector<MapValidationResult> results;
  results.reserve(max_map);

  ValidationSummary summary;
  summary.total_maps = max_map;

  for (int i = 0; i < max_map; ++i) {
    ASSIGN_OR_RETURN(auto result,
                     ValidateMapPointers(overworld, tables, i, include_tail));
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

  int baseline_maps_checked = 0;
  int baseline_mismatches = 0;
  const auto baseline_path = parser.GetString("baseline");
  if (baseline_path.has_value()) {
    Rom baseline_rom;
    RETURN_IF_ERROR(baseline_rom.LoadFromFile(*baseline_path));
    zelda3::Overworld baseline(&baseline_rom);
    RETURN_IF_ERROR(baseline.Load(&baseline_rom));
    if (baseline.materialized_map_count() != zelda3::kNumOverworldMaps) {
      return absl::InvalidArgumentError(
          "Overworld validation baseline must contain 160 materialized maps");
    }
    // Compare physical Tile16 cells, after each model resolves its own pointer
    // tables and shared Tile32 dictionary. Compressed-byte equality cannot prove
    // that repacking shared definitions preserved the original terrain.
    for (int map = 0; map < zelda3::kNumOverworldMaps; ++map) {
      const int world = map / 64;
      const int left = (map % 8) * 32;
      const int top = ((map % 64) / 8) * 32;
      const auto& before = baseline.GetMapTiles(world);
      const auto& after = overworld.GetMapTiles(world);
      if (before.size() < static_cast<size_t>(left + 32) ||
          after.size() < static_cast<size_t>(left + 32)) {
        return absl::DataLossError("Overworld validation grid is incomplete");
      }
      for (int x = left; x < left + 32; ++x) {
        if (before[x].size() < static_cast<size_t>(top + 32) ||
            after[x].size() < static_cast<size_t>(top + 32)) {
          return absl::DataLossError("Overworld validation grid is incomplete");
        }
        for (int y = top; y < top + 32; ++y) {
          baseline_mismatches += before[x][y] != after[x][y];
        }
      }
      ++baseline_maps_checked;
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
  if (baseline_path.has_value()) {
    formatter.AddField("baseline_maps_checked", baseline_maps_checked);
    formatter.AddField("baseline_mismatches", baseline_mismatches);
  }

  // Tile16 validation
  if (check_tile16) {
    auto tile16_result = ValidateTile16Region(rom);
    formatter.AddField("tile16_uses_expanded", tile16_result.uses_expanded);

    if (tile16_result.uses_expanded) {
      formatter.AddField("tile16_suspicious_count",
                         tile16_result.suspicious_count);
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

  if (baseline_mismatches != 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Overworld baseline terrain changed in %d original Tile16 cells",
        baseline_mismatches));
  }
  return absl::OkStatus();
}

}  // namespace yaze::cli
