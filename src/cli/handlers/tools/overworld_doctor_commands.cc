#include "cli/handlers/tools/overworld_doctor_commands.h"

#include <fstream>
#include <iostream>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/util/compression.h"
#include "app/rom.h"
#include "app/snes.h"
#include "util/log.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::cli {

namespace {

// =============================================================================
// ROM Layout Constants
// =============================================================================

// Tile16 expanded region
constexpr uint32_t kMap16TilesExpanded = 0x1E8000;
constexpr uint32_t kMap16TilesExpandedEnd = 0x1F0000;
constexpr uint32_t kMap16ExpandedFlagPos = 0x02FD28;
constexpr uint32_t kMap32ExpandedFlagPos = 0x01772E;
constexpr int kNumTile16Vanilla = 3752;
constexpr int kNumTile16Expanded = 4096;

// Pointer table layout (vanilla - 160 entries only!)
// CRITICAL: These tables only cover maps 0x00-0x9F (160 maps)
// Maps 0xA0-0xBF do NOT have pointer table entries without ASM expansion
constexpr uint32_t kPtrTableLowBase = 0x1794D;   // 160 entries × 3 bytes = 0x1E0
constexpr uint32_t kPtrTableHighBase = 0x17B2D;  // Starts right after low table
constexpr int kVanillaMapCount = 160;            // 0x00-0x9F only

// ZSCustomOverworld version markers (from zelda3/common.h)
constexpr uint32_t kZSCustomVersionPos = 0x140145;  // OverworldCustomASMHasBeenApplied

// ZSCustomOverworld feature enable flags (from zelda3/overworld/overworld_map.h)
constexpr uint32_t kCustomBGEnabledPos = 0x140141;      // OverworldCustomAreaSpecificBGEnabled
constexpr uint32_t kCustomMainPalettePos = 0x140142;    // OverworldCustomMainPaletteEnabled
constexpr uint32_t kCustomMosaicPos = 0x140143;         // OverworldCustomMosaicEnabled
constexpr uint32_t kCustomAnimatedGFXPos = 0x140146;    // OverworldCustomAnimatedGFXEnabled
constexpr uint32_t kCustomOverlayPos = 0x140147;        // OverworldCustomSubscreenOverlayEnabled
constexpr uint32_t kCustomTileGFXPos = 0x140148;        // OverworldCustomTileGFXGroupEnabled

// Known problematic addresses in tile16 region (from previous corruption)
const uint32_t kProblemAddresses[] = {
    0x1E878B, 0x1E95A3, 0x1ED6F3, 0x1EF540
};

// =============================================================================
// Diagnostic Result Structure
// =============================================================================

struct RomFeatures {
  // Version info
  uint8_t zs_custom_version = 0xFF;  // 0xFF = vanilla, 2 = v2, 3 = v3
  bool is_vanilla = true;
  bool is_v2 = false;
  bool is_v3 = false;
  
  // Expanded data flags
  bool has_expanded_tile16 = false;
  bool has_expanded_tile32 = false;
  bool has_expanded_pointer_tables = false;  // Requires ASM patch
  
  // ZSCustomOverworld features (ROM-level enable flags)
  bool custom_bg_enabled = false;
  bool custom_main_palette_enabled = false;
  bool custom_mosaic_enabled = false;
  bool custom_animated_gfx_enabled = false;
  bool custom_overlay_enabled = false;
  bool custom_tile_gfx_enabled = false;
};

struct DiagnosticResult {
  RomFeatures features;
  
  // Tile16 status
  bool uses_expanded_tile16 = false;
  bool tile16_corruption_detected = false;
  std::vector<uint32_t> corrupted_addresses;
  int corrupted_tile_count = 0;
  
  // Map pointer status
  bool lw_dw_maps_valid = true;   // Maps 0x00-0x7F
  bool sw_maps_valid = true;      // Maps 0x80-0x9F
  bool tail_maps_valid = false;   // Maps 0xA0-0xBF (requires expansion)
  int invalid_map_count = 0;
  
