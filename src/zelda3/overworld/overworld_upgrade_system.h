#ifndef YAZE_ZELDA3_OVERWORLD_UPGRADE_SYSTEM_H
#define YAZE_ZELDA3_OVERWORLD_UPGRADE_SYSTEM_H

#include "absl/status/status.h"

namespace yaze {

class Rom;

namespace zelda3 {

/**
 * @brief Service responsible for applying the ZSCustomOverworld ASM patch
 *        and updating ROM version markers for A Link to the Past.
 *
 * This system encapsulates the destructive logic of upgrading a vanilla ROM
 * to support expanded overworld features (v2 and v3).
 */
class OverworldUpgradeSystem {
 public:
  /**
   * @brief Constructs the upgrade system.
   * @param rom The ROM to be mutated. Must be valid for the lifetime of this object.
   */
  explicit OverworldUpgradeSystem(Rom& rom);

  /**
   * @brief Apply ZSCustomOverworld ASM patch to upgrade ROM version.
   * @param target_version Target version (2 for v2, 3 for v3).
   * @return OK if the patch was successful, or an error detailing the ASM failure.
   */
  absl::Status ApplyZSCustomOverworldASM(int target_version);

 private:
  /**
   * @brief Update ROM version markers and feature flags after ASM patching.
   *        This is an internal sub-step of the upgrade process.
   */
  absl::Status UpdateROMVersionMarkers(int target_version);

  Rom& rom_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_OVERWORLD_UPGRADE_SYSTEM_H
