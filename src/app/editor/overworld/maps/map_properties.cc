#include "app/editor/overworld/maps/map_properties.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/editor/overworld/maps/overworld_map_metadata.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/ui/ui_constants.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/popup_id.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze {
namespace editor {

using ImGui::BeginTable;
// HOVER_HINT is defined in util/macro.h
using ImGui::Separator;
using ImGui::TableNextColumn;
using ImGui::Text;

// Using centralized UI constants
namespace {

bool IsValidMapId(int map_id) {
  return map_id >= 0 && map_id < zelda3::kNumOverworldMaps;
}

int EffectivePropertyMapId(zelda3::Overworld* overworld, int map_id) {
  if (!overworld || !IsValidMapId(map_id)) {
    return map_id;
  }
  auto* map = overworld->mutable_overworld_map(map_id);
  if (!map) {
    return map_id;
  }
  const int parent = map->parent();
  if (!IsValidMapId(parent)) {
    return map_id;
  }
  return parent;
}

absl::Status CheckFieldSupported(const Rom* rom,
                                 const OverworldPropertyEdit& edit) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }
  const auto version = zelda3::OverworldVersionHelper::GetVersion(*rom);
  switch (edit.field) {
    case OverworldPropertyField::kAreaSize:
      if ((edit.value == static_cast<int>(zelda3::AreaSizeEnum::WideArea) ||
           edit.value == static_cast<int>(zelda3::AreaSizeEnum::TallArea)) &&
          !zelda3::OverworldVersionHelper::SupportsAreaEnum(version)) {
        return absl::FailedPreconditionError(
            "Wide and Tall areas require ZSCustomOverworld v3+");
      }
      return absl::OkStatus();
    case OverworldPropertyField::kMainPalette:
    case OverworldPropertyField::kAreaSpecificBgColor:
    case OverworldPropertyField::kMosaicExpanded:
      if (!zelda3::OverworldVersionHelper::SupportsCustomBGColors(version)) {
        return absl::FailedPreconditionError(
            "This field requires ZSCustomOverworld v2+");
      }
      return absl::OkStatus();
    case OverworldPropertyField::kAnimatedGraphics:
      if (!zelda3::OverworldVersionHelper::SupportsAnimatedGFX(version)) {
        return absl::FailedPreconditionError(
            "Animated GFX requires ZSCustomOverworld v3+");
      }
      return absl::OkStatus();
    case OverworldPropertyField::kCustomTileset:
      if (!zelda3::OverworldVersionHelper::SupportsExpandedSpace(version)) {
        return absl::FailedPreconditionError(
            "Custom tile graphics require ZSCustomOverworld v1+");
      }
      return absl::OkStatus();
    case OverworldPropertyField::kSubscreenOverlay:
      if (version == zelda3::OverworldVersion::kVanilla) {
        return absl::FailedPreconditionError(
            "Visual effect editing requires ZSCustomOverworld v1+");
      }
      return absl::OkStatus();
    default:
      return absl::OkStatus();
  }
}

int MusicRomAddressForMapState(int map_id, int state) {
  if (map_id >= 0 && map_id < zelda3::kDarkWorldMapIdStart) {
    switch (state) {
      case 0:
        return zelda3::kOverworldMusicBeginning + map_id;
      case 1:
        return zelda3::kOverworldMusicZelda + map_id;
      case 2:
        return zelda3::kOverworldMusicMasterSword + map_id;
      case 3:
        return zelda3::kOverworldMusicAgahnim + map_id;
      default:
        return -1;
    }
  }
  if (map_id >= zelda3::kDarkWorldMapIdStart &&
      map_id < zelda3::kSpecialWorldMapIdStart && state == 0) {
    return zelda3::kOverworldMusicDarkWorld +
           (map_id - zelda3::kDarkWorldMapIdStart);
  }
  return -1;
}

bool WriteRomByteIfValid(Rom* rom, int address, uint8_t value) {
  if (!rom || address < 0 || static_cast<size_t>(address) >= rom->size()) {
    return false;
  }
  (*rom)[address] = value;
  return true;
}

void WriteRomWordIfValid(Rom* rom, int address, uint16_t value) {
  if (!rom || address < 0 || static_cast<size_t>(address + 1) >= rom->size()) {
    return;
  }
  (*rom)[address] = value & 0xFF;
  (*rom)[address + 1] = (value >> 8) & 0xFF;
}