  // Tail support
  bool can_support_tail = false;  // True only if expanded pointer tables exist
};

uint32_t PcToSnes(uint32_t pc) {
  uint32_t bank = (pc >> 15) & 0x7F;
  uint32_t addr = (pc & 0x7FFF) + 0x8000;
  return (bank << 16) | addr;
}

uint32_t SnesToPc(uint32_t snes) {
  return ((snes & 0x7F0000) >> 1) | (snes & 0x7FFF);
}

// Check if a tile16 entry looks valid
bool IsTile16Valid(uint16_t tile_info) {
  // Tile info format: tttttttt ttttpppp hvf00000
  // Bits 8-12 (0x1F00) should be 0 for valid tiles (unless flip bits are set)
  // This heuristic catches obvious garbage data
  if ((tile_info & 0x1F00) != 0 && (tile_info & 0xE000) == 0) {
    return false;
  }
  return true;
}

// =============================================================================
// Feature Detection
// =============================================================================

RomFeatures DetectRomFeatures(Rom* rom, bool verbose) {
  RomFeatures f;
  
  // Detect ZSCustomOverworld version
  // The version byte is at 0x140145 in the expanded region
  // For vanilla ROMs, this address may be outside ROM size or contain 0xFF/0x00
  if (kZSCustomVersionPos < rom->size()) {
    f.zs_custom_version = rom->data()[kZSCustomVersionPos];
    // 0xFF or 0x00 means vanilla (no ZSCustomOverworld ASM applied)
    f.is_vanilla = (f.zs_custom_version == 0xFF || f.zs_custom_version == 0x00);
    f.is_v2 = (!f.is_vanilla && f.zs_custom_version == 2);
    f.is_v3 = (!f.is_vanilla && f.zs_custom_version >= 3);
  } else {
    // ROM too small to have version marker - definitely vanilla
    f.is_vanilla = true;
  }
  
  // Detect expanded tile16/tile32
  // IMPORTANT: In vanilla ROMs, these "flag" positions contain game code, not flags!
  // Only check these flags if ZSCustomOverworld ASM is applied (not vanilla)
  if (!f.is_vanilla) {
    if (kMap16ExpandedFlagPos < rom->size()) {
      uint8_t flag = rom->data()[kMap16ExpandedFlagPos];
      // 0x0F = vanilla/standard layout, other = expanded
      f.has_expanded_tile16 = (flag != 0x0F);
    }
    
    if (kMap32ExpandedFlagPos < rom->size()) {
      uint8_t flag = rom->data()[kMap32ExpandedFlagPos];
      // 0x04 = vanilla/standard layout, other = expanded
      f.has_expanded_tile32 = (flag != 0x04);
    }
  }
  // For vanilla ROMs, assume standard (non-expanded) layout
  // These flags are false by default
  
  // TODO: Detect expanded pointer tables (requires checking for ASM marker)
  // For now, assume vanilla tables (160 entries) unless we add a marker
  f.has_expanded_pointer_tables = false;
  
  // Detect ZSCustomOverworld feature enables (only if v2+)
  if (!f.is_vanilla) {
    if (kCustomBGEnabledPos < rom->size())
      f.custom_bg_enabled = (rom->data()[kCustomBGEnabledPos] != 0);
    if (kCustomMainPalettePos < rom->size())
      f.custom_main_palette_enabled = (rom->data()[kCustomMainPalettePos] != 0);
    if (kCustomMosaicPos < rom->size())
      f.custom_mosaic_enabled = (rom->data()[kCustomMosaicPos] != 0);
    if (kCustomAnimatedGFXPos < rom->size())
      f.custom_animated_gfx_enabled = (rom->data()[kCustomAnimatedGFXPos] != 0);
    if (kCustomOverlayPos < rom->size())
      f.custom_overlay_enabled = (rom->data()[kCustomOverlayPos] != 0);
    if (kCustomTileGFXPos < rom->size())
      f.custom_tile_gfx_enabled = (rom->data()[kCustomTileGFXPos] != 0);
  }
  
  if (verbose) {
    std::cout << "\n=== ROM Feature Detection ===\n";
    std::cout << absl::StrFormat("ZSCustomOverworld version: %s\n",
        f.is_vanilla ? "Vanilla" : 
        f.is_v2 ? "v2" : 
        f.is_v3 ? "v3" : "Unknown");
    std::cout << absl::StrFormat("Expanded Tile16: %s\n", 
        f.has_expanded_tile16 ? "YES" : "NO");
    std::cout << absl::StrFormat("Expanded Tile32: %s\n", 
        f.has_expanded_tile32 ? "YES" : "NO");
    std::cout << absl::StrFormat("Expanded Pointer Tables: %s\n",
        f.has_expanded_pointer_tables ? "YES (192 maps)" : "NO (160 maps)");
    
    if (!f.is_vanilla) {
      std::cout << "\nZSCustomOverworld Features:\n";
      std::cout << absl::StrFormat("  Custom BG Colors: %s\n", 
          f.custom_bg_enabled ? "ON" : "OFF");
      std::cout << absl::StrFormat("  Custom Main Palette: %s\n", 
          f.custom_main_palette_enabled ? "ON" : "OFF");
      std::cout << absl::StrFormat("  Custom Mosaic: %s\n", 
          f.custom_mosaic_enabled ? "ON" : "OFF");
      std::cout << absl::StrFormat("  Custom Animated GFX: %s\n", 
          f.custom_animated_gfx_enabled ? "ON" : "OFF");
      std::cout << absl::StrFormat("  Custom Overlay: %s\n", 
          f.custom_overlay_enabled ? "ON" : "OFF");
      std::cout << absl::StrFormat("  Custom Tile GFX: %s\n", 
          f.custom_tile_gfx_enabled ? "ON" : "OFF");
    }
  }
  
  return f;
}

// =============================================================================
// Map Pointer Validation
// =============================================================================

void ValidateMapPointers(Rom* rom, DiagnosticResult& result, bool verbose) {
  // CRITICAL: Pointer tables only cover maps 0x00-0x9F (160 entries)
  // Do NOT attempt to read/write beyond this range without ASM expansion!
  
  if (verbose) {
    std::cout << "\n=== Map Pointer Validation ===\n";
    std::cout << absl::StrFormat("Pointer table layout: %d entries (maps 0x00-0x%02X)\n",
        kVanillaMapCount, kVanillaMapCount - 1);
  }
  
  result.lw_dw_maps_valid = true;
  result.sw_maps_valid = true;
  
  // Check maps 0x00-0x9F (the valid range for vanilla pointer tables)
  for (int map_id = 0; map_id < kVanillaMapCount; ++map_id) {
    uint32_t ptr_low_addr = kPtrTableLowBase + (3 * map_id);
    uint32_t ptr_high_addr = kPtrTableHighBase + (3 * map_id);
    
    if (ptr_low_addr + 3 > rom->size() || ptr_high_addr + 3 > rom->size()) {
      result.invalid_map_count++;
      if (map_id < 0x80) result.lw_dw_maps_valid = false;
      else result.sw_maps_valid = false;
      continue;
    }
    
    // Read 24-bit pointers
    uint32_t snes_low = rom->data()[ptr_low_addr] |
                        (rom->data()[ptr_low_addr + 1] << 8) |
                        (rom->data()[ptr_low_addr + 2] << 16);
    uint32_t snes_high = rom->data()[ptr_high_addr] |
                         (rom->data()[ptr_high_addr + 1] << 8) |
                         (rom->data()[ptr_high_addr + 2] << 16);
    
    uint32_t pc_low = SnesToPc(snes_low);
    uint32_t pc_high = SnesToPc(snes_high);
    
    // Validate pointers
    bool low_valid = (pc_low > 0 && pc_low < rom->size());
    bool high_valid = (pc_high > 0 && pc_high < rom->size());
    
    if (!low_valid || !high_valid) {
      result.invalid_map_count++;
      if (map_id < 0x80) result.lw_dw_maps_valid = false;
      else result.sw_maps_valid = false;
      
      if (verbose) {
        std::cout << absl::StrFormat(
            "  Map 0x%02X: INVALID - low=0x%06X (%s) high=0x%06X (%s)\n",
            map_id, pc_low, low_valid ? "OK" : "BAD",
            pc_high, high_valid ? "OK" : "BAD");
      }
    }
  }
  
  // Tail maps (0xA0-0xBF) - only valid with expanded pointer tables
  result.tail_maps_valid = result.features.has_expanded_pointer_tables;
  result.can_support_tail = result.features.has_expanded_pointer_tables;
  
  if (verbose) {
    std::cout << absl::StrFormat("\nLight/Dark World (0x00-0x7F): %s\n",
        result.lw_dw_maps_valid ? "OK" : "CORRUPTED");
    std::cout << absl::StrFormat("Special World (0x80-0x9F): %s\n",
        result.sw_maps_valid ? "OK" : "CORRUPTED");
    std::cout << absl::StrFormat("Tail Maps (0xA0-0xBF): %s\n",
        result.can_support_tail ? "Supported (expanded tables)" : 
        "NOT AVAILABLE (requires ASM expansion)");
  }
}

// =============================================================================
// Tile16 Corruption Check
// =============================================================================

void CheckTile16Corruption(Rom* rom, DiagnosticResult& result, bool verbose) {
  if (!result.features.has_expanded_tile16) {
    if (verbose) {
      std::cout << "\nTile16: Using vanilla layout, skipping expansion check.\n";
    }
    return;
  }
  
  if (verbose) {
    std::cout << "\n=== Tile16 Corruption Check ===\n";
  }
  
  // Check known problem addresses
  for (uint32_t addr : kProblemAddresses) {
    if (addr >= kMap16TilesExpanded && addr < kMap16TilesExpandedEnd) {
      int tile_offset = addr - kMap16TilesExpanded;
      int tile_index = tile_offset / 8;
      
      // Read the tile data at this position
      uint16_t tile_data[4];
      for (int i = 0; i < 4 && (addr + i*2 + 1) < rom->size(); ++i) {
        tile_data[i] = rom->data()[addr + i*2] | 
                       (rom->data()[addr + i*2 + 1] << 8);
      }
      
      // Check if data looks corrupted
      bool looks_valid = true;
      for (int i = 0; i < 4; ++i) {
        if (!IsTile16Valid(tile_data[i])) {
          looks_valid = false;
          break;
        }
      }
      
      if (!looks_valid) {
        result.tile16_corruption_detected = true;
        result.corrupted_addresses.push_back(addr);
        result.corrupted_tile_count++;
        
        if (verbose) {
          std::cout << absl::StrFormat(
              "  CORRUPTION at 0x%06X (tile16 #%d): %04X %04X %04X %04X\n",
              addr, tile_index, tile_data[0], tile_data[1], 
              tile_data[2], tile_data[3]);
        }
      }
    }
  }
  
  // Count suspicious expansion tiles
  int suspicious_count = 0;
  for (int tile = kNumTile16Vanilla; tile < kNumTile16Expanded; ++tile) {
    uint32_t addr = kMap16TilesExpanded + (tile * 8);
    if (addr + 8 > rom->size()) break;
    
    bool all_same = true;
    uint16_t first_val = rom->data()[addr] | (rom->data()[addr + 1] << 8);
    for (int i = 1; i < 4; ++i) {
      uint16_t val = rom->data()[addr + i*2] | (rom->data()[addr + i*2 + 1] << 8);
      if (val != first_val) {
        all_same = false;
        break;
      }
    }
    
    if (all_same && first_val != 0x0000 && first_val != 0xFFFF) {
      suspicious_count++;
    }
  }
  
  if (verbose) {
    if (result.tile16_corruption_detected) {
      std::cout << absl::StrFormat("Found %d corrupted tile16 entries\n",
          result.corrupted_tile_count);
    } else {
      std::cout << "No tile16 corruption detected at known problem addresses.\n";
    }
    if (suspicious_count > 0) {
      std::cout << absl::StrFormat(
          "Note: %d tiles in expansion region (3752-4095) have uniform values\n",
          suspicious_count);
    }
  }
}

// =============================================================================
// Main Diagnostic Function
// =============================================================================

DiagnosticResult DiagnoseRom(Rom* rom, bool verbose) {
  DiagnosticResult result;
  
  // Detect ROM features
  result.features = DetectRomFeatures(rom, verbose);
  result.uses_expanded_tile16 = result.features.has_expanded_tile16;
  
  // Validate map pointers
  ValidateMapPointers(rom, result, verbose);
  
  // Check tile16 corruption
  CheckTile16Corruption(rom, result, verbose);
  
  return result;
}

// =============================================================================
// Repair Functions
// =============================================================================

absl::Status RepairTile16Region(Rom* rom, const DiagnosticResult& diag, 
                                 bool verbose) {
  if (!diag.tile16_corruption_detected) {
    if (verbose) {
      std::cout << "No tile16 corruption to repair.\n";
    }
    return absl::OkStatus();
  }
  
  std::cout << absl::StrFormat(
      "Repairing %zu corrupted tile16 addresses...\n",
      diag.corrupted_addresses.size());
  
  // Zero out corrupted regions
  for (uint32_t addr : diag.corrupted_addresses) {
    // Zero out 8 bytes (one tile16 entry) at each corrupted address
    for (int i = 0; i < 8 && addr + i < rom->size(); ++i) {
      (*rom)[addr + i] = 0x00;
    }
    if (verbose) {
      std::cout << absl::StrFormat("  Zeroed tile16 at 0x%06X\n", addr);
    }
  }
  
  return absl::OkStatus();
}

absl::Status PadTailMaps(Rom* rom, const DiagnosticResult& diag, bool verbose) {
  // CRITICAL: Check if ROM has expanded pointer tables
  // Without ASM expansion, writing to tail map pointers will corrupt LW/DW!
  if (!diag.can_support_tail) {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ERROR: Cannot pad tail maps without ASM expansion            ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  The pointer tables only have 160 entries (maps 0x00-0x9F).   ║\n";
    std::cout << "║  Writing to map 0xA0+ would CORRUPT Light World pointers!     ║\n";
    std::cout << "║                                                               ║\n";
    std::cout << "║  Required: Apply ASM patch to expand pointer tables to 192.  ║\n";
    std::cout << "║  See: docs/internal/overworld_third_world_padding.md          ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
    
    return absl::FailedPreconditionError(
        "Tail map padding requires ASM expansion of pointer tables. "
        "Without this, writing to maps 0xA0-0xBF would corrupt maps 0x00-0x1F.");
  }
  
  // If we have expanded tables, proceed with padding
  // (This code path is currently unreachable until ASM expansion is implemented)
  std::cout << "Padding tail maps (0xA0-0xBF) with expanded pointer tables...\n";
  
  // TODO: Implement actual padding when ASM expansion is available
  // For now, this is a placeholder that won't be reached
  
  return absl::OkStatus();
}

// =============================================================================
// Print Summary
// =============================================================================

void PrintDiagnosticSummary(const DiagnosticResult& diag) {
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║                    DIAGNOSTIC SUMMARY                         ║\n";
  std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
  
  // ROM Version
  std::cout << absl::StrFormat("║  ROM Version: %-46s ║\n",
      diag.features.is_vanilla ? "Vanilla" :
      diag.features.is_v2 ? "ZSCustomOverworld v2" :
      diag.features.is_v3 ? "ZSCustomOverworld v3" : "Unknown");
  
  // Expanded data
  std::cout << absl::StrFormat("║  Expanded Tile16: %-42s ║\n",
      diag.features.has_expanded_tile16 ? "YES" : "NO");
  std::cout << absl::StrFormat("║  Expanded Tile32: %-42s ║\n",
      diag.features.has_expanded_tile32 ? "YES" : "NO");
  std::cout << absl::StrFormat("║  Expanded Ptr Tables: %-38s ║\n",
      diag.features.has_expanded_pointer_tables ? "YES (192 maps)" : "NO (160 maps)");
  
  std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
  
  // Map status
  std::cout << absl::StrFormat("║  Light/Dark World (0x00-0x7F): %-29s ║\n",
      diag.lw_dw_maps_valid ? "OK" : "CORRUPTED");
  std::cout << absl::StrFormat("║  Special World (0x80-0x9F): %-32s ║\n",
      diag.sw_maps_valid ? "OK" : "CORRUPTED");
  std::cout << absl::StrFormat("║  Tail Maps (0xA0-0xBF): %-36s ║\n",
      diag.can_support_tail ? "Available" : "N/A (no ASM expansion)");
  
  // Tile16 corruption
  if (diag.uses_expanded_tile16) {
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    if (diag.tile16_corruption_detected) {
      std::cout << absl::StrFormat("║  Tile16 Corruption: DETECTED (%zu addresses)%-17s ║\n",
          diag.corrupted_addresses.size(), "");
      for (uint32_t addr : diag.corrupted_addresses) {
        int tile_idx = (addr - kMap16TilesExpanded) / 8;
        std::cout << absl::StrFormat("║    - 0x%06X (tile #%d)%-36s ║\n", 
            addr, tile_idx, "");
      }
    } else {
      std::cout << "║  Tile16 Corruption: None detected                             ║\n";
    }
  }
  
  std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
}

// =============================================================================
// Baseline Comparison
// =============================================================================

void CompareWithBaseline(Rom* rom, const std::vector<uint8_t>& baseline, 
                         bool verbose) {
  std::cout << "\n=== Baseline Comparison ===\n";
  
  size_t min_size = std::min(rom->size(), baseline.size());
  
  // Compare map pointer tables
  size_t ptr_low_diffs = 0;
  size_t ptr_high_diffs = 0;
  
  for (int i = 0; i < kVanillaMapCount; ++i) {
    uint32_t low_addr = kPtrTableLowBase + (3 * i);
    uint32_t high_addr = kPtrTableHighBase + (3 * i);
    
    if (low_addr + 3 <= min_size) {
      for (int j = 0; j < 3; ++j) {
        if (rom->data()[low_addr + j] != baseline[low_addr + j]) {
          ptr_low_diffs++;
        }
      }
    }
    
    if (high_addr + 3 <= min_size) {
      for (int j = 0; j < 3; ++j) {
        if (rom->data()[high_addr + j] != baseline[high_addr + j]) {
          ptr_high_diffs++;
        }
      }
    }
  }
  
  std::cout << absl::StrFormat("Map32 Pointer Low table: %zu bytes differ\n", 
      ptr_low_diffs);
  std::cout << absl::StrFormat("Map32 Pointer High table: %zu bytes differ\n", 
      ptr_high_diffs);
  
  // Compare tile16 region if expanded
  if (kMap16TilesExpanded < min_size) {
    size_t tile16_diffs = 0;
    uint32_t end = std::min(kMap16TilesExpandedEnd, static_cast<uint32_t>(min_size));
    for (uint32_t i = kMap16TilesExpanded; i < end; ++i) {
      if (rom->data()[i] != baseline[i]) {
        tile16_diffs++;
      }
    }
    std::cout << absl::StrFormat("Tile16 Expanded region: %zu bytes differ\n", 
        tile16_diffs);
  }
  
  // Show first few differences in pointer tables if any
  if (verbose && (ptr_low_diffs > 0 || ptr_high_diffs > 0)) {
    std::cout << "\nPointer table differences (first 5):\n";
    int shown = 0;
    for (int i = 0; i < kVanillaMapCount && shown < 5; ++i) {
      uint32_t low_addr = kPtrTableLowBase + (3 * i);
      if (low_addr + 3 <= min_size) {
        bool differs = false;
        for (int j = 0; j < 3; ++j) {
          if (rom->data()[low_addr + j] != baseline[low_addr + j]) {
            differs = true;
            break;
          }
        }
        if (differs) {
          uint32_t bl_ptr = baseline[low_addr] | 
                           (baseline[low_addr + 1] << 8) | 
                           (baseline[low_addr + 2] << 16);
          uint32_t rom_ptr = rom->data()[low_addr] | 
                            (rom->data()[low_addr + 1] << 8) | 
                            (rom->data()[low_addr + 2] << 16);
          std::cout << absl::StrFormat("  Map 0x%02X low: baseline=$%06X target=$%06X\n",
              i, bl_ptr, rom_ptr);
          shown++;
        }
      }
    }
  }
}

}  // namespace

