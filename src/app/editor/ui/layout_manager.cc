#include "app/editor/ui/layout_manager.h"

#include "app/editor/system/editor_card_registry.h"
#include "app/editor/ui/layout_presets.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/log.h"

namespace yaze {
namespace editor {

namespace {

// Helper function to show default cards from LayoutPresets
void ShowDefaultCardsForEditor(EditorCardRegistry* registry, EditorType type) {
  if (!registry) return;

  auto default_cards = LayoutPresets::GetDefaultCards(type);
  for (const auto& card_id : default_cards) {
    registry->ShowCard(card_id);
  }

  LOG_INFO("LayoutManager", "Showing %zu default cards for editor type %d",
           default_cards.size(), static_cast<int>(type));
}

}  // namespace

void LayoutManager::InitializeEditorLayout(EditorType type,
                                           ImGuiID dockspace_id) {
  // Don't reinitialize if already set up
  if (IsLayoutInitialized(type)) {
    LOG_INFO("LayoutManager",
             "Layout for editor type %d already initialized, skipping",
             static_cast<int>(type));
    return;
  }

  // Store dockspace ID and current editor type for potential rebuilds
  last_dockspace_id_ = dockspace_id;
  current_editor_type_ = type;

  LOG_INFO("LayoutManager", "Initializing layout for editor type %d",
           static_cast<int>(type));

  // Clear existing layout for this dockspace
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

  // Build layout based on editor type
  switch (type) {
    case EditorType::kOverworld:
      BuildOverworldLayout(dockspace_id);
      break;
    case EditorType::kDungeon:
      BuildDungeonLayout(dockspace_id);
      break;
    case EditorType::kGraphics:
      BuildGraphicsLayout(dockspace_id);
      break;
    case EditorType::kPalette:
      BuildPaletteLayout(dockspace_id);
      break;
    case EditorType::kScreen:
      BuildScreenLayout(dockspace_id);
      break;
    case EditorType::kMusic:
      BuildMusicLayout(dockspace_id);
      break;
    case EditorType::kSprite:
      BuildSpriteLayout(dockspace_id);
      break;
    case EditorType::kMessage:
      BuildMessageLayout(dockspace_id);
      break;
    case EditorType::kAssembly:
      BuildAssemblyLayout(dockspace_id);
      break;
    case EditorType::kSettings:
      BuildSettingsLayout(dockspace_id);
      break;
    case EditorType::kEmulator:
      BuildEmulatorLayout(dockspace_id);
      break;
    default:
      LOG_WARN("LayoutManager", "No layout defined for editor type %d",
               static_cast<int>(type));
      break;
  }

  // Show default cards from LayoutPresets (single source of truth)
  ShowDefaultCardsForEditor(card_registry_, type);

  // Finalize the layout
  ImGui::DockBuilderFinish(dockspace_id);

  // Mark as initialized
  MarkLayoutInitialized(type);
}

void LayoutManager::RebuildLayout(EditorType type, ImGuiID dockspace_id) {
  // Validate dockspace exists
  ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
  if (!node) {
    LOG_ERROR("LayoutManager",
              "Cannot rebuild layout: dockspace ID %u not found", dockspace_id);
    return;
  }

  LOG_INFO("LayoutManager", "Forcing rebuild of layout for editor type %d",
           static_cast<int>(type));

  // Store dockspace ID and current editor type
  last_dockspace_id_ = dockspace_id;
  current_editor_type_ = type;

  // Clear the layout initialization flag to force rebuild
  layouts_initialized_[type] = false;

  // Clear existing layout for this dockspace
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

  // Build layout based on editor type
  switch (type) {
    case EditorType::kOverworld:
      BuildOverworldLayout(dockspace_id);
      break;
    case EditorType::kDungeon:
      BuildDungeonLayout(dockspace_id);
      break;
    case EditorType::kGraphics:
      BuildGraphicsLayout(dockspace_id);
      break;
    case EditorType::kPalette:
      BuildPaletteLayout(dockspace_id);
      break;
    case EditorType::kScreen:
      BuildScreenLayout(dockspace_id);
      break;
    case EditorType::kMusic:
      BuildMusicLayout(dockspace_id);
      break;
    case EditorType::kSprite:
      BuildSpriteLayout(dockspace_id);
      break;
    case EditorType::kMessage:
      BuildMessageLayout(dockspace_id);
      break;
    case EditorType::kAssembly:
      BuildAssemblyLayout(dockspace_id);
      break;
    case EditorType::kSettings:
      BuildSettingsLayout(dockspace_id);
      break;
    case EditorType::kEmulator:
      BuildEmulatorLayout(dockspace_id);
      break;
    default:
      LOG_WARN("LayoutManager", "No layout defined for editor type %d",
               static_cast<int>(type));
      break;
  }

  // Show default cards from LayoutPresets (single source of truth)
  ShowDefaultCardsForEditor(card_registry_, type);

  // Finalize the layout
  ImGui::DockBuilderFinish(dockspace_id);

  // Mark as initialized
  MarkLayoutInitialized(type);

  LOG_INFO("LayoutManager", "Layout rebuild complete for editor type %d",
           static_cast<int>(type));
}

void LayoutManager::BuildOverworldLayout(ImGuiID dockspace_id) {
  // Default Overworld Editor Layout:
  // - Center 75%: Overworld Canvas (main editing area, maximized)
  // - Right 25%: Tile16 Selector
  //
  // Other cards (Tile8, Area Graphics, GFX Groups, etc.) start hidden
  // and can be opened via sidebar or View menu

  ImGuiID dock_center_id = 0;
  ImGuiID dock_right_id = 0;

  // Split dockspace: Center 75% | Right 25%
  dock_right_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right,
                                              0.25f, nullptr, &dock_center_id);