OverworldMapMetadataClipboard CaptureMapMetadataClipboard(
    const zelda3::Overworld& overworld, int map_id) {
  OverworldMapMetadataClipboard clipboard;
  if (!IsValidMapId(map_id)) {
    return clipboard;
  }

  const auto* selected_map = overworld.overworld_map(map_id);
  if (!selected_map) {
    return clipboard;
  }

  int source_map_id = map_id;
  if (IsValidMapId(selected_map->parent())) {
    source_map_id = selected_map->parent();
  }
  const auto* map = overworld.overworld_map(source_map_id);
  if (!map) {
    return clipboard;
  }

  clipboard.valid = true;
  clipboard.source_map_id = source_map_id;
  clipboard.area_size = static_cast<int>(map->area_size());
  clipboard.area_graphics = map->area_graphics();
  clipboard.area_palette = map->area_palette();
  clipboard.main_palette = map->main_palette();
  clipboard.animated_graphics = map->animated_gfx();
  clipboard.message_id = map->message_id();
  clipboard.subscreen_overlay = map->subscreen_overlay();
  clipboard.area_specific_bg_color = map->area_specific_bg_color();
  clipboard.mosaic = *const_cast<zelda3::OverworldMap*>(map)->mutable_mosaic();
  clipboard.mosaic_expanded = map->mosaic_expanded();
  for (int i = 0; i < 3; ++i) {
    clipboard.sprite_graphics[i] = map->sprite_graphics(i);
    clipboard.sprite_palette[i] = map->sprite_palette(i);
  }
  for (int i = 0; i < 4; ++i) {
    clipboard.music[i] = map->area_music(i);
  }
  for (int i = 0; i < 8; ++i) {
    clipboard.custom_tilesets[i] = map->custom_tileset(i);
  }
  return clipboard;
}

}  // namespace

int MapPropertiesSystem::CurrentGameState() const {
  return CurrentGameState(local_game_state_);
}

int MapPropertiesSystem::CurrentGameState(int fallback) const {
  return std::clamp(game_state_ ? *game_state_ : fallback, 0, 2);
}

void MapPropertiesSystem::SetCurrentGameState(int game_state) {
  const int clamped_state = std::clamp(game_state, 0, 2);
  local_game_state_ = clamped_state;
  if (game_state_) {
    *game_state_ = clamped_state;
  }
}

void MapPropertiesSystem::PrepareMapForGraphicsRefresh(int map_index) {
  if (!overworld_ || map_index < 0 || map_index >= zelda3::kNumOverworldMaps) {
    return;
  }
  auto* map = overworld_->mutable_overworld_map(map_index);
  if (!map) {
    return;
  }
  map->set_game_state(CurrentGameState(map->game_state()));
}

void MapPropertiesSystem::DrawCustomBackgroundColorEditor(
    int current_map, bool& show_custom_bg_color_editor) {
  (void)show_custom_bg_color_editor;  // Used by caller for window state
  if (!overworld_->is_loaded()) {
    Text(tr("No overworld loaded"));
    return;
  }

  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  if (!zelda3::OverworldVersionHelper::SupportsCustomBGColors(rom_version)) {
    Text(tr("Custom background colors require ZSCustomOverworld v2+"));
    return;
  }

  Text(tr("Custom Background Color Editor"));
  Separator();

  // Read enable flag from ROM (not static - must reflect current ROM state)
  bool use_area_specific_bg_color =
      (*rom_)[zelda3::OverworldCustomAreaSpecificBGEnabled] != 0x00;
  if (ImGui::Checkbox(tr("Use Area-Specific Background Color"),
                      &use_area_specific_bg_color)) {
    // Update ROM data when checkbox is toggled
    (*rom_)[zelda3::OverworldCustomAreaSpecificBGEnabled] =
        use_area_specific_bg_color ? 0x01 : 0x00;
  }

  if (use_area_specific_bg_color) {
    // Get current color
    uint16_t current_color =
        overworld_->overworld_map(current_map)->area_specific_bg_color();
    gfx::SnesColor snes_color(current_color);

    // Convert to ImVec4 for color picker
    ImVec4 color_vec = gui::ConvertSnesColorToImVec4(snes_color);

    if (ImGui::ColorPicker4(
            "Background Color", (float*)&color_vec,
            ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex)) {
      // Convert back to SNES color and update
      gfx::SnesColor new_snes_color = gui::ConvertImVec4ToSnesColor(color_vec);
      ApplyPropertyEdit({current_map,
                         OverworldPropertyField::kAreaSpecificBgColor, 0,
                         new_snes_color.snes()});
    }

    Text(tr("SNES Color: 0x%04X"), current_color);
  }
}

