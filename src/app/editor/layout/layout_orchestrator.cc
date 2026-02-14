#include "app/editor/layout/layout_orchestrator.h"

#include "absl/strings/str_format.h"

namespace yaze {
namespace editor {

LayoutOrchestrator::LayoutOrchestrator(LayoutManager* layout_manager,
                                       PanelManager* panel_manager)
    : layout_manager_(layout_manager), panel_manager_(panel_manager) {}

void LayoutOrchestrator::Initialize(LayoutManager* layout_manager,
                                    PanelManager* panel_manager) {
  layout_manager_ = layout_manager;
  panel_manager_ = panel_manager;
}

void LayoutOrchestrator::ApplyPreset(EditorType type, size_t session_id) {
  if (!IsInitialized()) {
    return;
  }

  // Get the default preset for this editor type
  auto preset = LayoutPresets::GetDefaultPreset(type);

  // Show default panels
  ShowPresetPanels(preset, session_id, type);

  // Hide optional panels
  HideOptionalPanels(type, session_id);

  // Apply DockBuilder layout
  ApplyDockLayout(type);
}

void LayoutOrchestrator::ApplyNamedPreset(const std::string& preset_name,
                                          size_t session_id) {
  if (!IsInitialized()) {
    return;
  }

  PanelLayoutPreset preset;
  if (preset_name == "Minimal") {
    preset = LayoutPresets::GetMinimalPreset();
  } else if (preset_name == "Logic Debugger") {
    preset = LayoutPresets::GetLogicDebuggerPreset();
  } else if (preset_name == "Overworld Artist") {
    preset = LayoutPresets::GetOverworldArtistPreset();
  } else if (preset_name == "Dungeon Master") {
    preset = LayoutPresets::GetDungeonMasterPreset();
  } else if (preset_name == "Audio Engineer") {
    preset = LayoutPresets::GetAudioEngineerPreset();
  } else {
    // Unknown preset, use minimal
    preset = LayoutPresets::GetMinimalPreset();
  }

  ShowPresetPanels(preset, session_id, EditorType::kUnknown);
  RequestLayoutRebuild();
}

void LayoutOrchestrator::ResetToDefault(EditorType type, size_t session_id) {
  ApplyPreset(type, session_id);
  RequestLayoutRebuild();
}

std::string LayoutOrchestrator::GetWindowTitle(const std::string& card_id,
                                               size_t session_id) const {
  if (!panel_manager_) {
    return "";
  }
  return panel_manager_->GetPanelWindowName(session_id, card_id);
}

std::vector<std::string> LayoutOrchestrator::GetVisiblePanels(
    size_t session_id) const {
  // Return empty since this requires more complex session handling
  // This can be implemented later when session-aware panel visibility is needed
  return {};
}

void LayoutOrchestrator::ShowPresetPanels(const PanelLayoutPreset& preset,
                                          size_t session_id,
                                          EditorType editor_type) {
  if (!panel_manager_) {
    return;
  }

  for (const auto& panel_id : preset.default_visible_panels) {
    panel_manager_->ShowPanel(session_id, panel_id);
  }
}

void LayoutOrchestrator::HideOptionalPanels(EditorType type,
                                            size_t session_id) {
  if (!panel_manager_) {
    return;
  }

  auto preset = LayoutPresets::GetDefaultPreset(type);
  for (const auto& panel_id : preset.optional_panels) {
    panel_manager_->HidePanel(session_id, panel_id);
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

std::string LayoutOrchestrator::GetPrefixedPanelId(const std::string& card_id,
                                                   size_t session_id) const {
  if (panel_manager_) {
    return panel_manager_->MakePanelId(session_id, card_id);
  }
  if (session_id == 0) {
    return card_id;
  }
  return absl::StrFormat("s%zu.%s", session_id, card_id);
}

}  // namespace editor
}  // namespace yaze