  // Dock main windows
  ImGui::DockBuilderDockWindow(" Overworld Canvas", dock_center_id);
  ImGui::DockBuilderDockWindow(" Tile16 Selector", dock_right_id);

  // Note: Other cards (Tile8 Selector, Area Graphics, Scratch Pad, GFX Groups)
  // are not docked by default - they can be opened from the sidebar/menu
}

void LayoutManager::BuildDungeonLayout(ImGuiID dockspace_id) {
  // Default Dungeon Editor Layout:
  // - Left 15%: Room Selector (list of dungeon rooms)
  // - Center 85%: Room Canvas (main editing area, maximized)
  //
  // Other cards (Object Editor, Entrances, Palette Editor, Room Matrix) start hidden

  ImGuiID dock_left_id = 0;
  ImGuiID dock_center_id = 0;

  // Split dockspace: Left 15% | Center 85%
  dock_left_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.15f,
                                             nullptr, &dock_center_id);

  // Dock main windows
  ImGui::DockBuilderDockWindow(" Rooms List", dock_left_id);
  ImGui::DockBuilderDockWindow(" Dungeon Controls", dock_center_id);

  // Note: Other cards (Object Editor, Entrances, Palette Editor, Room Matrix, Room Graphics)
  // are not docked by default - they can be opened from the sidebar/menu
}

void LayoutManager::BuildGraphicsLayout(ImGuiID dockspace_id) {
  // Default Graphics Editor Layout:
  // - Left 25%: Sheet Browser (list of all GFX sheets)
  // - Center 75%: Sheet Editor (main editing canvas with tabs)
  //
  // Other cards (Player Animations, Prototype Viewer) start hidden

  ImGuiID dock_left_id = 0;
  ImGuiID dock_center_id = 0;

  // Split dockspace: Left 25% | Center 75%
  dock_left_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f,
                                             nullptr, &dock_center_id);

  // Dock main windows
  ImGui::DockBuilderDockWindow(" GFX Sheets", dock_left_id);
  ImGui::DockBuilderDockWindow(" Sheet Editor", dock_center_id);

  // Note: Player Animations and Prototype Viewer are not docked by default
  // They can be opened from the sidebar/menu
}