void MapPropertiesSystem::DrawOverlayEditor(int current_map,
                                            bool& show_overlay_editor) {
  (void)show_overlay_editor;  // Used by caller for window state
  if (!overworld_->is_loaded()) {
    Text(tr("No overworld loaded"));
    return;
  }

  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);

  ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                     ICON_MD_LAYERS " Visual Effects Configuration");
  ImGui::Text(tr("Map: 0x%02X"), current_map);
  Separator();

  if (rom_version == zelda3::OverworldVersion::kVanilla) {
    ImGui::TextColored(
        ImVec4(1.0f, 0.8f, 0.4f, 1.0f), ICON_MD_INFO
        " Enhanced overlay editing requires ZSCustomOverworld v1+");
    ImGui::Separator();
    ImGui::TextWrapped(tr(
        "Subscreen overlays are a vanilla feature used for atmospheric effects "
        "like fog, rain, and forest canopy. ZSCustomOverworld expands this by "
        "allowing per-area overlay configuration and additional "
        "customization."));
    return;
  }

  // Help section
  if (ImGui::CollapsingHeader(ICON_MD_HELP_OUTLINE " What are Visual Effects?",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Indent();
    ImGui::TextWrapped(tr(
        "Visual effects (subscreen overlays) are semi-transparent layers drawn "
        "on top of or behind your map. They reference special area maps "
        "(0x80-0x9F) "
        "for their tile16 graphics data."));
    ImGui::Spacing();
    ImGui::Text(tr("Common uses:"));
    ImGui::BulletText(tr("Fog effects (Lost Woods, Skull Woods)"));
    ImGui::BulletText(tr("Rain (Misery Mire)"));
    ImGui::BulletText(tr("Forest canopy (Lost Woods)"));
    ImGui::BulletText(tr("Sky backgrounds (Death Mountain)"));
    ImGui::BulletText(tr("Under bridge views"));
    ImGui::Unindent();
    ImGui::Separator();
  }

  // Read enable flag from ROM (not static - must reflect current ROM state)
  bool use_subscreen_overlay =
      (*rom_)[zelda3::OverworldCustomSubscreenOverlayEnabled] != 0x00;
  if (ImGui::Checkbox(ICON_MD_VISIBILITY " Enable Visual Effect for This Area",
                      &use_subscreen_overlay)) {
    // Update ROM data when checkbox is toggled
    (*rom_)[zelda3::OverworldCustomSubscreenOverlayEnabled] =
        use_subscreen_overlay ? 0x01 : 0x00;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        tr("Enable/disable visual effect overlay for this map area"));
  }

  if (use_subscreen_overlay) {
    ImGui::Spacing();
    uint16_t current_overlay =
        overworld_->overworld_map(current_map)->subscreen_overlay();
    if (gui::InputHexWord(ICON_MD_PHOTO " Visual Effect Map ID",
                          &current_overlay, kInputFieldSize + 30)) {
      ApplyPropertyEdit({current_map, OverworldPropertyField::kSubscreenOverlay,
                         0, current_overlay});
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          tr("ID of the special area map (0x80-0x9F) to use for\n"
             "visual effects. That map's tile16 data will be drawn\n"
             "as a semi-transparent layer on this area."));
    }

    // Show description
    std::string overlay_desc = GetOverlayDescription(current_overlay);
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), ICON_MD_INFO " %s",
                       overlay_desc.c_str());

    ImGui::Separator();
    if (ImGui::CollapsingHeader(ICON_MD_LIGHTBULB
                                " Common Visual Effect IDs")) {
      ImGui::Indent();
      ImGui::BulletText(tr("0x0093 - Triforce Room Curtain"));
      ImGui::BulletText(tr("0x0094 - Under the Bridge"));
      ImGui::BulletText(tr("0x0095 - Sky Background (LW Death Mountain)"));
      ImGui::BulletText(tr("0x0096 - Pyramid Background"));
      ImGui::BulletText(tr("0x0097 - Fog Overlay (Master Sword Area)"));
      ImGui::BulletText(tr("0x009C - Lava Background (DW Death Mountain)"));
      ImGui::BulletText(tr("0x009D - Fog Overlay (Lost/Skull Woods)"));
      ImGui::BulletText(tr("0x009E - Tree Canopy (Forest)"));
      ImGui::BulletText(tr("0x009F - Rain Effect (Misery Mire)"));
      ImGui::BulletText(tr("0x00FF - No Overlay (Disabled)"));
      ImGui::Unindent();
    }
  } else {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), ICON_MD_BLOCK
                       " No visual effects enabled for this area");
  }
}

