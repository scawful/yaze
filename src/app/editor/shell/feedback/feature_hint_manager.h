#ifndef YAZE_APP_EDITOR_SHELL_FEEDBACK_FEATURE_HINT_MANAGER_H_
#define YAZE_APP_EDITOR_SHELL_FEEDBACK_FEATURE_HINT_MANAGER_H_

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "app/editor/shell/feedback/toast_manager.h"

namespace yaze {
namespace editor {

class UserSettings;

/**
 * @brief Stable IDs for one-shot, dismissible onboarding tips.
 *
 * Persist in UserSettings::Preferences::dismissed_feature_hints. Prefer
 * dotted namespaces (e.g. zscream.overworld) so future HM/other packs share
 * the same store without collisions.
 */
namespace FeatureHintId {
constexpr const char* kZScreamWelcome = "zscream.welcome";
constexpr const char* kZScreamOverworld = "zscream.overworld";
constexpr const char* kZScreamDungeon = "zscream.dungeon";
constexpr const char* kZScreamProjects = "zscream.projects";
constexpr const char* kZScreamSave = "zscream.save";
constexpr const char* kZScreamTile16 = "zscream.tile16";
constexpr const char* kZScreamZso = "zscream.zso";
constexpr const char* kZScreamScreenMap = "zscream.screen_map";
}  // namespace FeatureHintId

struct FeatureHintDefinition {
  const char* id = nullptr;
  const char* message = nullptr;
  ToastType type = ToastType::kInfo;
  float ttl_seconds = 8.0f;
  /// When true and a guide callback is set, toast shows an "Open guide" button.
  bool offer_guide = false;
};

/**
 * @brief Show-once feature tips backed by ToastManager + UserSettings.
 *
 * Modern onboarding layer: fire contextual tips the first time a trigger
 * hits, then persist dismissal so survivors aren't nagged. Reset clears the
 * dismissed set (Settings > Editor Behavior).
 */
class FeatureHintManager {
 public:
  void SetUserSettings(UserSettings* settings) { user_settings_ = settings; }
  void SetToastManager(ToastManager* toast_manager) {
    toast_manager_ = toast_manager;
  }
  /// Opens Help → Coming from ZScream (or equivalent) from actionable tips.
  void SetGuideCallback(std::function<void()> callback) {
    guide_callback_ = std::move(callback);
  }

  /// Register built-in ZScream migrant tips. Safe to call more than once.
  void RegisterBuiltinHints();

  /// Register or replace a hint definition (tests / future packs).
  void RegisterHint(FeatureHintDefinition definition);

  /// Show the tip if enabled, known, and not yet dismissed. Returns true when
  /// a toast was emitted (and the id was marked dismissed).
  bool MaybeShow(std::string_view id);

  /// Mark dismissed without showing (e.g. user opened the guide manually).
  void Dismiss(std::string_view id);

  bool IsDismissed(std::string_view id) const;
  bool IsEnabled() const;
  void SetEnabled(bool enabled);

  /// Clear all dismissed ids and persist. Tips can fire again.
  void ResetDismissed();

  size_t registered_count() const { return catalog_.size(); }
  size_t dismissed_count() const;

 private:
  void Persist();

  UserSettings* user_settings_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
  std::function<void()> guide_callback_;
  std::unordered_map<std::string, FeatureHintDefinition> catalog_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SHELL_FEEDBACK_FEATURE_HINT_MANAGER_H_
