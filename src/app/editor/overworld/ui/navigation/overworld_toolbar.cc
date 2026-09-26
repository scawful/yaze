#include "app/editor/overworld/ui/navigation/overworld_toolbar.h"
#include "util/i18n/tr.h"

#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/overworld/maps/overworld_map_metadata.h"
#include "app/editor/system/commands/shortcut_manager.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::editor {

namespace {

// Extra width required before a folded view group comes back, so a canvas
// resized right at the boundary does not toggle every frame.
constexpr float kToolbarUnfoldHysteresis = 32.0f;

struct EntityModeButton {
  EntityEditMode mode;
  const char* icon;
  const char* tooltip;
};

constexpr EntityModeButton kEntityModeButtons[] = {
    {EntityEditMode::ENTRANCES, ICON_MD_DOOR_FRONT,
     "Entrances (3)\nOnly entrances respond to hover and drag"},
    {EntityEditMode::EXITS, ICON_MD_DOOR_BACK,
     "Exits (4)\nOnly exits respond to hover and drag"},
    {EntityEditMode::ITEMS, ICON_MD_GRASS,
     "Items (5)\nOnly items respond to hover and drag.\n"
     "Arrows nudge the selected item (Shift = 16px), Cmd/Ctrl+D duplicates"},
    {EntityEditMode::SPRITES, ICON_MD_PEST_CONTROL_RODENT,
     "Sprites (6)\nOnly sprites respond to hover and drag"},
};

void GroupSeparator() {
  ImGui::SameLine(0, 6.0f);
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float height = ImGui::GetFrameHeight();
  ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + 3.0f),
                                      ImVec2(pos.x, pos.y + height - 3.0f),
                                      ImGui::GetColorU32(ImGuiCol_Separator));
  ImGui::Dummy(ImVec2(1.0f, height));
  ImGui::SameLine(0, 6.0f);
}

}  // namespace

std::string OverworldToolbar::WithHint(const char* text,
                                       const char* shortcut_name) const {
  if (!shortcuts || !shortcut_name) {
    return text;
  }
  const std::string keys = shortcuts->GetDisplayString(shortcut_name);
  if (keys.empty()) {
    return text;
  }
  // Put the hint on the first line: "Zoom In (=)\n<details>".
  std::string result = text;
  const size_t newline = result.find('\n');
  const std::string hint = " (" + keys + ")";
  if (newline == std::string::npos) {
    return result + hint;
  }
  return result.insert(newline, hint);
}

void OverworldToolbar::DrawViewControls(bool in_menu) {
  const auto toggle = [&](const char* icon, const char* label,
                          const char* shortcut, const std::function<bool()>& on,
                          const std::function<void()>& flip) {
    if (!on || !flip) {
      return;
    }
    const std::string tip = WithHint(label, shortcut);
    if (in_menu) {
      const std::string keys =
          shortcuts && shortcut ? shortcuts->GetDisplayString(shortcut) : "";
      const std::string item = absl::StrFormat("%s %s", icon, label);
      if (ImGui::MenuItem(item.c_str(), keys.empty() ? nullptr : keys.c_str(),
                          on())) {
        flip();
      }
      return;
    }
    if (gui::ToolbarIconButton(icon, tip.c_str(), on())) {
      flip();
    }
    ImGui::SameLine(0, 2);
  };
  const auto action = [&](const char* icon, const char* label,
                          const char* shortcut,
                          const std::function<void()>& run) {
    if (!run) {
      return;
    }
    if (in_menu) {
      const std::string keys =
          shortcuts && shortcut ? shortcuts->GetDisplayString(shortcut) : "";
      const std::string item = absl::StrFormat("%s %s", icon, label);
      if (ImGui::MenuItem(item.c_str(),
                          keys.empty() ? nullptr : keys.c_str())) {
        run();
      }
      return;
    }
    const std::string tip = WithHint(label, shortcut);
    if (gui::ToolbarIconButton(icon, tip.c_str())) {
      run();
    }
    ImGui::SameLine(0, 2);
  };

  toggle(ICON_MD_GRID_4X4, "Grid", "overworld.toggle_grid", is_grid_visible,
         on_toggle_grid);
  toggle(ICON_MD_PLACE, "Entities\nShow entrances, exits, items and sprites",
         "overworld.toggle_entities", are_entities_visible, on_toggle_entities);
  toggle(ICON_MD_VISIBILITY, "Overlay preview\nDraw this map's overlay",
         nullptr, is_overlay_preview_enabled, on_toggle_overlay_preview);
  if (in_menu) {
    ImGui::Separator();
  }
  action(ICON_MD_ZOOM_OUT, "Zoom out\nCmd/Ctrl+wheel zooms at the cursor",
         "overworld.zoom_out", on_zoom_out);
  if (!in_menu && get_zoom) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%3.0f%%", get_zoom() * 100.0f);
    ImGui::SameLine(0, 2);
  }
  action(ICON_MD_ZOOM_IN, "Zoom in\nCmd/Ctrl+wheel zooms at the cursor",
         "overworld.zoom_in", on_zoom_in);
  action(ICON_MD_FIT_SCREEN, "Zoom to fit world", "overworld.zoom_fit",
         on_zoom_fit);
  action(ICON_MD_CENTER_FOCUS_STRONG, "Center on selected map",
         "overworld.center_map", on_center_map);
}

