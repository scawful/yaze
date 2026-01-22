#ifndef YAZE_APP_STARTUP_FLAGS_H_
#define YAZE_APP_STARTUP_FLAGS_H_

#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"

namespace yaze {

/**
 * @brief Tri-state toggle used for startup UI visibility controls.
 *
 * kAuto  - Use existing runtime logic (legacy behavior)
 * kShow  - Force the element to be shown on startup
 * kHide  - Force the element to be hidden on startup
 */
enum class StartupVisibility {
  kAuto,
  kShow,
  kHide,
};

/**
 * @brief Asset loading mode for editor resources.
 *
 * kAuto  - Choose based on runtime (e.g., headless/server => lazy).
 * kFull  - Load all editor assets on ROM load (legacy behavior).
 * kLazy  - Defer editor asset loading until first use.
 */
enum class AssetLoadMode {
  kAuto,
  kFull,
  kLazy,
};

inline StartupVisibility StartupVisibilityFromString(
    absl::string_view value) {
  const std::string lower = absl::AsciiStrToLower(std::string(value));
  if (lower == "show" || lower == "on" || lower == "visible") {
    return StartupVisibility::kShow;
  }
  if (lower == "hide" || lower == "off" || lower == "none") {
    return StartupVisibility::kHide;
  }
  return StartupVisibility::kAuto;
}

inline std::string StartupVisibilityToString(StartupVisibility value) {
  switch (value) {
    case StartupVisibility::kShow:
      return "show";
    case StartupVisibility::kHide:
      return "hide";
    case StartupVisibility::kAuto:
    default:
      return "auto";
  }
}

inline AssetLoadMode AssetLoadModeFromString(absl::string_view value) {
  const std::string lower = absl::AsciiStrToLower(std::string(value));
  if (lower == "full" || lower == "eager") {
    return AssetLoadMode::kFull;
  }
  if (lower == "lazy" || lower == "deferred" || lower == "on_demand") {
    return AssetLoadMode::kLazy;
  }
  return AssetLoadMode::kAuto;
}

inline std::string AssetLoadModeToString(AssetLoadMode value) {
  switch (value) {
    case AssetLoadMode::kFull:
      return "full";
    case AssetLoadMode::kLazy:
      return "lazy";
    case AssetLoadMode::kAuto:
    default:
      return "auto";
  }
}

}  // namespace yaze

#endif  // YAZE_APP_STARTUP_FLAGS_H_
