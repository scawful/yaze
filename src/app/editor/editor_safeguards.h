#ifndef YAZE_APP_EDITOR_SAFEGUARDS_H
#define YAZE_APP_EDITOR_SAFEGUARDS_H

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/rom.h"

namespace yaze {
namespace editor {

// Macro for checking ROM loading state in editor methods
#define REQUIRE_ROM_LOADED(rom_ptr, operation) \
  do { \
    if (!(rom_ptr) || !(rom_ptr)->is_loaded()) { \
      return absl::FailedPreconditionError( \
          absl::StrFormat("%s: ROM not loaded", (operation))); \
    } \
  } while (0)

// Macro for ROM state checking with custom error message
#define CHECK_ROM_STATE(rom_ptr, message) \
  do { \
    if (!(rom_ptr) || !(rom_ptr)->is_loaded()) { \
      return absl::FailedPreconditionError(message); \
    } \
  } while (0)

// Helper function for generating consistent ROM status messages
inline std::string GetRomStatusMessage(const Rom* rom) {
  if (!rom) return "No ROM loaded";
  if (!rom->is_loaded()) return "ROM failed to load";
  return absl::StrFormat("ROM loaded: %s", rom->title());
}

// Helper function to check if ROM is in a valid state for editing
inline bool IsRomReadyForEditing(const Rom* rom) {
  return rom && rom->is_loaded() && !rom->title().empty();
}

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SAFEGUARDS_H
