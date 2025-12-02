#ifndef YAZE_APP_EDITOR_SAFEGUARDS_H
#define YAZE_APP_EDITOR_SAFEGUARDS_H

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "rom/rom.h"

namespace yaze {
namespace editor {

// Helper function to check if ROM is loaded and return error if not
inline absl::Status RequireRomLoaded(const Rom* rom, const std::string& operation) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("%s: ROM not loaded", operation));
  }
  return absl::OkStatus();
}

// Helper function to check ROM state with custom message
inline absl::Status CheckRomState(const Rom* rom, const std::string& message) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError(message);
  }
  return absl::OkStatus();
}

// Helper function for generating consistent ROM status messages
inline std::string GetRomStatusMessage(const Rom* rom) {
  if (!rom)
    return "No ROM loaded";
  if (!rom->is_loaded())
    return "ROM failed to load";
  return absl::StrFormat("ROM loaded: %s", rom->title());
}

// Helper function to check if ROM is in a valid state for editing
inline bool IsRomReadyForEditing(const Rom* rom) {
  return rom && rom->is_loaded() && !rom->title().empty();
}

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SAFEGUARDS_H
