#include "app/editor/ui/layout_manager.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/log.h"

namespace yaze {
namespace editor {

void LayoutManager::InitializeEditorLayout(EditorType type,
                                           ImGuiID dockspace_id) {
  // Don't reinitialize if already set up
  if (IsLayoutInitialized(type)) {
    LOG_INFO("LayoutManager",
             "Layout for editor type %d already initialized, skipping",
             static_cast<int>(type));
    return;
  }

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
    default:
      LOG_WARN("LayoutManager", "No layout defined for editor type %d",
               static_cast<int>(type));
      break;
  }

  // Finalize the layout
  ImGui::DockBuilderFinish(dockspace_id);

  // Mark as initialized
  MarkLayoutInitialized(type);
}

void LayoutManager::BuildOverworldLayout(ImGuiID dockspace_id) {
  // TODO: [EditorManagerRefactor] Implement DockBuilder layout for Overworld
  // Editor
  //
  // Desired layout:
  // - Left 25%: Tile16 Selector (top 50%) + Tile8 Selector (bottom 50%)
  // - Center 60%: Main Canvas (full height)
  // - Right 15%: Area Graphics (top 60%) + Scratch Pad (bottom 40%)
  //
  // Additional floating cards:
  // - Tile16 Editor (floating, 800x600)
  // - GFX Groups (floating, 700x550)
  // - Usage Stats (floating, 600x500)
  // - V3 Settings (floating, 500x600)

  ImGuiID dock_left_id = 0;
  ImGuiID dock_center_id = 0;
  ImGuiID dock_right_id = 0;

  // Split dockspace: Left 25% | Center 60% | Right 15%
  dock_left_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f,
                                             nullptr, &dockspace_id);
  dock_right_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right,
                                              0.20f, nullptr, &dockspace_id);
  dock_center_id = dockspace_id;  // Center is what remains

  // Split left panel: Tile16 (top) and Tile8 (bottom)
  ImGuiID dock_left_top = 0;
  ImGuiID dock_left_bottom = ImGui::DockBuilderSplitNode(
      dock_left_id, ImGuiDir_Down, 0.50f, nullptr, &dock_left_top);

  // Split right panel: Area Graphics (top) and Scratch Pad (bottom)
  ImGuiID dock_right_top = 0;
  ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(
      dock_right_id, ImGuiDir_Down, 0.40f, nullptr, &dock_right_top);

  // Dock windows to their designated nodes
  ImGui::DockBuilderDockWindow(" Overworld Canvas", dock_center_id);
  ImGui::DockBuilderDockWindow(" Tile16 Selector", dock_left_top);
  ImGui::DockBuilderDockWindow(" Tile8 Selector", dock_left_bottom);
  ImGui::DockBuilderDockWindow(" Area Graphics", dock_right_top);
  ImGui::DockBuilderDockWindow(" Scratch Pad", dock_right_bottom);

  // Note: Floating windows (Tile16 Editor, GFX Groups, etc.) are not docked
  // They will appear as floating windows with their configured default
  // positions
}

void LayoutManager::BuildDungeonLayout(ImGuiID dockspace_id) {
  // TODO: [EditorManagerRefactor] Implement DockBuilder layout for Dungeon
  // Editor
  //
  // Desired layout:
  // - Left 20%: Room Selector (top 60%) + Entrances (bottom 40%)
  // - Center 65%: Room Canvas + Tabs for multiple rooms
  // - Right 15%: Object Editor (top) + Palette Editor (bottom)

  ImGuiID dock_left_id = 0;
  ImGuiID dock_center_id = 0;
  ImGuiID dock_right_id = 0;

  // Split dockspace: Left 20% | Center 65% | Right 15%
  dock_left_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.20f,
                                             nullptr, &dockspace_id);
  dock_right_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right,
                                              0.19f, nullptr, &dockspace_id);
  dock_center_id = dockspace_id;

  // Split left panel: Room Selector (top 60%) and Entrances (bottom 40%)
  ImGuiID dock_left_top = 0;
  ImGuiID dock_left_bottom = ImGui::DockBuilderSplitNode(
      dock_left_id, ImGuiDir_Down, 0.40f, nullptr, &dock_left_top);

  // Split right panel: Object Editor (top 50%) and Palette Editor (bottom 50%)
  ImGuiID dock_right_top = 0;
  ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(
      dock_right_id, ImGuiDir_Down, 0.50f, nullptr, &dock_right_top);

  // Dock windows
  ImGui::DockBuilderDockWindow(" Rooms List", dock_left_top);
  ImGui::DockBuilderDockWindow(" Entrances", dock_left_bottom);
  ImGui::DockBuilderDockWindow(" Object Editor", dock_right_top);
  ImGui::DockBuilderDockWindow(" Palette Editor", dock_right_bottom);

  // Room tabs and Room Matrix are floating by default
  // Individual room windows (###RoomCard*) will dock together due to their
  // window class
}

