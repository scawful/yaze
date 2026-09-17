#ifndef YAZE_APP_EDITOR_SYSTEM_EMULATOR_RUNTIME_POLICY_H_
#define YAZE_APP_EDITOR_SYSTEM_EMULATOR_RUNTIME_POLICY_H_

#include <string>
#include <vector>

namespace yaze::editor {

// When emulator panels are hidden, only advance SNES/audio if the user opted
// into background keep-alive. MusicPlayer drives its own frames when needed.
inline bool ShouldTickEmulatorWhenHidden(bool keep_running_in_background) {
  return keep_running_in_background;
}

// Startup category policy:
// - Honor the last-active category, including Emulator (user left it open).
// - Never default to Emulator when no saved category is available.
inline std::string PreferStartupCategory(
    const std::string& saved_category,
    const std::vector<std::string>& available_categories) {
  auto category_available = [&](const std::string& cat) {
    if (available_categories.empty()) {
      return true;
    }
    for (const auto& candidate : available_categories) {
      if (candidate == cat) {
        return true;
      }
    }
    return false;
  };

  if (!saved_category.empty() && category_available(saved_category)) {
    return saved_category;
  }

  for (const auto& cat : available_categories) {
    if (cat != "Emulator") {
      return cat;
    }
  }
  return {};
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_EMULATOR_RUNTIME_POLICY_H_
