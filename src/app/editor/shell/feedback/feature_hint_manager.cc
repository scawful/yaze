#include "app/editor/shell/feedback/feature_hint_manager.h"

#include "app/editor/system/session/user_settings.h"

namespace yaze {
namespace editor {

void FeatureHintManager::RegisterBuiltinHints() {
  RegisterHint({FeatureHintId::kZScreamWelcome,
                "Coming from ZScream? yaze maps the same editors into a modern "
                "workspace — open the concept guide for a side-by-side map.",
                ToastType::kInfo, 12.0f, /*offer_guide=*/true});
  RegisterHint({FeatureHintId::kZScreamOverworld,
                "ZScream tip: Overworld edits live here. Pause-menu map art is "
                "Screen Editor → Overworld Map (not this canvas).",
                ToastType::kInfo, 9.0f, true});
  RegisterHint({FeatureHintId::kZScreamDungeon,
                "Browse room tools on the left. Inspect selections and "
                "preview rooms on the right.",
                ToastType::kInfo, 9.0f, true});
  RegisterHint({FeatureHintId::kZScreamProjects,
                "ZScream tip: .yaze / .yazeproj projects track ROM + settings. "
                "You can also open .zsproj files when available.",
                ToastType::kInfo, 9.0f, true});
  RegisterHint({FeatureHintId::kZScreamSave,
                "ZScream tip: Save Scope lets you choose ROM vs project vs "
                "both — check File → Save Scope before writing.",
                ToastType::kInfo, 9.0f, false});
  RegisterHint(
      {FeatureHintId::kZScreamTile16,
       "ZScream tip: Tile16 palette + paint tools live in the "
       "overworld sidebar (tile16 selector / workbench) — same role as "
       "ZScream's tile picker.",
       ToastType::kInfo, 9.0f, true});
  RegisterHint({FeatureHintId::kZScreamZso,
                "ZScream tip: This ROM has ZSCustomOverworld ASM. Area sizes, "
                "BG colors, and overlays depend on v1/v2/v3 — check Map "
                "Properties for version-gated fields.",
                ToastType::kInfo, 10.0f, true});
  RegisterHint({FeatureHintId::kZScreamScreenMap,
                "ZScream tip: Screen Editor → Overworld Map edits pause-menu "
                "map art. Playable world tiles stay in the Overworld editor.",
                ToastType::kInfo, 9.0f, true});
}

void FeatureHintManager::RegisterHint(FeatureHintDefinition definition) {
  if (!definition.id || !definition.message) {
    return;
  }
  catalog_[definition.id] = definition;
}

bool FeatureHintManager::MaybeShow(std::string_view id) {
  if (!IsEnabled() || !toast_manager_ || id.empty()) {
    return false;
  }
  if (IsDismissed(id)) {
    return false;
  }
  auto it = catalog_.find(std::string(id));
  if (it == catalog_.end()) {
    return false;
  }

  const FeatureHintDefinition& hint = it->second;
  if (hint.offer_guide && guide_callback_) {
    toast_manager_->Show(hint.message, hint.type, hint.ttl_seconds,
                         "Open guide", guide_callback_);
  } else {
    toast_manager_->Show(hint.message, hint.type, hint.ttl_seconds);
  }
  Dismiss(id);
  return true;
}

void FeatureHintManager::Dismiss(std::string_view id) {
  if (!user_settings_ || id.empty()) {
    return;
  }
  auto& dismissed = user_settings_->prefs().dismissed_feature_hints;
  if (dismissed.insert(std::string(id)).second) {
    Persist();
  }
}

bool FeatureHintManager::IsDismissed(std::string_view id) const {
  if (!user_settings_ || id.empty()) {
    return false;
  }
  const auto& dismissed = user_settings_->prefs().dismissed_feature_hints;
  return dismissed.find(std::string(id)) != dismissed.end();
}

bool FeatureHintManager::IsEnabled() const {
  if (!user_settings_) {
    return true;
  }
  return user_settings_->prefs().feature_hints_enabled;
}

void FeatureHintManager::SetEnabled(bool enabled) {
  if (!user_settings_) {
    return;
  }
  if (user_settings_->prefs().feature_hints_enabled == enabled) {
    return;
  }
  user_settings_->prefs().feature_hints_enabled = enabled;
  Persist();
}

void FeatureHintManager::ResetDismissed() {
  if (!user_settings_) {
    return;
  }
  user_settings_->prefs().dismissed_feature_hints.clear();
  Persist();
}

size_t FeatureHintManager::dismissed_count() const {
  if (!user_settings_) {
    return 0;
  }
  return user_settings_->prefs().dismissed_feature_hints.size();
}

void FeatureHintManager::Persist() {
  if (user_settings_) {
    (void)user_settings_->Save();
  }
}

}  // namespace editor
}  // namespace yaze
