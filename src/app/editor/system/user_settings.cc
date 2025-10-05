#include "app/editor/system/user_settings.h"
#include <fstream>
#include "util/file_util.h"

namespace yaze {
namespace editor {

UserSettings::UserSettings() {
  // TODO: Get platform-specific settings path
  settings_file_path_ = "yaze_settings.json";
}

absl::Status UserSettings::Load() {
  // TODO: Load from JSON file
  return absl::OkStatus();
}

absl::Status UserSettings::Save() {
  // TODO: Save to JSON file
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
