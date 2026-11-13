#ifndef YAZE_ZELDA3_OVERWORLD_UPGRADE_SYSTEM_H
#define YAZE_ZELDA3_OVERWORLD_UPGRADE_SYSTEM_H

#include <string>

#include "absl/status/status.h"

namespace yaze {

class Rom;

namespace zelda3 {

/**
 * @brief Service responsible for applying the ZSCustomOverworld ASM patch
 *        and updating ROM version markers for A Link to the Past.
 *
 * Stages supported vanilla/legacy-table v2 ROMs into the established v3
 * tables without changing the caller until migration and validation succeed.
 */
class OverworldUpgradeSystem {
 public:
  /**
   * @brief Constructs the upgrade system.
   * @param rom The ROM to be mutated. Must be valid for the lifetime of this object.
   */
  // asm_directory defaults to the installed resource location. Hosts which
  // supply their own bundled resources may provide that directory explicitly.
  explicit OverworldUpgradeSystem(Rom& rom, std::string asm_directory = "");

  /**
   * @brief Apply ZSCustomOverworld ASM patch to upgrade ROM version.
   * @param target_version Target version (3). The incomplete bundled v2
   * installer is refused; existing legacy-table v2 ROMs can upgrade to v3.
   * @return OK if the patch was successful, or an error detailing the ASM failure.
   */
  absl::Status ApplyZSCustomOverworldASM(int target_version);

 private:
  Rom& rom_;
  std::string asm_directory_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_OVERWORLD_UPGRADE_SYSTEM_H
