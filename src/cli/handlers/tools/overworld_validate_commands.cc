#include "cli/handlers/tools/overworld_validate_commands.h"

#include <iostream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/util/compression.h"
#include "app/rom.h"
#include "util/log.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::cli {

namespace {

struct MapValidationResult {
  int map_id;
  bool pointers_valid;
  bool decompress_low_ok;
  bool decompress_high_ok;
  uint32_t snes_low;
  uint32_t snes_high;
  uint32_t pc_low;
  uint32_t pc_high;
  std::string error;
};

uint32_t PcToSnesLoRom(uint32_t pc) {
  uint32_t bank = (pc >> 15) & 0x7F;
  uint32_t addr = (pc & 0x7FFF) + 0x8000;
  return (bank << 16) | addr;
}

uint32_t SnesToPcLoRom(uint32_t snes) {
  return ((snes & 0x7F0000) >> 1) | (snes & 0x7FFF);
}

absl::StatusOr<MapValidationResult> ValidateMapPointers(
    const zelda3::Overworld& overworld, int map_id, bool include_tail) {
  MapValidationResult res{};
  res.map_id = map_id;
  auto version = zelda3::OverworldVersionHelper::GetVersion(*overworld.rom());

  // Skip tail maps unless explicitly requested
  if (!include_tail && map_id >= zelda3::kSpecialWorldMapIdStart + 0x20) {
    res.error = "skipped (tail disabled)";
    return res;
  }

  const auto ptr_low_base = overworld.rom()->version_constants().kCompressedAllMap32PointersLow;
  const auto ptr_high_base = overworld.rom()->version_constants().kCompressedAllMap32PointersHigh;

  auto read_ptr = [&](uint32_t base) -> uint32_t {
    uint8_t b0 = overworld.rom()->data()[base + (3 * map_id)];
    uint8_t b1 = overworld.rom()->data()[base + (3 * map_id) + 1];
    uint8_t b2 = overworld.rom()->data()[base + (3 * map_id) + 2];
    return (b2 << 16) | (b1 << 8) | b0;
  };

  res.snes_low = read_ptr(ptr_low_base);
  res.snes_high = read_ptr(ptr_high_base);
  res.pc_low = SnesToPcLoRom(res.snes_low);
  res.pc_high = SnesToPcLoRom(res.snes_high);

  // Basic bounds check
  res.pointers_valid = res.pc_low < overworld.rom()->size() &&
                       res.pc_high < overworld.rom()->size();

  auto try_decompress = [&](uint32_t pc) -> bool {
    if (pc >= overworld.rom()->size()) return false;
    int sz = 0;
    auto buf = gfx::HyruleMagicDecompress(overworld.rom()->data() + pc, &sz, 1);
    return !buf.empty();
  };

  res.decompress_low_ok = try_decompress(res.pc_low);
  res.decompress_high_ok = try_decompress(res.pc_high);

  if (!res.pointers_valid) {
    res.error = "pointer out of bounds";
  } else if (!res.decompress_low_ok || !res.decompress_high_ok) {
    res.error = "decompression failed";
  }

  return res;
}

void PrintResult(const MapValidationResult& r, bool json) {
  if (json) {
    std::cout << absl::StrFormat(
                     "{\"map\":%d,\"snes_low\":\"0x%06X\",\"snes_high\":\"0x%06X\"," \
                     "\"pc_low\":\"0x%06X\",\"pc_high\":\"0x%06X\",\"pointers_valid\":%s," \
                     "\"decomp_low\":%s,\"decomp_high\":%s,\"error\":\"%s\"}",
                     r.map_id, r.snes_low, r.snes_high, r.pc_low, r.pc_high,
                     r.pointers_valid ? "true" : "false",
                     r.decompress_low_ok ? "true" : "false",
                     r.decompress_high_ok ? "true" : "false",
                     r.error.c_str())
              << "\n";
  } else {
    std::cout << absl::StrFormat(
                     "map %02X: snes_low=0x%06X pc_low=0x%06X snes_high=0x%06X pc_high=0x%06X "
                     "ptr_ok=%d dec_low=%d dec_high=%d %s",
                     r.map_id, r.snes_low, r.pc_low, r.snes_high, r.pc_high,
                     r.pointers_valid, r.decompress_low_ok, r.decompress_high_ok,
                     r.error.c_str())
              << "\n";
  }
}

// Check if expanded tile16 region has been corrupted
void ValidateTile16Region(const Rom* rom) {
  constexpr uint32_t kMap16TilesExpanded = 0x1E8000;
  constexpr uint32_t kMap16TilesExpandedEnd = 0x1F0000;  // 4096 tiles * 8 bytes
  constexpr uint32_t kMap16ExpandedFlagPos = 0x02FD28;
  
  // Check if ROM uses expanded tile16
  uint8_t expanded_flag = rom->data()[kMap16ExpandedFlagPos];
  bool uses_expanded = (expanded_flag != 0x0F);
  
  std::cout << "\n=== Tile16 Region Diagnostics ===\n";
  std::cout << absl::StrFormat("Expanded tile16 flag at 0x%06X: 0x%02X (%s)\n",
                               kMap16ExpandedFlagPos, expanded_flag,
                               uses_expanded ? "EXPANDED" : "VANILLA");
  
  if (!uses_expanded) {
    std::cout << "ROM uses vanilla tile16 layout, skipping expanded region check.\n";
    return;
  }
  
  // Check known problem addresses that might have been overwritten
  // These are addresses mentioned in the padding doc as "existing custom blocks"
  const uint32_t problem_addresses[] = {
    0x1E878B, 0x1E95A3, 0x1ED6F3, 0x1EF540
  };
  
  std::cout << "\nChecking for data at known problem addresses in tile16 region:\n";
  for (uint32_t addr : problem_addresses) {
    if (addr >= kMap16TilesExpanded && addr < kMap16TilesExpandedEnd) {
      // Calculate which tile16 this affects
      int tile_offset = (addr - kMap16TilesExpanded);
      int tile_index = tile_offset / 8;
      int byte_in_tile = tile_offset % 8;
      
      // Read 16 bytes around this address
      uint8_t sample[16] = {0};
      for (int i = 0; i < 16 && addr + i < rom->size(); ++i) {
        sample[i] = rom->data()[addr + i];
      }
      
      // Check if it looks like valid tile16 data (shorts should be reasonable)
      bool looks_valid = true;
      for (int i = 0; i < 16; i += 2) {
        uint16_t val = sample[i] | (sample[i+1] << 8);
        // Tile info format: tttttttt ttttpppp hvf00000
        // Upper byte shouldn't have bits 0-4 set unless flip/priority
        if ((val & 0x1F00) != 0 && (val & 0xE000) == 0) {
          looks_valid = false;
        }
      }
      
      std::cout << absl::StrFormat(
          "  0x%06X (tile16 #%d, byte %d): %02X %02X %02X %02X %02X %02X %02X %02X ... %s\n",
          addr, tile_index, byte_in_tile,
          sample[0], sample[1], sample[2], sample[3],
          sample[4], sample[5], sample[6], sample[7],
          looks_valid ? "OK" : "SUSPICIOUS");
    }
  }
  
  // Sample a few tiles to see if they look valid
  std::cout << "\nSampling tile16 data at key positions:\n";
  const int sample_tiles[] = {0, 100, 240, 300, 500, 1000, 2000, 3000, 4000};
  for (int tile_idx : sample_tiles) {
    if (tile_idx >= 4096) continue;
    uint32_t addr = kMap16TilesExpanded + (tile_idx * 8);
    if (addr + 8 > rom->size()) continue;
    
    // Read 4 tile info shorts
    uint16_t tiles[4];
    for (int i = 0; i < 4; ++i) {
      tiles[i] = rom->data()[addr + i*2] | (rom->data()[addr + i*2 + 1] << 8);
    }
    
    std::cout << absl::StrFormat(
        "  tile16 #%4d @ 0x%06X: %04X %04X %04X %04X\n",
        tile_idx, addr, tiles[0], tiles[1], tiles[2], tiles[3]);
  }
}

}  // namespace

absl::Status OverworldValidateCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  bool include_tail = parser.HasFlag("include-tail");
  bool json = parser.HasFlag("json");
  bool check_tile16 = parser.HasFlag("check-tile16");

  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));

  // kNumOverworldMaps is 160 (0xA0), tail maps are 0xA0-0xBF (32 more)
  int max_map = include_tail ? 0xC0 : zelda3::kNumOverworldMaps;
  for (int i = 0; i < max_map; ++i) {
    ASSIGN_OR_RETURN(auto res, ValidateMapPointers(overworld, i, include_tail));
    PrintResult(res, json);
  }

  // Tile16 region diagnostic
  if (check_tile16) {
    ValidateTile16Region(rom);
  }

  return absl::OkStatus();
}

}  // namespace yaze::cli