absl::Status OverworldDoctorCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  bool fix_mode = parser.HasFlag("fix");
  bool verbose = parser.HasFlag("verbose");
  auto output_path = parser.GetString("output");
  auto baseline_path = parser.GetString("baseline");
  
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║                    OVERWORLD DOCTOR                           ║\n";
  std::cout << "║         ROM Diagnostic & Repair Tool                          ║\n";
  std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
  
  // Run diagnostics
  std::cout << "\nRunning diagnostics...\n";
  auto diag = DiagnoseRom(rom, verbose);
  
  // Print summary
  PrintDiagnosticSummary(diag);
  
  // Baseline comparison if provided
  if (baseline_path.has_value()) {
    std::ifstream baseline_file(baseline_path.value(), std::ios::binary);
    if (!baseline_file) {
      std::cout << absl::StrFormat("\nWarning: Cannot open baseline ROM: %s\n",
          baseline_path.value());
    } else {
      std::vector<uint8_t> baseline_data(
          (std::istreambuf_iterator<char>(baseline_file)),
          std::istreambuf_iterator<char>());
      baseline_file.close();
      
      std::cout << absl::StrFormat("\nComparing with baseline: %s\n", 
          baseline_path.value());
      CompareWithBaseline(rom, baseline_data, verbose);
    }
  }
  
  // Check for issues that need fixing
  bool has_fixable_issues = diag.tile16_corruption_detected;
  bool has_unfixable_issues = !diag.lw_dw_maps_valid || !diag.sw_maps_valid;
  
  if (has_unfixable_issues) {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  WARNING: Map pointer corruption detected!                    ║\n";
    std::cout << "║  This ROM may have been damaged by incorrect pointer writes.  ║\n";
    std::cout << "║  Consider restoring from a backup or original ROM.            ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
  }
  
  if (!fix_mode) {
    std::cout << "\nTo apply available fixes, run with --fix flag.\n";
    std::cout << "To save to a new file, use --output <path>.\n";
    return absl::OkStatus();
  }
  
  // Apply fixes
  std::cout << "\n=== Applying Fixes ===\n";
  
  if (diag.tile16_corruption_detected) {
    RETURN_IF_ERROR(RepairTile16Region(rom, diag, verbose));
    std::cout << "Tile16 corruption repaired.\n";
  } else {
    std::cout << "No tile16 corruption to repair.\n";
  }
  
  // Note: We no longer attempt to pad tail maps without proper ASM support
  if (!diag.can_support_tail) {
    std::cout << "\nNote: Tail map padding skipped (requires ASM expansion).\n";
    std::cout << "The editor will use blank fallback tiles for maps 0xA0-0xBF.\n";
  }
  
  // Save the fixed ROM
  if (output_path.has_value()) {
    std::cout << absl::StrFormat("\nSaving fixed ROM to: %s\n", 
                                  output_path.value());
    Rom::SaveSettings settings;
    settings.filename = output_path.value();
    settings.z3_save = false;  // Don't apply additional transforms
    RETURN_IF_ERROR(rom->SaveToFile(settings));
    std::cout << "ROM saved successfully.\n";
  } else if (has_fixable_issues) {
    std::cout << "\nNo output path specified. Use --output <path> to save fixes.\n";
  }
  
  return absl::OkStatus();
}

}  // namespace yaze::cli