void MapPropertiesSystem::SetupCanvasContextMenu(
    gui::Canvas& canvas, const OverworldContextTarget& target,
    bool& current_map_lock, int current_mode, project::YazeProject* project,
    SharedClipboard* shared_clipboard) {
  (void)current_mode;  // Explicit context actions are independent of tool mode.
  const int map_id = target.map_id;
  const bool valid_map = target.valid();
  canvas.ClearContextMenuItems();
  // This canvas supplies one complete View menu through editor navigation.
  canvas.SetShowBuiltinContextMenu(false);

  const bool has_metadata = overworld_ && overworld_->is_loaded() &&
                            valid_map && overworld_->overworld_map(map_id);
  const auto describe = [&](int id) -> std::string {
    if (!has_metadata) {
      return absl::StrFormat("0x%02X", id);
    }
    const auto m = BuildOverworldMapMetadata(*overworld_, rom_, project, id,
                                             target.game_state);
    return m.map_name.empty()
               ? absl::StrFormat("%s | %s", m.map_id_label, m.world_label)
               : absl::StrFormat("%s | %s | %s", m.map_id_label, m.world_label,
                                 m.map_name);
  };
  const auto hint = [this](const char* name) -> std::string {
    return shortcut_hint_ && name ? shortcut_hint_(name) : std::string();
  };
  const auto add = [&canvas](gui::CanvasMenuItem item, const char* icon,
                             bool separator_after = false) {
    item.icon = icon;
    item.separator_after = separator_after;
    canvas.AddContextMenuItem(std::move(item));
  };

  // --- Header: where the click landed -------------------------------------
  auto title = gui::CanvasMenuItem::Disabled(
      valid_map ? describe(map_id) : std::string("Outside Overworld"));
  add(std::move(title), ICON_MD_MAP, !(valid_map && target.tile16_id >= 0));
  if (valid_map && target.tile16_id >= 0) {
    const int tile_x =
        static_cast<int>(target.world_position.x / kTile16Size) % 32;
    const int tile_y =
        static_cast<int>(target.world_position.y / kTile16Size) % 32;
    add(gui::CanvasMenuItem::Disabled(absl::StrFormat(
            "Tile16 0x%03X | (%d, %d)", target.tile16_id, tile_x, tile_y)),
        ICON_MD_GRID_ON, true);
  }

  // --- Tile ------------------------------------------------------------------
  const bool has_tile = valid_map && target.tile16_id >= 0;
  auto sample_item = gui::CanvasMenuItem::Conditional(
      "Sample Tile16",
      [this, target]() {
        if (sample_tile16_callback_ && target.valid() && target.tile16_id >= 0)
          (void)sample_tile16_callback_(target);
      },
      [this, has_tile]() { return bool(sample_tile16_callback_) && has_tile; });
  // I samples the hovered tile; in Brush/Fill a plain right-click does too.
  sample_item.shortcut = "I";
  add(std::move(sample_item), ICON_MD_COLORIZE);
  add(gui::CanvasMenuItem::Conditional(
          "Edit Tile16...",
          [this, target]() {
            if (edit_tile16_callback_ && target.valid() &&
                target.tile16_id >= 0)
              edit_tile16_callback_(target);
          },
          [this, has_tile]() {
            return bool(edit_tile16_callback_) && has_tile;
          }),
      ICON_MD_GRID_VIEW, true);

  // --- Map -------------------------------------------------------------------
  auto select_item = gui::CanvasMenuItem::Conditional(
      "Select This Map",
      [this, target]() {
        if (map_selection_callback_ && target.valid())
          map_selection_callback_(target.map_id, false);
      },
      [this, valid_map]() {
        return valid_map && bool(map_selection_callback_);
      });
  // Hidden while this map's area is already the current one.
  select_item.visible_condition = [this, target]() {
    if (!current_map_provider_) {
      return true;
    }
    const int current = current_map_provider_();
    if (current == target.map_id) {
      return false;
    }
    const auto* current_map = overworld_ && IsValidMapId(current)
                                  ? overworld_->overworld_map(current)
                                  : nullptr;
    return !(current_map && current_map->parent() == target.parent_map_id);
  };
  add(std::move(select_item), ICON_MD_CHECK);

  auto properties_item = gui::CanvasMenuItem::Conditional(
      "Map Properties...",
      [this, target]() {
        if (!map_selection_callback_ || !target.valid())
          return;
        map_selection_callback_(target.map_id, false);
        if (open_map_properties_callback_)
          open_map_properties_callback_();
      },
      [this, valid_map]() {
        return valid_map && bool(map_selection_callback_) &&
               bool(open_map_properties_callback_);
      });
  properties_item.shortcut = "Double-click";
  add(std::move(properties_item), ICON_MD_TUNE);

  auto pin_item = gui::CanvasMenuItem::Conditional(
      "Pin Map",
      [this, target, &current_map_lock]() {
        if (!target.valid())
          return;
        if (current_map_lock) {
          current_map_lock = false;
        } else if (map_selection_callback_) {
          map_selection_callback_(target.map_id, false);
          current_map_lock = true;
        }
      },
      [this, valid_map, &current_map_lock]() {
        return valid_map && (current_map_lock || bool(map_selection_callback_));
      });
  pin_item.checked_condition = [&current_map_lock]() {
    return current_map_lock;
  };
  pin_item.shortcut = hint("overworld.toggle_lock");
  pin_item.tooltip =
      "Pinned: the current map and its properties stay put while the cursor "
      "moves. Clicking a map still selects it.";

  // Related maps: the other screens of this area and the other world's
  // screen at the same position. Selecting one also centers it.
  gui::CanvasMenuItem related_menu;
  related_menu.label = "Related Maps";
  if (has_metadata) {
    const auto jump = [this](int id) {
      if (map_jump_callback_) {
        map_jump_callback_(id);
      } else if (map_selection_callback_) {
        map_selection_callback_(id, false);
      }
    };
    const int world_start = target.world * 0x40;
    const int world_end =
        std::min(world_start + 0x40, zelda3::kNumOverworldMaps);
    for (int id = world_start; id < world_end; ++id) {
      const auto* member = overworld_->overworld_map(id);
      if (id == map_id || !member || member->parent() != target.parent_map_id) {
        continue;
      }
      const std::string role =
          id == target.parent_map_id ? "Area parent: " : "Same area: ";
      related_menu.subitems.emplace_back(role + describe(id),
                                         [jump, id]() { jump(id); });
    }
    if (target.world < 2) {
      const int counterpart = target.world == 0 ? map_id + 0x40 : map_id - 0x40;
      if (IsValidMapId(counterpart) && overworld_->overworld_map(counterpart)) {
        if (!related_menu.subitems.empty()) {
          related_menu.subitems.back().separator_after = true;
        }
        related_menu.subitems.emplace_back(
            std::string("Other world: ") + describe(counterpart),
            [jump, counterpart]() { jump(counterpart); });
      }
    }
  }
  const bool has_related = !related_menu.subitems.empty();
  add(std::move(pin_item), ICON_MD_PUSH_PIN, !has_related);
  if (has_related) {
    related_menu.enabled_condition = [this]() {
      return bool(map_jump_callback_) || bool(map_selection_callback_);
    };
    add(std::move(related_menu), ICON_MD_ACCOUNT_TREE, true);
  }

  // --- Clipboard: area properties (graphics, palettes, music, message) ------
  if (has_metadata && shared_clipboard) {
    add(gui::CanvasMenuItem(
            "Copy Map Properties",
            [this, target, shared_clipboard]() {
              shared_clipboard->overworld_map_metadata =
                  CaptureMapMetadataClipboard(*overworld_, target.map_id);
              shared_clipboard->overworld_map_metadata.scope =
                  OverworldMapMetadataClipboardScope::kAll;
              shared_clipboard->has_overworld_map_metadata =
                  shared_clipboard->overworld_map_metadata.valid;
            }),
        ICON_MD_CONTENT_COPY);
    constexpr auto kScope = OverworldMapMetadataClipboardScope::kAll;
    auto paste_item = gui::CanvasMenuItem::Conditional(
        "Paste Map Properties",
        [this, target, shared_clipboard]() {
          const auto edits = BuildOverworldMetadataPasteEdits(
              target.map_id, shared_clipboard->overworld_map_metadata, kScope);
          (void)ApplyPropertyEdits(
              edits,
              absl::StrFormat(
                  "Paste map properties from 0x%02X",
                  shared_clipboard->overworld_map_metadata.source_map_id));
        },
        [shared_clipboard]() {
          return shared_clipboard->has_overworld_map_metadata &&
                 CanPasteOverworldMapMetadata(
                     shared_clipboard->overworld_map_metadata, kScope);
        });
    if (shared_clipboard->has_overworld_map_metadata &&
        shared_clipboard->overworld_map_metadata.valid) {
      paste_item.tooltip = DescribeOverworldMapMetadataClipboard(
          shared_clipboard->overworld_map_metadata);
    }
    add(std::move(paste_item), ICON_MD_CONTENT_PASTE, true);
  }

  // --- Insert: entities placed at the clicked tile --------------------------
  gui::CanvasMenuItem insert_menu;
  insert_menu.label = "Insert";
  insert_menu.enabled_condition = [this, valid_map]() {
    return valid_map && bool(entity_insert_callback_);
  };
  struct EntityType {
    const char* icon;
    const char* label;
    const char* type;
  };
  // Transports have no insert path yet (see docs/internal/gui/context-menus.md).
  const EntityType entity_types[] = {
      {ICON_MD_DOOR_FRONT, "Entrance", "entrance"},
      {ICON_MD_CYCLONE, "Hole", "hole"},
      {ICON_MD_DOOR_BACK, "Exit", "exit"},
      {ICON_MD_GRASS, "Item", "item"},
      {ICON_MD_PEST_CONTROL_RODENT, "Sprite", "sprite"}};
  // Not a structured binding: lambdas can't capture those before Clang 16.
  for (const EntityType& entity : entity_types) {
    const char* type = entity.type;
    insert_menu.subitems.emplace_back(
        entity.label, entity.icon, [this, target, type]() {
          if (entity_insert_callback_ && target.valid())
            entity_insert_callback_(type, target);
        });
  }
  add(std::move(insert_menu), ICON_MD_ADD_LOCATION);

  // --- View: mirrors the toolbar toggles and zoom buttons -------------------
  gui::CanvasMenuItem view_menu;
  view_menu.label = "View";
  const auto add_toggle = [&](const char* icon, const char* label,
                              const ContextViewToggle& toggle,
                              const char* shortcut_name) {
    if (!toggle.toggle || !toggle.is_on) {
      return;
    }
    gui::CanvasMenuItem item(label, icon, toggle.toggle);
    item.checked_condition = toggle.is_on;
    item.shortcut = hint(shortcut_name);
    view_menu.subitems.push_back(std::move(item));
  };
  add_toggle(ICON_MD_GRID_4X4, "Grid", grid_toggle_, "overworld.toggle_grid");
  add_toggle(ICON_MD_PLACE, "Entities", entities_toggle_,
             "overworld.toggle_entities");
  add_toggle(ICON_MD_LAYERS, "Overlay Preview", overlay_toggle_, nullptr);
  if (!view_menu.subitems.empty()) {
    view_menu.subitems.back().separator_after = true;
  }
  const auto add_view_action = [&](const char* icon, const char* label,
                                   const std::function<void()>& callback,
                                   const char* shortcut_name) {
    auto item = gui::CanvasMenuItem::Conditional(
        label,
        [callback]() {
          if (callback)
            callback();
        },
        [callback]() { return bool(callback); });
    item.icon = icon;
    item.shortcut = hint(shortcut_name);
    view_menu.subitems.push_back(std::move(item));
  };
  add_view_action(ICON_MD_ZOOM_IN, "Zoom In", zoom_in_callback_,
                  "overworld.zoom_in");
  add_view_action(ICON_MD_ZOOM_OUT, "Zoom Out", zoom_out_callback_,
                  "overworld.zoom_out");
  if (zoom_fit_callback_) {
    add_view_action(ICON_MD_FIT_SCREEN, "Zoom to Fit", zoom_fit_callback_,
                    "overworld.zoom_fit");
  }
  if (center_map_callback_) {
    add_view_action(ICON_MD_CENTER_FOCUS_STRONG, "Center on Map",
                    center_map_callback_, "overworld.center_map");
  }
  add(std::move(view_menu), ICON_MD_VISIBILITY);
}