void OverworldToolbar::Draw(int& current_world, int& current_map,
                            bool& current_map_lock, EditingMode& current_mode,
                            EntityEditMode& entity_edit_mode,
                            WorkspaceWindowManager* window_manager, Rom* rom,
                            zelda3::Overworld* overworld,
                            project::YazeProject* project, int game_state) {
  if (!overworld || !overworld->is_loaded() || !window_manager || !rom) {
    return;
  }
  if (!overworld->overworld_map(current_map)) {
    return;
  }

  const float avail = ImGui::GetContentRegionAvail().x;
  if (!compact_ && full_width_ > 0.0f && avail < full_width_) {
    compact_ = true;
  } else if (compact_ && avail >= full_width_ + kToolbarUnfoldHysteresis) {
    compact_ = false;
  }

  gui::StyleVarGuard toolbar_style_guard(
      {{ImGuiStyleVar_FramePadding, ImVec2(6.0f, 5.0f)},
       {ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f)}});
  ImGui::PushID("OverworldToolbar");
  const float row_start_x = ImGui::GetCursorPosX();

  // --- World -----------------------------------------------------------------
  static constexpr const char* kWorldButtons[] = {"LW", "DW", "SW"};
  static constexpr const char* kWorldShortcuts[] = {"overworld.world_light",
                                                    "overworld.world_dark",
                                                    "overworld.world_special"};
  for (int world_index = 0; world_index < 3; ++world_index) {
    if (world_index > 0) {
      ImGui::SameLine(0, 2);
    }
    if (gui::ToggleButton(kWorldButtons[world_index],
                          current_world == world_index, ImVec2(30.0f, 0.0f)) &&
        on_world_changed) {
      on_world_changed(world_index);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "%s", WithHint(kWorldNames[world_index], kWorldShortcuts[world_index])
                    .c_str());
    }
  }

  // --- Map -------------------------------------------------------------------
  GroupSeparator();
  const auto metadata = BuildOverworldMapMetadata(*overworld, rom, project,
                                                  current_map, game_state);
  if (ImGui::Button(metadata.map_id_label.c_str()) && on_open_map_properties) {
    on_open_map_properties();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s\n%s\nClick or double-click the map to open Map Properties.\n"
        "%s / %s: adjacent map",
        metadata.map_title.c_str(), metadata.area_size_label.c_str(),
        WithHint("Previous", "overworld.map_left").c_str(),
        WithHint("Next", "overworld.map_right").c_str());
  }
  ImGui::SameLine(0, 2);
  const std::string pin_tip =
      current_map_lock
          ? WithHint("Unpin map\nResume following the cursor",
                     "overworld.toggle_lock")
          : WithHint("Pin map\nKeep properties on this map while navigating",
                     "overworld.toggle_lock");
  if (gui::ToolbarIconButton(
          current_map_lock ? ICON_MD_LOCK : ICON_MD_LOCK_OPEN, pin_tip.c_str(),
          current_map_lock)) {
    current_map_lock = !current_map_lock;
  }

  // --- Tool ------------------------------------------------------------------
  GroupSeparator();
  const auto set_mode = [&](EditingMode mode) {
    if (on_set_mode) {
      on_set_mode(mode);
    } else {
      current_mode = mode;
    }
  };
  if (gui::ToolbarIconButton(
          ICON_MD_MOUSE,
          "Select (1)\nLeft-drag empty map: pan   Middle-drag: pan\n"
          "Drag entities to move them   Right-click: map menu\n"
          "Double-click a map: Map Properties",
          current_mode == EditingMode::MOUSE)) {
    set_mode(EditingMode::MOUSE);
  }
  ImGui::SameLine(0, 2);
  const std::string brush_tip = WithHint(
      "Brush (2)\nLeft-drag: paint the selected tile16\n"
      "Right-click: sample tile16 (or I)   Right-drag: capture a brush\n"
      "[ / ]: previous / next tile16   Shift+Right-click: map menu",
      "overworld.brush_toggle");
  if (gui::ToolbarIconButton(ICON_MD_DRAW, brush_tip.c_str(),
                             current_mode == EditingMode::DRAW_TILE)) {
    set_mode(EditingMode::DRAW_TILE);
  }
  ImGui::SameLine(0, 2);
  const std::string fill_tip = WithHint(
      "Fill screen\nClick fills the 32x32 screen under the cursor\n"
      "Right-click: sample tile16   Shift+Right-click: map menu",
      "overworld.fill");
  if (gui::ToolbarIconButton(ICON_MD_FORMAT_COLOR_FILL, fill_tip.c_str(),
                             current_mode == EditingMode::FILL_TILE)) {
    set_mode(EditingMode::FILL_TILE);
  }

  // --- Entity modes ------------------------------------------------------------
  GroupSeparator();
  for (const auto& button : kEntityModeButtons) {
    const bool active = entity_edit_mode == button.mode;
    const std::string tip =
        std::string(button.tooltip) + (active ? "\nClick again for all" : "");
    if (gui::ToolbarIconButton(button.icon, tip.c_str(), active)) {
      const EntityEditMode next = active ? EntityEditMode::NONE : button.mode;
      if (on_set_entity_mode) {
        on_set_entity_mode(next);
      } else {
        entity_edit_mode = next;
      }
    }
    ImGui::SameLine(0, 2);
  }

  // --- View (folds into "More" when narrow) ------------------------------------
  if (!compact_) {
    GroupSeparator();
    DrawViewControls(/*in_menu=*/false);
  }

  // --- Panels ----------------------------------------------------------------
  GroupSeparator();
  const size_t session_id = window_manager->GetActiveSessionId();
  if (compact_) {
    if (gui::ToolbarIconButton(ICON_MD_MORE_HORIZ,
                               "View\nGrid, entities, overlay and zoom")) {
      ImGui::OpenPopup("OverworldViewMenu");
    }
    if (ImGui::BeginPopup("OverworldViewMenu")) {
      DrawViewControls(/*in_menu=*/true);
      ImGui::EndPopup();
    }
    ImGui::SameLine(0, 2);
  }
  if (gui::ToolbarIconButton(ICON_MD_APPS, "Overworld windows")) {
    ImGui::OpenPopup("OverworldWindowsPopup");
  }
  if (ImGui::BeginPopup("OverworldWindowsPopup")) {
    const auto item = [&](const char* label, const char* panel_id,
                          const char* shortcut_name) {
      const std::string keys = shortcuts && shortcut_name
                                   ? shortcuts->GetDisplayString(shortcut_name)
                                   : "";
      if (ImGui::MenuItem(label, keys.empty() ? nullptr : keys.c_str(),
                          window_manager->IsWindowOpen(panel_id))) {
        window_manager->ToggleWindow(session_id, panel_id);
      }
    };
    item(ICON_MD_TUNE " Map Properties", OverworldPanelIds::kMapProperties,
         nullptr);
    item(ICON_MD_GRID_ON " Tile16 Selector", OverworldPanelIds::kTile16Selector,
         nullptr);
    item(ICON_MD_EDIT " Tile16 Editor", OverworldPanelIds::kTile16Editor,
         "overworld.toggle_tile16_editor");
    item(ICON_MD_GRID_VIEW " Tile8 Selector", OverworldPanelIds::kTile8Selector,
         nullptr);
    item(ICON_MD_IMAGE " Area Graphics", OverworldPanelIds::kAreaGraphics,
         nullptr);
    item(ICON_MD_LAYERS " GFX Groups", OverworldPanelIds::kGfxGroups, nullptr);
    item(ICON_MD_LIST " Item List", OverworldPanelIds::kItemList,
         "overworld.toggle_item_list");
    item(ICON_MD_AUTO_FIX_HIGH " Scratch Workspace",
         OverworldPanelIds::kScratchSpace, nullptr);
    item(ICON_MD_ANALYTICS " Usage Statistics", OverworldPanelIds::kUsageStats,
         nullptr);
    ImGui::EndPopup();
  }
  ImGui::SameLine(0, 2);
  if (gui::ToolbarIconButton(
          ICON_MD_TUNE,
          "Map Properties\nGraphics, palettes, message, music and area size",
          window_manager->IsWindowOpen(OverworldPanelIds::kMapProperties))) {
    window_manager->ToggleWindow(session_id, OverworldPanelIds::kMapProperties);
  }

  const auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom);
  if (rom_version == zelda3::OverworldVersion::kVanilla &&
      on_upgrade_rom_version) {
    ImGui::SameLine(0, 6);
    if (gui::PrimaryButton(ICON_MD_UPGRADE " Upgrade")) {
      on_upgrade_rom_version(3);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "%s", tr("Upgrade ROM to ZSCustomOverworld v3\n"
                   "Enables wide/tall areas, custom palettes and more"));
    }
  }

  // Natural width of the full row, measured only while unfolded.
  if (!compact_) {
    full_width_ = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x -
                  row_start_x + ImGui::GetScrollX();
  }
  ImGui::PopID();
}

}  // namespace yaze::editor