void LayoutManager::BuildGraphicsLayout(ImGuiID dockspace_id) {
  // TODO: [EditorManagerRefactor] Implement DockBuilder layout for Graphics
  // Editor
  //
  // Desired layout:
  // - Left 30%: Sheet Browser
  // - Center 50%: Sheet Editor
  // - Right 20%: Animations (top) + Prototype (bottom)

  ImGuiID dock_left_id = 0;
  ImGuiID dock_center_id = 0;
  ImGuiID dock_right_id = 0;

  // Split dockspace: Left 30% | Center 50% | Right 20%
  dock_left_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.30f,
                                             nullptr, &dockspace_id);
  dock_right_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right,
                                              0.29f, nullptr, &dockspace_id);
  dock_center_id = dockspace_id;

  // Split right panel: Animations (top) and Prototype (bottom)
  ImGuiID dock_right_top = 0;
  ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(
      dock_right_id, ImGuiDir_Down, 0.50f, nullptr, &dock_right_top);

  // Dock windows
  ImGui::DockBuilderDockWindow(" GFX Sheets", dock_left_id);
  ImGui::DockBuilderDockWindow(" Sheet Editor", dock_center_id);
  ImGui::DockBuilderDockWindow(" Animations", dock_right_top);
  ImGui::DockBuilderDockWindow(" Prototype", dock_right_bottom);
}

void LayoutManager::BuildPaletteLayout(ImGuiID dockspace_id) {
  // TODO: [EditorManagerRefactor] Implement DockBuilder layout for Palette
  // Editor
  //
  // Desired layout:
  // - Left 25%: Group Manager (top) + ROM Palette Browser (bottom)
  // - Center 50%: Main Palette Editor
  // - Right 25%: SNES Palette (top) + Color Harmony Tools (bottom)

  ImGuiID dock_left_id = 0;
  ImGuiID dock_center_id = 0;
  ImGuiID dock_right_id = 0;

  // Split dockspace: Left 25% | Center 50% | Right 25%
  dock_left_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f,
                                             nullptr, &dockspace_id);
  dock_right_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right,
                                              0.33f, nullptr, &dockspace_id);
  dock_center_id = dockspace_id;

  // Split left panel: Group Manager (top) and ROM Browser (bottom)
  ImGuiID dock_left_top = 0;
  ImGuiID dock_left_bottom = ImGui::DockBuilderSplitNode(
      dock_left_id, ImGuiDir_Down, 0.50f, nullptr, &dock_left_top);

  // Split right panel: SNES Palette (top) and Color Tools (bottom)
  ImGuiID dock_right_top = 0;
  ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(
      dock_right_id, ImGuiDir_Down, 0.50f, nullptr, &dock_right_top);

  // Dock windows
  ImGui::DockBuilderDockWindow(" Group Manager", dock_left_top);
  ImGui::DockBuilderDockWindow(" ROM Palette Browser", dock_left_bottom);
  ImGui::DockBuilderDockWindow(" Palette Editor", dock_center_id);
  ImGui::DockBuilderDockWindow(" SNES Palette", dock_right_top);
  ImGui::DockBuilderDockWindow(" Color Harmony", dock_right_bottom);
}