absl::Status MapPropertiesSystem::ApplyPropertyEdit(
    const OverworldPropertyEdit& edit) {
  if (property_edit_callback_) {
    return property_edit_callback_(edit);
  }
  return ApplyPropertyEditDirect(edit);
}

absl::Status MapPropertiesSystem::ApplyPropertyEdits(
    const std::vector<OverworldPropertyEdit>& edits,
    const std::string& description) {
  if (property_edit_batch_callback_) {
    return property_edit_batch_callback_(edits, description);
  }
  for (const auto& edit : edits) {
    if (!CheckPropertyEditSupported(edit).ok()) {
      continue;
    }
    RETURN_IF_ERROR(ApplyPropertyEdit(edit));
  }
  return absl::OkStatus();
}

absl::Status MapPropertiesSystem::CheckPropertyEditSupported(
    const OverworldPropertyEdit& edit) const {
  return CheckFieldSupported(rom_, edit);
}

absl::StatusOr<int> MapPropertiesSystem::ReadPropertyValue(
    const OverworldPropertyEdit& edit) const {
  if (!overworld_) {
    return absl::FailedPreconditionError("Overworld is not available");
  }
  const int map_id = EffectivePropertyMapId(
      const_cast<zelda3::Overworld*>(overworld_), edit.map_id);
  if (!IsValidMapId(map_id)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid overworld map: %d", edit.map_id));
  }
  const auto* map = overworld_->overworld_map(map_id);
  if (!map) {
    return absl::NotFoundError(
        absl::StrFormat("Overworld map 0x%02X is unavailable", map_id));
  }

  switch (edit.field) {
    case OverworldPropertyField::kAreaSize:
      return static_cast<int>(map->area_size());
    case OverworldPropertyField::kAreaGraphics:
      return map->area_graphics();
    case OverworldPropertyField::kAreaPalette:
      return map->area_palette();
    case OverworldPropertyField::kMainPalette:
      return map->main_palette();
    case OverworldPropertyField::kSpriteGraphics:
      return map->sprite_graphics(std::clamp(edit.index, 0, 2));
    case OverworldPropertyField::kSpritePalette:
      return map->sprite_palette(std::clamp(edit.index, 0, 2));
    case OverworldPropertyField::kAnimatedGraphics:
      return map->animated_gfx();
    case OverworldPropertyField::kCustomTileset:
      return map->custom_tileset(std::clamp(edit.index, 0, 7));
    case OverworldPropertyField::kMessageId:
      return map->message_id();
    case OverworldPropertyField::kMusic:
      return map->area_music(std::clamp(edit.index, 0, 3));
    case OverworldPropertyField::kMosaic:
      return *const_cast<zelda3::OverworldMap*>(map)->mutable_mosaic() ? 1 : 0;
    case OverworldPropertyField::kMosaicExpanded:
      return map->mosaic_expanded()[std::clamp(edit.index, 0, 3)] ? 1 : 0;
    case OverworldPropertyField::kAreaSpecificBgColor:
      return map->area_specific_bg_color();
    case OverworldPropertyField::kSubscreenOverlay:
      return map->subscreen_overlay();
  }
  return absl::InvalidArgumentError("Unknown overworld property field");
}

