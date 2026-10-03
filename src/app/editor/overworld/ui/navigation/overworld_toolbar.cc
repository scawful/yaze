#include "app/editor/overworld/ui/navigation/overworld_toolbar.h"
#include "util/i18n/tr.h"

#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/overworld/maps/overworld_map_metadata.h"
#include "app/editor/system/commands/shortcut_manager.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::editor {

namespace {

struct EntityModeButton {
  EntityEditMode mode;
  const char* label;
  const char* tooltip;
};

constexpr EntityModeButton kEntityModeButtons[] = {
    {EntityEditMode::ENTRANCES, "Entrances",
     "Entrances (3)\nOnly entrances respond to hover and drag"},
    {EntityEditMode::EXITS, "Exits",
     "Exits (4)\nOnly exits respond to hover and drag"},
    {EntityEditMode::ITEMS, "Items",
     "Items (5)\nOnly items respond to hover and drag.\n"
     "Arrows nudge the selected item (Shift = 16px), Cmd/Ctrl+D duplicates"},
    {EntityEditMode::SPRITES, "Sprites",
     "Sprites (6)\nOnly sprites respond to hover and drag"},
};

bool ToolButton(const char* label, const char* tooltip, bool active,
                const char* automation_id = nullptr) {
  const bool clicked = gui::ToggleButton(label, active);
  if (automation_id) {
    gui::WidgetIdRegistry::Instance().RegisterWidget(
        std::string("OverworldToolbar/button:") + automation_id, "button",
        ImGui::GetItemID());
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", tooltip);
  }
  return clicked;
}

void GroupSeparator() {
  ImGui::SameLine(0, 6.0f);
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float height = ImGui::GetFrameHeight();
  ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + 3.0f),
                                      ImVec2(pos.x, pos.y + height - 3.0f),
                                      ImGui::GetColorU32(ImGuiCol_Separator));
  ImGui::Dummy(ImVec2(1.0f, 0.0f));
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
      const std::string title =
          std::string(label).substr(0, std::string(label).find('\n'));
      const std::string item = absl::StrFormat("%s %s", icon, title);
      if (ImGui::MenuItem(item.c_str(), keys.empty() ? nullptr : keys.c_str(),
                          on())) {
        flip();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tip.c_str());
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
      const std::string title =
          std::string(label).substr(0, std::string(label).find('\n'));
      const std::string item = absl::StrFormat("%s %s", icon, title);
      if (ImGui::MenuItem(item.c_str(),
                          keys.empty() ? nullptr : keys.c_str())) {
        run();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", WithHint(label, shortcut).c_str());
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
                            project::YazeProject* project, int game_state,
                            const std::function<void()>& draw_appearance) {
  if (!overworld || !overworld->is_loaded() || !window_manager || !rom) {
    return;
  }
  if (!overworld->overworld_map(current_map)) {
    return;
  }

  const float avail = ImGui::GetContentRegionAvail().x;
  const float right_edge = ImGui::GetCursorScreenPos().x + avail;
  const bool tiny = avail < ImGui::GetFontSize() * 38.0f;

  gui::StyleVarGuard toolbar_style_guard(
      {{ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f)},
       {ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f)}});
  ImGui::PushID("OverworldToolbar");

  // --- World -----------------------------------------------------------------
  static constexpr const char* kWorldButtons[] = {"LW", "DW", "SW"};
  static constexpr const char* kWorldShortcuts[] = {"overworld.world_light",
                                                    "overworld.world_dark",
                                                    "overworld.world_special"};
  for (int world_index = 0; !tiny && world_index < 3; ++world_index) {
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
  if (!tiny)
    GroupSeparator();
  const auto metadata = BuildOverworldMapMetadata(*overworld, rom, project,
                                                  current_map, game_state);
  if (ImGui::Button(metadata.map_id_label.c_str()) && on_open_map_properties) {
    on_open_map_properties();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s\n%s\nClick this button or double-click the canvas map for "
        "properties.\n"
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
  if (gui::ThemedIconButton(
          current_map_lock ? ICON_MD_PUSH_PIN : ICON_MD_GPS_FIXED,
          pin_tip.c_str(),
          ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()),
          current_map_lock)) {
    current_map_lock = !current_map_lock;
  }

  // Keep frequent map properties visible even when secondary tools overflow.
  ImGui::SameLine(0, 4);
  const std::string story_label = absl::StrFormat("Story %d", game_state);
  ImGui::SetNextItemWidth(ImGui::CalcTextSize("Story 0").x +
                          ImGui::GetFrameHeight() + 8.0f);
  const bool story_open = ImGui::BeginCombo("##Story", story_label.c_str());
  gui::WidgetIdRegistry::Instance().RegisterWidget(
      "OverworldToolbar/combo:Story", "combobox", ImGui::GetItemID());
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Story part to preview. Changes the active sprite set, "
        "sprite graphics and sprite palettes. "
        "Shared item placements stay the same.");
  }
  if (story_open) {
    for (int state = 0; state < 3; ++state) {
      const std::string label =
          absl::StrFormat("%d — %s", state, kGameStateNames[state]);
      if (ImGui::Selectable(label.c_str(), game_state == state) &&
          on_story_changed) {
        on_story_changed(state);
      }
      gui::WidgetIdRegistry::Instance().RegisterWidget(
          absl::StrFormat("OverworldToolbar/story:%d", state), "selectable",
          ImGui::GetItemID());
    }
    ImGui::EndCombo();
  }
  if (draw_appearance) {
    ImGui::SameLine(0, 4);
    draw_appearance();
  }

  const char* entity_label = "Entities";
  for (const auto& button : kEntityModeButtons) {
    if (entity_edit_mode == button.mode) {
      entity_label = button.label;
    }
  }
  const auto button_width = [](const char* label) {
    return ImGui::CalcTextSize(label, nullptr, true).x +
           ImGui::GetStyle().FramePadding.x * 2.0f;
  };
  // Match the actual buttons and gaps below. Shorten secondary labels before
  // moving tools into overflow; input fields do not imply a narrow toolbar.
  const float drawing_width =
      button_width(ICON_MD_MOUSE) + button_width(ICON_MD_DRAW) +
      button_width(ICON_MD_FORMAT_COLOR_FILL) +
      button_width(ICON_MD_GRASS " Insert Item") + 3.0f * 2.0f;
  const auto tools_width = [&](bool compact_labels) {
    return 3.0f * 13.0f + drawing_width +
           button_width(compact_labels ? ICON_MD_PLACE : entity_label) +
           button_width(compact_labels ? ICON_MD_VISIBILITY : "View") + 2.0f +
           button_width(compact_labels ? ICON_MD_WINDOW : "Windows");
  };
  const float remaining = right_edge - ImGui::GetItemRectMax().x;
  const bool compact = tools_width(false) > remaining;
  // Tiny docks also need access to the world selector through this popup.
  const bool overflow = tiny || tools_width(true) > remaining;
  if (overflow) {
    ImGui::SameLine(0, 4);
    if (ToolButton(ICON_MD_MORE_HORIZ, "Drawing tools and map options", false,
                   "Tools")) {
      ImGui::OpenPopup("ToolsOverflow");
    }
    if (!ImGui::BeginPopup("ToolsOverflow")) {
      ImGui::PopID();
      return;
    }
    if (tiny) {
      for (int world = 0; world < 3; ++world) {
        if (ImGui::MenuItem(kWorldNames[world], nullptr,
                            current_world == world) &&
            on_world_changed) {
          on_world_changed(world);
        }
      }
      ImGui::Separator();
    }
  } else {
    GroupSeparator();
  }
  // --- Tool ------------------------------------------------------------------
  const auto set_mode = [&](EditingMode mode) {
    if (on_set_mode) {
      on_set_mode(mode);
    } else {
      current_mode = mode;
    }
  };
  if (ToolButton(ICON_MD_MOUSE,
                 "Select (1)\nLeft-drag empty map: pan   Middle-drag: pan\n"
                 "Drag entities to move them   Right-click: map menu\n"
                 "Double-click a map: Map Properties",
                 current_mode == EditingMode::MOUSE, "Select")) {
    set_mode(EditingMode::MOUSE);
  }
  ImGui::SameLine(0, 2);
  const std::string brush_tip = WithHint(
      "Brush (2)\nLeft-drag: paint the selected tile16\n"
      "Right-click: sample tile16 (or I)   Right-drag: capture a brush\n"
      "[ / ]: previous / next tile16   Shift+Right-click: map menu",
      "overworld.brush_toggle");
  if (ToolButton(ICON_MD_DRAW, brush_tip.c_str(),
                 current_mode == EditingMode::DRAW_TILE, "Paint")) {
    set_mode(EditingMode::DRAW_TILE);
  }
  ImGui::SameLine(0, 2);
  const std::string fill_tip = WithHint(
      "Fill screen\nClick fills the 32x32 screen under the cursor\n"
      "Right-click: sample tile16   Shift+Right-click: map menu",
      "overworld.fill");
  if (ToolButton(ICON_MD_FORMAT_COLOR_FILL, fill_tip.c_str(),
                 current_mode == EditingMode::FILL_TILE, "Fill")) {
    set_mode(EditingMode::FILL_TILE);
  }
  ImGui::SameLine(0, 2);
  if (ToolButton(ICON_MD_GRASS " Insert Item",
                 "Pick an item from the list, then click the map to place it",
                 false, "Insert Item")) {
    if (on_insert_item) {
      on_insert_item();
    }
  }

  // --- Entity modes ------------------------------------------------------------
  GroupSeparator();
  if (ImGui::Button(compact ? ICON_MD_PLACE : entity_label)) {
    ImGui::OpenPopup("EntityTools");
  }
  gui::WidgetIdRegistry::Instance().RegisterWidget(
      "OverworldToolbar/button:Entities", "button", ImGui::GetItemID());
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Choose which entities respond to hover and drag.\n"
        "Entrances (3), exits (4), items (5), sprites (6).");
  }
  if (ImGui::BeginPopup("EntityTools")) {
    const auto choose = [&](EntityEditMode mode) {
      if (on_set_entity_mode) {
        on_set_entity_mode(mode);
      } else {
        entity_edit_mode = mode;
      }
    };
    if (ImGui::MenuItem("All entities", nullptr,
                        entity_edit_mode == EntityEditMode::NONE)) {
      choose(EntityEditMode::NONE);
    }
    for (const auto& button : kEntityModeButtons) {
      if (ImGui::MenuItem(button.label, nullptr,
                          entity_edit_mode == button.mode)) {
        choose(button.mode);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", button.tooltip);
      }
    }
    ImGui::EndPopup();
  }

  // Secondary display controls stay in a named menu, leaving space for tools.
  GroupSeparator();
  const size_t session_id = window_manager->GetActiveSessionId();
  if (ToolButton(compact ? ICON_MD_VISIBILITY : "View",
                 "Grid, entities, overlay and zoom", false, "View")) {
    ImGui::OpenPopup("OverworldViewMenu");
  }
  if (ImGui::BeginPopup("OverworldViewMenu")) {
    DrawViewControls(/*in_menu=*/true);
    ImGui::EndPopup();
  }
  ImGui::SameLine(0, 2);
  if (ToolButton(compact ? ICON_MD_WINDOW : "Windows",
                 "Open overworld editing windows", false, "Windows")) {
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
        const bool opening = !window_manager->IsWindowOpen(panel_id);
        window_manager->ToggleWindow(session_id, panel_id);
        if (opening && panel_id == OverworldPanelIds::kItemList &&
            on_set_entity_mode) {
          on_set_entity_mode(EntityEditMode::ITEMS);
        }
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
    const auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom);
    if (rom_version == zelda3::OverworldVersion::kVanilla &&
        on_upgrade_rom_version) {
      ImGui::Separator();
      if (ImGui::MenuItem(ICON_MD_UPGRADE
                          " Upgrade to ZSCustomOverworld v3...")) {
        on_upgrade_rom_version(3);
      }
    }
    ImGui::EndPopup();
  }
  if (overflow) {
    ImGui::EndPopup();
  }
  ImGui::PopID();
}

}  // namespace yaze::editor
