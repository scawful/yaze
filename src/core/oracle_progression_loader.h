#ifndef YAZE_CORE_ORACLE_PROGRESSION_LOADER_H
#define YAZE_CORE_ORACLE_PROGRESSION_LOADER_H

#include <string>

#include "absl/status/statusor.h"
#include "core/oracle_progression.h"

namespace yaze::core {

// Loads Oracle of Secrets progression state from an emulator SRAM file.
//
// The file is interpreted as raw SRAM bytes starting at $7EF000, which matches
// the layout used by OracleProgressionState::ParseFromSRAM.
absl::StatusOr<OracleProgressionState> LoadOracleProgressionFromSrmFile(
    const std::string& srm_path);

}  // namespace yaze::core

#endif  // YAZE_CORE_ORACLE_PROGRESSION_LOADER_H