void LayoutManager::BuildScreenLayout(ImGuiID dockspace_id) {
  // TODO: [EditorManagerRefactor] Implement DockBuilder layout for Screen
  // Editor
  //
  // Desired layout:
  // - Grid layout with Overworld Map in center (larger)
  // - Corners: Dungeon Maps, Title Screen, Inventory Menu, Naming Screen

  ImGuiID dock_top = 0;
  ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down,
                                                    0.50f, nullptr, &dock_top);

  // Split top: left and right
  ImGuiID dock_top_left = 0;
  ImGuiID dock_top_right = ImGui::DockBuilderSplitNode(
      dock_top, ImGuiDir_Right, 0.50f, nullptr, &dock_top_left);

  // Split bottom: left and right
  ImGuiID dock_bottom_left = 0;
  ImGuiID dock_bottom_right = ImGui::DockBuilderSplitNode(
      dock_bottom, ImGuiDir_Right, 0.50f, nullptr, &dock_bottom_left);

  // Dock windows in grid
  ImGui::DockBuilderDockWindow(" Dungeon Map Editor", dock_top_left);
  ImGui::DockBuilderDockWindow(" Title Screen", dock_top_right);
  ImGui::DockBuilderDockWindow(" Inventory Menu", dock_bottom_left);
  ImGui::DockBuilderDockWindow(" Naming Screen", dock_bottom_right);

  // Overworld Map could be floating or in center - let user configure
}

void LayoutManager::BuildMusicLayout(ImGuiID dockspace_id) {
  // TODO: [EditorManagerRefactor] Implement DockBuilder layout for Music Editor
  //
  // Desired layout:
  // - Left 30%: Music Tracker
  // - Center 45%: Instrument Editor
  // - Right 25%: Assembly/Export

  ImGuiID dock_left_id = 0;
  ImGuiID dock_center_id = 0;
  ImGuiID dock_right_id = 0;

  // Split dockspace: Left 30% | Center 45% | Right 25%
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
  // TODO: [EditorManagerRefactor] Implement DockBuilder layout for Sprite
  // Editor
  //
  // Desired layout:
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
  // TODO: [EditorManagerRefactor] Implement DockBuilder layout for Message
  // Editor
  //
  // Desired layout:
  // - Left 25%: Message List
  // - Center 50%: Message Editor
  // - Right 25%: Font Atlas (top) + Dictionary (bottom)

  ImGuiID dock_left_id = 0;
  ImGuiID dock_center_id = 0;
  ImGuiID dock_right_id = 0;

  // Split dockspace: Left 25% | Center 50% | Right 25%
  dock_left_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f,
                                             nullptr, &dockspace_id);
  dock_right_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right,
                                              0.33f, nullptr, &dockspace_id);
  dock_center_id = dockspace_id;

  // Split right panel: Font Atlas (top) and Dictionary (bottom)
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
  // TODO: [EditorManagerRefactor] Implement DockBuilder layout for Assembly
  // Editor
  //
  // Desired layout:
  // - Left 60%: Code Editor
  // - Right 40%: Output/Errors (top) + Documentation (bottom)

  ImGuiID dock_left_id = 0;
  ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(
      dockspace_id, ImGuiDir_Right, 0.40f, nullptr, &dock_left_id);

  // Split right panel: Output (top) and Docs (bottom)
  ImGuiID dock_right_top = 0;
  ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(
      dock_right_id, ImGuiDir_Down, 0.50f, nullptr, &dock_right_top);

  // Dock windows
  ImGui::DockBuilderDockWindow(" Assembly Editor", dock_left_id);
  ImGui::DockBuilderDockWindow(" Assembly Output", dock_right_top);
  ImGui::DockBuilderDockWindow(" Assembly Docs", dock_right_bottom);
}

void LayoutManager::BuildSettingsLayout(ImGuiID dockspace_id) {
  // TODO: [EditorManagerRefactor] Implement DockBuilder layout for Settings
  // Editor
  //
  // Desired layout:
  // - Left 25%: Category navigation (vertical list)
  // - Right 75%: Settings content for selected category

  ImGuiID dock_left_id = 0;
  ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(
      dockspace_id, ImGuiDir_Right, 0.75f, nullptr, &dock_left_id);

  // Dock windows
  ImGui::DockBuilderDockWindow(" Settings Navigation", dock_left_id);
  ImGui::DockBuilderDockWindow(" Settings Content", dock_right_id);
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

}  // namespace editor
}  // namespace yaze
