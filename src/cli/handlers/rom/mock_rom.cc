#include "cli/handlers/rom/mock_rom.h"

#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "core/project.h"
#include "zelda3/zelda3_labels.h"

ABSL_DECLARE_FLAG(bool, mock_rom);

namespace yaze {
namespace cli {

absl::Status InitializeMockRom(Rom& rom) {
  // Create a minimal but valid SNES ROM header
  // Zelda3 is a 1MB ROM (0x100000 bytes) in LoROM mapping
  constexpr size_t kMockRomSize = 0x100000;  // 1MB
  std::vector<uint8_t> mock_data(kMockRomSize, 0x00);

  // SNES header is at 0x7FC0 for LoROM
  constexpr size_t kHeaderOffset = 0x7FC0;

  // Set ROM title (21 bytes at 0x7FC0)
  const char* title = "YAZE MOCK ROM TEST  ";  // 21 chars including spaces
  for (size_t i = 0; i < 21; ++i) {
    mock_data[kHeaderOffset + i] = title[i];
  }

  // ROM makeup byte (0x7FD5): $20 = LoROM, no special chips
  mock_data[kHeaderOffset + 0x15] = 0x20;

  // ROM type (0x7FD6): $00 = ROM only
  mock_data[kHeaderOffset + 0x16] = 0x00;

  // ROM size (0x7FD7): $09 = 1MB (2^9 KB = 512 KB = 1MB with header)
  mock_data[kHeaderOffset + 0x17] = 0x09;

  // SRAM size (0x7FD8): $03 = 8KB (Zelda3 standard)
  mock_data[kHeaderOffset + 0x18] = 0x03;

  // Country code (0x7FD9): $01 = USA
  mock_data[kHeaderOffset + 0x19] = 0x01;

  // Developer ID (0x7FDA): $33 = Extended header (Zelda3)
  mock_data[kHeaderOffset + 0x1A] = 0x33;

  // Version number (0x7FDB): $00 = 1.0
  mock_data[kHeaderOffset + 0x1B] = 0x00;

  // Checksum complement (0x7FDC-0x7FDD): We'll leave as 0x0000 for mock
  // Checksum (0x7FDE-0x7FDF): We'll leave as 0x0000 for mock

  // Load the mock data into the ROM
  auto load_status = rom.LoadFromData(mock_data);
  if (!load_status.ok()) {
    return absl::InternalError(absl::StrFormat(
        "Failed to initialize mock ROM: %s", load_status.message()));
  }

  // Initialize embedded labels so queries work without actual ROM data
  project::YazeProject project;
  auto labels_status = project.InitializeEmbeddedLabels();
  if (!labels_status.ok()) {
    return absl::InternalError(absl::StrFormat(
        "Failed to initialize embedded labels: %s", labels_status.message()));
  }

  // Attach labels to ROM's resource label manager
  if (rom.resource_label()) {
    rom.resource_label()->labels_ = project.resource_labels;
    rom.resource_label()->labels_loaded_ = true;
  }

  return absl::OkStatus();
}

bool ShouldUseMockRom() {
  return absl::GetFlag(FLAGS_mock_rom);
}

}  // namespace cli
}  // namespace yaze