void LayoutManager::BuildPaletteLayout(ImGuiID dockspace_id) {
  // Layout:
  // - Left 20%: Control Panel
  // - Center 80%: Palette Editor (maximized)
  //
  // Other cards (ROM Browser, SNES Palette, Color Harmony) start hidden

  ImGuiID dock_left_id = 0;
  ImGuiID dock_center_id = 0;

  // Split dockspace: Left 20% | Center 80%
  dock_left_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.20f,
                                             nullptr, &dock_center_id);

  // Dock windows
  ImGui::DockBuilderDockWindow(" Palette Control Panel", dock_left_id);
  ImGui::DockBuilderDockWindow(" OW Main Palette", dock_center_id);
}

void LayoutManager::BuildScreenLayout(ImGuiID dockspace_id) {
  // Layout:
  // - Grid layout with Dungeon Map Editor in center
  // - Title Screen, Inventory Menu, Naming Screen as tabs or separate

  ImGuiID dock_main_id = dockspace_id;
  ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(
      dock_main_id, ImGuiDir_Right, 0.40f, nullptr, &dock_main_id);
  
  ImGuiID dock_right_top = 0;
  ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(
      dock_right_id, ImGuiDir_Down, 0.50f, nullptr, &dock_right_top);

  // Dock windows
  ImGui::DockBuilderDockWindow(" Dungeon Map Editor", dock_main_id);
  ImGui::DockBuilderDockWindow(" Title Screen", dock_right_top);
  ImGui::DockBuilderDockWindow(" Inventory Menu", dock_right_bottom);
  ImGui::DockBuilderDockWindow(" Naming Screen", dock_right_bottom);
}

void LayoutManager::BuildMusicLayout(ImGuiID dockspace_id) {
  // Layout:
  // - Left 30%: Music Tracker
  // - Center 45%: Instrument Editor
  // - Right 25%: Assembly/Export

  ImGuiID dock_left_id = 0;
  ImGuiID dock_center_id = 0;
  ImGuiID dock_right_id = 0;

  // Split dockspace
  dock_left_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.30f,
                                             nullptr, &dockspace_id);
  dock_right_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right,
                                              0.36f, nullptr, &dockspace_id);
  dock_center_id = dockspace_id;

  // Dock windows
  ImGui::DockBuilderDockWindow(" Music Tracker", dock_left_id);
  ImGui::DockBuilderDockWindow(" Instrument Editor", dock_center_id);
  ImGui::DockBuilderDockWindow(" Music Assembly", dock_right_id);
}

void LayoutManager::BuildSpriteLayout(ImGuiID dockspace_id) {
  // Layout:
  // - Left 50%: Vanilla Sprites
  // - Right 50%: Custom Sprites

  ImGuiID dock_left_id = 0;
  ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(
      dockspace_id, ImGuiDir_Right, 0.50f, nullptr, &dock_left_id);

  // Dock windows
  ImGui::DockBuilderDockWindow(" Vanilla Sprites", dock_left_id);
  ImGui::DockBuilderDockWindow(" Custom Sprites", dock_right_id);
}

void LayoutManager::BuildMessageLayout(ImGuiID dockspace_id) {
  // Layout:
  // - Left 25%: Message List
  // - Center 50%: Message Editor
  // - Right 25%: Font Atlas (top) + Dictionary (bottom)

  ImGuiID dock_left_id = 0;
  ImGuiID dock_center_id = 0;
  ImGuiID dock_right_id = 0;

  // Split dockspace
  dock_left_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f,
                                             nullptr, &dockspace_id);
  dock_right_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right,
                                              0.33f, nullptr, &dockspace_id);
  dock_center_id = dockspace_id;

  // Split right panel
  ImGuiID dock_right_top = 0;
  ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(
      dock_right_id, ImGuiDir_Down, 0.50f, nullptr, &dock_right_top);

  // Dock windows
  ImGui::DockBuilderDockWindow(" Message List", dock_left_id);
  ImGui::DockBuilderDockWindow(" Message Editor", dock_center_id);
  ImGui::DockBuilderDockWindow(" Font Atlas", dock_right_top);
  ImGui::DockBuilderDockWindow(" Dictionary", dock_right_bottom);
}