absl::Status MapPropertiesSystem::ApplyPropertyEditDirect(
    const OverworldPropertyEdit& edit) {
  if (!overworld_ || !rom_) {
    return absl::FailedPreconditionError(
        "Overworld property editing requires loaded overworld data and ROM");
  }
  const int map_id = EffectivePropertyMapId(overworld_, edit.map_id);
  if (!IsValidMapId(map_id)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid overworld map: %d", edit.map_id));
  }
  auto* map = overworld_->mutable_overworld_map(map_id);
  if (!map) {
    return absl::NotFoundError(
        absl::StrFormat("Overworld map 0x%02X is unavailable", map_id));
  }

  const absl::Status supported = CheckFieldSupported(rom_, edit);
  if (!supported.ok()) {
    return supported;
  }

  const int game_state = std::clamp(edit.index, 0, 2);
  switch (edit.field) {
    case OverworldPropertyField::kAreaSize: {
      const auto size = static_cast<zelda3::AreaSizeEnum>(edit.value);
      auto status = overworld_->ConfigureMultiAreaMap(map_id, size);
      if (!status.ok()) {
        return status;
      }
      RefreshSiblingMapGraphics(map_id, true);
      RefreshOverworldMap();
      break;
    }
    case OverworldPropertyField::kAreaGraphics:
      map->set_area_graphics(static_cast<uint8_t>(edit.value));
      RefreshMapProperties();
      if (maps_bmp_) {
        (*maps_bmp_)[map_id].set_modified(true);
      }
      PrepareMapForGraphicsRefresh(map_id);
      map->LoadAreaGraphics();
      RefreshSiblingMapGraphics(map_id);
      RefreshTile16Blockset();
      RefreshOverworldMap();
      break;
    case OverworldPropertyField::kAreaPalette:
      map->set_area_palette(static_cast<uint8_t>(edit.value));
      RefreshMapProperties();
      RefreshMapPalette();
      RefreshOverworldMap();
      break;
    case OverworldPropertyField::kMainPalette:
      map->set_main_palette(static_cast<uint8_t>(edit.value));
      RefreshMapProperties();
      RefreshMapPalette();
      RefreshOverworldMap();
      break;
    case OverworldPropertyField::kSpriteGraphics:
      map->set_sprite_graphics(game_state, static_cast<uint8_t>(edit.value));
      ForceRefreshGraphics(map_id);
      RefreshMapProperties();
      RefreshOverworldMap();
      break;
    case OverworldPropertyField::kSpritePalette:
      map->set_sprite_palette(game_state, static_cast<uint8_t>(edit.value));
      RefreshMapProperties();
      RefreshOverworldMap();
      break;
    case OverworldPropertyField::kAnimatedGraphics:
      map->set_animated_gfx(static_cast<uint8_t>(edit.value));
      ForceRefreshGraphics(map_id);
      RefreshMapProperties();
      RefreshTile16Blockset();
      RefreshOverworldMap();
      break;
    case OverworldPropertyField::kCustomTileset: {
      const int slot = std::clamp(edit.index, 0, 7);
      map->set_custom_tileset(slot, static_cast<uint8_t>(edit.value));
      PrepareMapForGraphicsRefresh(map_id);
      map->LoadAreaGraphics();
      ForceRefreshGraphics(map_id);
      RefreshSiblingMapGraphics(map_id);
      RefreshMapProperties();
      RefreshTile16Blockset();
      RefreshOverworldMap();
      break;
    }
    case OverworldPropertyField::kMessageId:
      map->set_message_id(static_cast<uint16_t>(edit.value));
      RefreshMapProperties();
      RefreshOverworldMap();
      break;
    case OverworldPropertyField::kMusic: {
      const int music_state = std::clamp(edit.index, 0, 3);
      *map->mutable_area_music(music_state) = static_cast<uint8_t>(edit.value);
      WriteRomByteIfValid(rom_, MusicRomAddressForMapState(map_id, music_state),
                          static_cast<uint8_t>(edit.value));
      RefreshMapProperties();
      break;
    }
    case OverworldPropertyField::kMosaic:
      *map->mutable_mosaic() = edit.value != 0;
      RefreshMapProperties();
      RefreshOverworldMap();
      break;
    case OverworldPropertyField::kMosaicExpanded:
      map->set_mosaic_expanded(std::clamp(edit.index, 0, 3), edit.value != 0);
      RefreshMapProperties();
      RefreshOverworldMap();
      break;
    case OverworldPropertyField::kAreaSpecificBgColor:
      map->set_area_specific_bg_color(static_cast<uint16_t>(edit.value));
      WriteRomWordIfValid(
          rom_, zelda3::OverworldCustomAreaSpecificBGPalette + (map_id * 2),
          static_cast<uint16_t>(edit.value));
      RefreshMapProperties();
      RefreshOverworldMap();
      break;
    case OverworldPropertyField::kSubscreenOverlay:
      map->set_subscreen_overlay(static_cast<uint16_t>(edit.value));
      WriteRomWordIfValid(
          rom_, zelda3::OverworldCustomSubscreenOverlayArray + (map_id * 2),
          static_cast<uint16_t>(edit.value));
      RefreshMapProperties();
      RefreshOverworldMap();
      break;
  }

  rom_->set_dirty(true);
  return absl::OkStatus();
}

