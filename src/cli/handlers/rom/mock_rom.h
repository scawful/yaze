#ifndef YAZE_CLI_HANDLERS_MOCK_ROM_H
#define YAZE_CLI_HANDLERS_MOCK_ROM_H

#include "absl/status/status.h"
#include "rom/rom.h"

namespace yaze {
namespace cli {

/**
 * @brief Initialize a mock ROM for testing without requiring an actual ROM file
 *
 * This creates a minimal but valid ROM structure populated with:
 * - All Zelda3 embedded labels (rooms, sprites, entrances, items, etc.)
 * - Minimal header data to satisfy ROM validation
 * - Empty but properly sized data sections
 *
 * Purpose: Allow AI agent testing and CI/CD without committing ROM files
 *
 * @param rom ROM object to initialize as mock
 * @return absl::OkStatus() on success, error status on failure
 */
absl::Status InitializeMockRom(Rom& rom);

/**
 * @brief Check if mock ROM mode should be used based on flags
 * @return true if --mock-rom flag is set
 */
bool ShouldUseMockRom();

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_MOCK_ROM_H