void LayoutManager::BuildAssemblyLayout(ImGuiID dockspace_id) {
  // Layout:
  // - Left 60%: Code Editor
  // - Right 40%: Output/Errors (top) + Documentation (bottom)

  ImGuiID dock_left_id = 0;
  ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(
      dockspace_id, ImGuiDir_Right, 0.40f, nullptr, &dock_left_id);

  // Split right panel
  ImGuiID dock_right_top = 0;
  ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(
      dock_right_id, ImGuiDir_Down, 0.50f, nullptr, &dock_right_top);

  // Dock windows
  ImGui::DockBuilderDockWindow(" Assembly Editor", dock_left_id);
  ImGui::DockBuilderDockWindow(" Assembly Output", dock_right_top);
  ImGui::DockBuilderDockWindow(" Assembly Docs", dock_right_bottom);
}

void LayoutManager::BuildSettingsLayout(ImGuiID dockspace_id) {
  // Layout:
  // - Left 25%: Category navigation
  // - Right 75%: Settings content

  ImGuiID dock_left_id = 0;
  ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(
      dockspace_id, ImGuiDir_Right, 0.75f, nullptr, &dock_left_id);

  // Dock windows
  ImGui::DockBuilderDockWindow(" Settings Navigation", dock_left_id);
  ImGui::DockBuilderDockWindow(" Settings Content", dock_right_id);
}

void LayoutManager::BuildEmulatorLayout(ImGuiID dockspace_id) {
  // Layout:
  // - Center 100%: PPU Viewer (Game Screen, maximized)
  //
  // Other cards (CPU Debugger, Memory Viewer, etc.) start hidden

  // Just dock the PPU viewer in the full dockspace - maximized game screen
  ImGui::DockBuilderDockWindow(" PPU Viewer", dockspace_id);
}

void LayoutManager::SaveCurrentLayout(const std::string& name) {
  // TODO: [EditorManagerRefactor] Implement layout saving to file
  // Use ImGui::SaveIniSettingsToMemory() and write to custom file
  LOG_INFO("LayoutManager", "Saving layout: %s", name.c_str());
}

void LayoutManager::LoadLayout(const std::string& name) {
  // TODO: [EditorManagerRefactor] Implement layout loading from file
  // Use ImGui::LoadIniSettingsFromMemory() and read from custom file
  LOG_INFO("LayoutManager", "Loading layout: %s", name.c_str());
}

void LayoutManager::ResetToDefaultLayout(EditorType type) {
  layouts_initialized_[type] = false;
  LOG_INFO("LayoutManager", "Reset layout for editor type %d",
           static_cast<int>(type));
}

bool LayoutManager::IsLayoutInitialized(EditorType type) const {
  auto it = layouts_initialized_.find(type);
  return it != layouts_initialized_.end() && it->second;
}

void LayoutManager::MarkLayoutInitialized(EditorType type) {
  layouts_initialized_[type] = true;
  LOG_INFO("LayoutManager", "Marked layout for editor type %d as initialized",
           static_cast<int>(type));
}

void LayoutManager::ClearInitializationFlags() {
  layouts_initialized_.clear();
  LOG_INFO("LayoutManager", "Cleared all layout initialization flags");
}

std::string LayoutManager::GetWindowTitle(const std::string& card_id) const {
  if (!card_registry_) {
    return "";
  }

  // Look up the card info in the registry (using session_id 0 for global lookup)
  auto* info = card_registry_->GetCardInfo(0, card_id);
  if (info) {
    return info->GetWindowTitle();
  }
  return "";
}

}  // namespace editor
}  // namespace yaze