void MapPropertiesSystem::RefreshMapProperties() {
  if (refresh_map_properties_) {
    refresh_map_properties_();
  }
}

void MapPropertiesSystem::RefreshOverworldMap() {
  if (refresh_overworld_map_) {
    refresh_overworld_map_();
  }
}

absl::Status MapPropertiesSystem::RefreshMapPalette() {
  if (refresh_map_palette_) {
    return refresh_map_palette_();
  }
  return absl::OkStatus();
}

absl::Status MapPropertiesSystem::RefreshTile16Blockset() {
  if (refresh_tile16_blockset_) {
    return refresh_tile16_blockset_();
  }
  return absl::OkStatus();
}

void MapPropertiesSystem::ForceRefreshGraphics(int map_index) {
  if (force_refresh_graphics_) {
    force_refresh_graphics_(map_index);
  }
}

void MapPropertiesSystem::RefreshSiblingMapGraphics(int map_index,
                                                    bool include_self) {
  if (!overworld_ || !maps_bmp_ || map_index < 0 ||
      map_index >= zelda3::kNumOverworldMaps) {
    return;
  }

  auto* map = overworld_->mutable_overworld_map(map_index);
  if (map->area_size() == zelda3::AreaSizeEnum::SmallArea) {
    return;  // No siblings for small areas
  }

  int parent_id = map->parent();
  std::vector<int> siblings;

  switch (map->area_size()) {
    case zelda3::AreaSizeEnum::LargeArea:
      siblings = {parent_id, parent_id + 1, parent_id + 8, parent_id + 9};
      break;
    case zelda3::AreaSizeEnum::WideArea:
      siblings = {parent_id, parent_id + 1};
      break;
    case zelda3::AreaSizeEnum::TallArea:
      siblings = {parent_id, parent_id + 8};
      break;
    default:
      return;
  }

  for (int sibling : siblings) {
    if (sibling >= 0 && sibling < zelda3::kNumOverworldMaps) {
      // Skip self unless include_self is true
      if (sibling == map_index && !include_self) {
        continue;
      }

      // Mark as modified FIRST
      (*maps_bmp_)[sibling].set_modified(true);

      // Load graphics from ROM
      PrepareMapForGraphicsRefresh(sibling);
      overworld_->mutable_overworld_map(sibling)->LoadAreaGraphics();

      // CRITICAL FIX: Force immediate refresh on the sibling
      // This will trigger the callback to OverworldEditor's
      // RefreshChildMapOnDemand
      ForceRefreshGraphics(sibling);
    }
  }

  // After marking all siblings, trigger a refresh
  // This ensures all marked maps get processed
  RefreshOverworldMap();
}

