#include "app/editor/system/layout_orchestrator.h"

namespace yaze {
namespace editor {

LayoutOrchestrator::LayoutOrchestrator(LayoutManager* layout_manager,
                                       EditorCardRegistry* card_registry)
    : layout_manager_(layout_manager), card_registry_(card_registry) {}

void LayoutOrchestrator::Initialize(LayoutManager* layout_manager,
                                    EditorCardRegistry* card_registry) {
  layout_manager_ = layout_manager;
  card_registry_ = card_registry;
}

void LayoutOrchestrator::ApplyPreset(EditorType type,
                                     const std::string& session_id) {
  if (!IsInitialized()) {
    return;
  }

  // Get the default preset for this editor type
  auto preset = LayoutPresets::GetDefaultPreset(type);

  // Show default cards
  ShowPresetCards(preset, session_id);

  // Hide optional cards
  HideOptionalCards(type, session_id);

  // Apply DockBuilder layout
  ApplyDockLayout(type);
}

void LayoutOrchestrator::ApplyNamedPreset(const std::string& preset_name,
                                          const std::string& session_id) {
  if (!IsInitialized()) {
    return;
  }

  CardLayoutPreset preset;
  if (preset_name == "Minimal") {
    preset = LayoutPresets::GetMinimalPreset();
  } else if (preset_name == "Developer") {
    preset = LayoutPresets::GetDeveloperPreset();
  } else if (preset_name == "Designer") {
    preset = LayoutPresets::GetDesignerPreset();
  } else if (preset_name == "Modder") {
    preset = LayoutPresets::GetModderPreset();
  } else {
    // Unknown preset, use minimal
    preset = LayoutPresets::GetMinimalPreset();
  }

  ShowPresetCards(preset, session_id);
}

void LayoutOrchestrator::ResetToDefault(EditorType type,
                                        const std::string& session_id) {
  ApplyPreset(type, session_id);
  RequestLayoutRebuild();
}

std::string LayoutOrchestrator::GetWindowTitle(
    const std::string& card_id, const std::string& session_id) const {
  if (!card_registry_) {
    return "";
  }

  // Try to get card info (using session_id 0 for global lookup)
  auto* info = card_registry_->GetCardInfo(0, card_id);
  if (info) {
    return info->GetWindowTitle();
  }

  return "";
}

std::vector<std::string> LayoutOrchestrator::GetVisibleCards(
    const std::string& session_id) const {
  // Return empty since this requires more complex session handling
  // This can be implemented later when session-aware card visibility is needed
  return {};
}

void LayoutOrchestrator::ShowPresetCards(const CardLayoutPreset& preset,
                                         const std::string& session_id) {
  if (!card_registry_) {
    return;
  }

  for (const auto& card_id : preset.default_visible_cards) {
    std::string prefixed_id = GetPrefixedCardId(card_id, session_id);
    card_registry_->ShowCard(prefixed_id);
  }
}

void LayoutOrchestrator::HideOptionalCards(EditorType type,
                                           const std::string& session_id) {
  if (!card_registry_) {
    return;
  }

  auto preset = LayoutPresets::GetDefaultPreset(type);
  for (const auto& card_id : preset.optional_cards) {
    std::string prefixed_id = GetPrefixedCardId(card_id, session_id);
    card_registry_->HideCard(prefixed_id);
  }
}

void LayoutOrchestrator::RequestLayoutRebuild() {
  rebuild_requested_ = true;
  if (layout_manager_) {
    layout_manager_->RequestRebuild();
  }
}

void LayoutOrchestrator::ApplyDockLayout(EditorType type) {
  if (!layout_manager_) {
    return;
  }

  // Map EditorType to LayoutType
  LayoutType layout_type = LayoutType::kDefault;
  switch (type) {
    case EditorType::kOverworld:
      layout_type = LayoutType::kOverworld;
      break;
    case EditorType::kDungeon:
      layout_type = LayoutType::kDungeon;
      break;
    case EditorType::kGraphics:
      layout_type = LayoutType::kGraphics;
      break;
    case EditorType::kPalette:
      layout_type = LayoutType::kPalette;
      break;
    case EditorType::kSprite:
      layout_type = LayoutType::kSprite;
      break;
    case EditorType::kScreen:
      layout_type = LayoutType::kScreen;
      break;
    case EditorType::kMusic:
      layout_type = LayoutType::kMusic;
      break;
    case EditorType::kMessage:
      layout_type = LayoutType::kMessage;
      break;
    case EditorType::kAssembly:
      layout_type = LayoutType::kAssembly;
      break;
    case EditorType::kSettings:
      layout_type = LayoutType::kSettings;
      break;
    case EditorType::kEmulator:
      layout_type = LayoutType::kEmulator;
      break;
    default:
      layout_type = LayoutType::kDefault;
      break;
  }

  layout_manager_->SetLayoutType(layout_type);
  layout_manager_->RequestRebuild();
}

std::string LayoutOrchestrator::GetPrefixedCardId(
    const std::string& card_id, const std::string& session_id) const {
  if (session_id.empty()) {
    return card_id;
  }
  return session_id + "." + card_id;
}

}  // namespace editor
}  // namespace yaze

