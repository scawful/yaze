#include "zelda3/overworld/overworld_upgrade_system.h"

#include <filesystem>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "core/asar_wrapper.h"
#include "rom/rom.h"
#include "util/file_util.h"
#include "util/log.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace zelda3 {

OverworldUpgradeSystem::OverworldUpgradeSystem(Rom& rom) : rom_(rom) {}

absl::Status OverworldUpgradeSystem::ApplyZSCustomOverworldASM(
    int target_version) {
  // Validate target version
  if (target_version < 2 || target_version > 3) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid target version: %d. Must be 2 or 3.", target_version));
  }

  // Check current ROM version
  uint8_t current_version = rom_[OverworldCustomASMHasBeenApplied];
  if (current_version != 0xFF && current_version >= target_version) {
    return absl::AlreadyExistsError(absl::StrFormat(
        "ROM is already version %d or higher", current_version));
  }

  LOG_DEBUG("OverworldUpgradeSystem",
            "Applying ZSCustomOverworld ASM v%d to ROM...", target_version);

  // Initialize Asar wrapper
  auto asar_wrapper = std::make_unique<core::AsarWrapper>();
  RETURN_IF_ERROR(asar_wrapper->Initialize());

  // Create backup of ROM data
  std::vector<uint8_t> original_rom_data = rom_.vector();
  std::vector<uint8_t> working_rom_data = original_rom_data;

  try {
    // Determine which ASM file to apply
    std::string asm_file_name =
        (target_version == 3) ? "asm/yaze.asm" : "asm/ZSCustomOverworld.asm";

    std::string asm_file_path = util::GetResourcePath(asm_file_name);

    LOG_DEBUG("OverworldUpgradeSystem", "Using ASM file: %s",
              asm_file_path.c_str());

    if (!std::filesystem::exists(asm_file_path)) {
      return absl::NotFoundError(
          absl::StrFormat("ASM file not found at: %s\n\n"
                          "Expected location: assets/%s\n"
                          "Make sure the assets directory is accessible.",
                          asm_file_path, asm_file_name));
    }

    // Apply the ASM patch
    auto patch_result =
        asar_wrapper->ApplyPatch(asm_file_path, working_rom_data);
    if (!patch_result.ok()) {
      return absl::InternalError(absl::StrFormat(
          "Failed to apply ASM patch: %s", patch_result.status().message()));
    }

    const auto& result = patch_result.value();
    if (!result.success) {
      std::string error_details = "ASM patch failed with errors:\n";
      for (const auto& error : result.errors) {
        error_details += "  - " + error + "\n";
      }
      if (!result.warnings.empty()) {
        error_details += "Warnings:\n";
        for (const auto& warning : result.warnings) {
          error_details += "  - " + warning + "\n";
        }
      }
      return absl::InternalError(error_details);
    }

    // Update ROM with patched data
    RETURN_IF_ERROR(rom_.LoadFromData(working_rom_data));

    // Update version marker and feature flags
    RETURN_IF_ERROR(UpdateROMVersionMarkers(target_version));

    // Log symbols found during patching
    LOG_DEBUG("OverworldUpgradeSystem",
              "ASM patch applied successfully. Found %zu symbols:",
              result.symbols.size());
    for (const auto& symbol : result.symbols) {
      LOG_DEBUG("OverworldUpgradeSystem", "  %s @ $%06X", symbol.name.c_str(),
                symbol.address);
    }

    LOG_DEBUG("OverworldUpgradeSystem",
              "ZSCustomOverworld v%d successfully applied to ROM",
              target_version);
    return absl::OkStatus();

  } catch (const std::exception& e) {
    // Restore original ROM data on any exception
    auto restore_result = rom_.LoadFromData(original_rom_data);
    if (!restore_result.ok()) {
      LOG_ERROR("OverworldUpgradeSystem", "Failed to restore ROM data: %s",
                restore_result.message().data());
    }
    return absl::InternalError(
        absl::StrFormat("Exception during ASM application: %s", e.what()));
  }
}

absl::Status OverworldUpgradeSystem::UpdateROMVersionMarkers(
    int target_version) {
  // Set the main version marker
  rom_[OverworldCustomASMHasBeenApplied] = static_cast<uint8_t>(target_version);

  // Enable feature flags based on target version
  if (target_version >= 2) {
    rom_[OverworldCustomAreaSpecificBGEnabled] = 0x01;
    rom_[OverworldCustomMainPaletteEnabled] = 0x01;
    LOG_DEBUG("OverworldUpgradeSystem",
              "Enabled v2+ features: Custom BG colors, Main palettes");
  }

  if (target_version >= 3) {
    rom_[OverworldCustomSubscreenOverlayEnabled] = 0x01;
    rom_[OverworldCustomAnimatedGFXEnabled] = 0x01;
    rom_[OverworldCustomTileGFXGroupEnabled] = 0x01;
    rom_[OverworldCustomMosaicEnabled] = 0x01;

    LOG_DEBUG("OverworldUpgradeSystem",
              "Enabled v3+ features: Subscreen overlays, Animated GFX, Tile "
              "GFX groups, Mosaic");

    for (int i = 0; i < 0xA0; i++) {
      rom_[kOverworldScreenSize + i] =
          static_cast<uint8_t>(AreaSizeEnum::SmallArea);
    }

    const std::vector<int> large_areas = {
        0x00, 0x02, 0x05, 0x07, 0x0A, 0x0B, 0x0F, 0x10, 0x11, 0x12, 0x13,
        0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1D, 0x1E, 0x25,
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x30, 0x32, 0x33, 0x34,
        0x35, 0x37, 0x3A, 0x40, 0x42, 0x45, 0x47, 0x4A, 0x4B, 0x4F, 0x50,
        0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B,
        0x5D, 0x5E, 0x65, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6E, 0x6F, 0x70,
        0x72, 0x73, 0x74, 0x75, 0x77, 0x7A, 0x80, 0x81};
    for (int area : large_areas) {
      rom_[kOverworldScreenSize + area] =
          static_cast<uint8_t>(AreaSizeEnum::LargeArea);
    }
  }

  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