std::string MapPropertiesSystem::GetOverlayDescription(uint16_t overlay_id) {
  if (overlay_id == 0x0093) {
    return "Triforce Room Curtain";
  } else if (overlay_id == 0x0094) {
    return "Under the Bridge";
  } else if (overlay_id == 0x0095) {
    return "Sky Background (LW Death Mountain)";
  } else if (overlay_id == 0x0096) {
    return "Pyramid Background";
  } else if (overlay_id == 0x0097) {
    return "First Fog Overlay (Master Sword Area)";
  } else if (overlay_id == 0x009C) {
    return "Lava Background (DW Death Mountain)";
  } else if (overlay_id == 0x009D) {
    return "Second Fog Overlay (Lost Woods/Skull Woods)";
  } else if (overlay_id == 0x009E) {
    return "Tree Canopy (Forest)";
  } else if (overlay_id == 0x009F) {
    return "Rain Effect (Misery Mire)";
  } else if (overlay_id == 0x00FF) {
    return "No Overlay";
  } else {
    return "Custom overlay";
  }
}

void MapPropertiesSystem::DrawOverlayPreviewOnMap(int current_map,
                                                  int current_world,
                                                  bool show_overlay_preview) {
  gfx::ScopedTimer timer("map_properties_draw_overlay_preview");

  if (!show_overlay_preview || !maps_bmp_ || !canvas_)
    return;

  // Get subscreen overlay information based on ROM version and map type
  uint16_t overlay_id = 0x00FF;
  bool has_subscreen_overlay = false;

  bool is_special_overworld_map = (current_map >= 0x80 && current_map < 0xA0);

  if (is_special_overworld_map) {
    // Special overworld maps (0x80-0x9F) do not support subscreen overlays
    return;
  }

  // Light World (0x00-0x3F) and Dark World (0x40-0x7F) maps support subscreen
  // overlays for all versions
  overlay_id = overworld_->overworld_map(current_map)->subscreen_overlay();
  has_subscreen_overlay = (overlay_id != 0x00FF);

  if (!has_subscreen_overlay)
    return;

  // Map subscreen overlay ID to special area map for bitmap
  int overlay_map_index = -1;
  if (overlay_id >= 0x80 && overlay_id < 0xA0) {
    overlay_map_index = overlay_id;
  }

  if (overlay_map_index < 0 || overlay_map_index >= zelda3::kNumOverworldMaps)
    return;

  // Get the subscreen overlay map's bitmap
  const auto& overlay_bitmap = (*maps_bmp_)[overlay_map_index];
  if (!overlay_bitmap.is_active() || !overlay_bitmap.texture())
    return;

  // Calculate position for subscreen overlay preview on the current map
  int current_map_x = current_map % 8;
  int current_map_y = current_map / 8;
  if (current_world == 1) {
    current_map_x = (current_map - 0x40) % 8;
    current_map_y = (current_map - 0x40) / 8;
  } else if (current_world == 2) {
    current_map_x = (current_map - 0x80) % 8;
    current_map_y = (current_map - 0x80) / 8;
  }

  int scale = static_cast<int>(canvas_->global_scale());
  int map_x = current_map_x * kOverworldMapSize * scale;
  int map_y = current_map_y * kOverworldMapSize * scale;

  // Determine if this is a background or foreground subscreen overlay
  bool is_background_overlay =
      (overlay_id == 0x0095 || overlay_id == 0x0096 || overlay_id == 0x009C);

  // Set alpha for semi-transparent preview
  ImU32 overlay_color =
      is_background_overlay ? IM_COL32(255, 255, 255, 128)
                            :  // Background subscreen overlays - lighter
          IM_COL32(255, 255, 255,
                   180);  // Foreground subscreen overlays - more opaque

  // Draw the subscreen overlay bitmap with semi-transparency
  canvas_->draw_list()->AddImage(
      (ImTextureID)(intptr_t)overlay_bitmap.texture(), ImVec2(map_x, map_y),
      ImVec2(map_x + kOverworldMapSize * scale,
             map_y + kOverworldMapSize * scale),
      ImVec2(0, 0), ImVec2(1, 1), overlay_color);
}

}  // namespace editor
}  // namespace yaze
