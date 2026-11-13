#include "app/editor/overworld/lyra_terrain_canvas.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "app/gui/automation/widget_id_registry.h"
#include "imgui/imgui.h"
#include "zelda3/resource_labels.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {
namespace {

using Document = core::LyraTerrainDocument;

uint32_t AllocateStableId(const std::vector<uint32_t>& existing_ids) {
  std::unordered_set<uint32_t> id_set(existing_ids.begin(), existing_ids.end());
  uint32_t candidate = 1;
  while (id_set.count(candidate) > 0) {
    ++candidate;
  }
  return candidate;
}

uint32_t AllocateSpriteId(const core::LyraAuthoringState& state) {
  std::vector<uint32_t> ids;
  for (const auto& map : state.maps) {
    for (const auto& s : map.sprites) {
      ids.push_back(s.id);
    }
  }
  return AllocateStableId(ids);
}

uint32_t AllocateDoorId(const core::LyraAuthoringState& state) {
  std::vector<uint32_t> ids;
  for (const auto& map : state.maps) {
    for (const auto& d : map.doors) {
      ids.push_back(d.id);
    }
  }
  return AllocateStableId(ids);
}

uint32_t AllocateExitId(const core::LyraAuthoringState& state) {
  std::vector<uint32_t> ids;
  for (const auto& map : state.maps) {
    for (const auto& e : map.exits) {
      ids.push_back(e.id);
    }
  }
  return AllocateStableId(ids);
}

uint32_t AllocateFlightPointId(const core::LyraAuthoringState& state) {
  std::vector<uint32_t> ids;
  for (const auto& f : state.flight_points) {
    ids.push_back(f.id);
  }
  return AllocateStableId(ids);
}

uint32_t AllocateEdgeConnectionId(const core::LyraAuthoringState& state) {
  std::vector<uint32_t> ids;
  for (const auto& c : state.edge_connections) {
    ids.push_back(c.id);
  }
  return AllocateStableId(ids);
}

// Edge of `map_id` that faces an adjacent `other`.
const char* FacingEdge(int map_id, int other) {
  if (other == map_id - Document::kColumns)
    return "north";
  if (other == map_id + Document::kColumns)
    return "south";
  return other < map_id ? "west" : "east";
}

// Lyra maps have no vanilla names; only a project label names them.
std::string MapName(int map_id) {
  const auto& labels = zelda3::GetResourceLabels();
  if (map_id >= Document::kFirstMap &&
      !labels.HasProjectLabel(zelda3::ResourceType::kOverworldMap, map_id)) {
    return {};
  }
  return labels.GetLabel(zelda3::ResourceType::kOverworldMap, map_id);
}

std::string NamedMap(int map_id) {
  const std::string name = MapName(map_id);
  return name.empty() ? absl::StrFormat("$%02X", map_id)
                      : absl::StrFormat("$%02X %s", map_id, name);
}

void DrawEdgeTilePicker(const char* label, int& value, int first, int last) {
  const std::string preview = std::to_string(value);
  if (!ImGui::BeginCombo(label, preview.c_str()))
    return;
  for (int tile = first; tile <= last; ++tile) {
    const bool selected = tile == value;
    const std::string option = std::to_string(tile);
    if (ImGui::Selectable(option.c_str(), selected))
      value = tile;
    if (selected)
      ImGui::SetItemDefaultFocus();
  }
  ImGui::EndCombo();
}

std::string SpriteTypeLabel(int type) {
  // ResolveSpriteName returns thread-local storage reused by the next call.
  const std::string name =
      zelda3::ResolveSpriteName(static_cast<uint16_t>(type));
  return absl::StrFormat("$%02X %s", type, name);
}

// Returns true only when the chosen type differs from the incoming one.
bool DrawSpriteTypePicker(const char* label, int& type,
                          std::array<char, 64>& filter) {
  const std::string preview = SpriteTypeLabel(type);
  if (!ImGui::BeginCombo(label, preview.c_str(), ImGuiComboFlags_HeightLarge)) {
    return false;
  }
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputTextWithHint("##filter", "Search ID or name", filter.data(),
                           filter.size());
  const std::string query = absl::AsciiStrToLower(filter.data());
  bool changed = false;
  for (int id = 0; id < 256; ++id) {
    const std::string option = SpriteTypeLabel(id);
    if (!query.empty() &&
        absl::AsciiStrToLower(option).find(query) == std::string::npos) {
      continue;
    }
    ImGui::PushID(id);
    const bool selected = id == type;
    if (ImGui::Selectable(option.c_str(), selected) && !selected) {
      type = id;
      changed = true;
    }
    if (selected)
      ImGui::SetItemDefaultFocus();
    ImGui::PopID();
  }
  ImGui::EndCombo();
  return changed;
}

}  // namespace

void LyraTerrainCanvas::DrawGameplayArea(int map_id,
                                         zelda3::Overworld& overworld,
                                         UndoManager& history) {
  if (!ImGui::CollapsingHeader("Gameplay area",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  // Keep a value copy: a selection below can replace the authoring vector.
  const auto* stored = document_->GameplayAreaForMap(map_id);
  const auto area = stored ? std::optional(*stored) : std::nullopt;
  if (area) {
    ImGui::Text("Parent: $%02X", area->parent_map_id);
    ImGui::TextWrapped("Ordered pages: %s", FormatMapIds(area->maps).c_str());
    if (ImGui::Button("Unassign whole area")) {
      status_ = SetGameplayArea(map_id, std::nullopt, overworld, history);
    }
  } else {
    ImGui::TextUnformatted("Parent: Unassigned");
  }
  const bool child = area && area->parent_map_id != map_id;
  if (child) {
    ImGui::TextWrapped("Select the parent page to change this area's size.");
  }
  ImGui::BeginDisabled(child);
  if (ImGui::BeginCombo("Assign this page as parent", "Choose footprint")) {
    using Size = core::LyraGameplayAreaSize;
    const std::array<std::pair<Size, const char*>, 4> options = {{
        {Size::kSmall, "Small (1 x 1 page)"},
        {Size::kWide, "Wide (2 x 1 pages)"},
        {Size::kTall, "Tall (1 x 2 pages)"},
        {Size::kLarge, "Large (2 x 2 pages)"},
    }};
    for (const auto& [size, label] : options) {
      const bool valid = core::LyraGameplayAreaFootprint(map_id, size).ok();
      ImGui::BeginDisabled(!valid);
      const bool selected = area && area->size == size;
      if (ImGui::Selectable(label, selected) && !selected) {
        status_ = SetGameplayArea(map_id, size, overworld, history);
      }
      ImGui::EndDisabled();
    }
    ImGui::EndCombo();
  }
  gui::WidgetIdRegistry::Instance().RegisterWidget(
      "LyraMapProperties/combo:GameplayFootprint", "combo",
      ImGui::GetID("Assign this page as parent"));
  ImGui::EndDisabled();
}

void LyraTerrainCanvas::DrawProperties(int map_id, int& state,
                                       zelda3::Overworld& overworld,
                                       UndoManager& history) {
  if (!document_) {
    return;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    placement_kind_ = 0;
  const bool is_lyra = (map_id >= Document::kFirstMap &&
                        map_id < Document::kFirstMap + Document::kMapCount);

  if (is_lyra) {
    const int map_index = map_id - Document::kFirstMap;
    if (property_map_ != map_id) {
      property_map_ = map_id;
      copy_message_.clear();
      copy_failed_ = false;
    }
    core::LyraMapProperties& draft = PropertyDraftFor(map_index);

    ImGui::PushID("LyraProperties");
    ImGui::Text("Lyra Map Properties ($%02X)", map_id);
    ImGui::TextWrapped(
        "Project draft; saved with project; build/runtime unavailable");

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
    if (ImGui::CollapsingHeader(ICON_MD_INFO " Tail Map Limitations ($A0-$BF)",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::PopStyleColor();
      ImGui::TextWrapped(
          "• Items & Bomb Doors: Disabled (pending SRAM event allocation in "
          "save file)\n"
          "• Exits: 1 free slot in vanilla table; multi-link requires ASM exit "
          "table v2\n"
          "• Entrances: Light-world trigger links v2 in development (Phase "
          "P2)\n"
          "• Sprites: Draft state only (requires actor compilation for ROM)");
    } else {
      ImGui::PopStyleColor();
    }
    DrawGameplayArea(map_id, overworld, history);

    // Game state selector combo 0..2
    const char* state_names[] = {"State 0", "State 1", "State 2"};
    int current_state = state;
    if (current_state < 0)
      current_state = 0;
    if (current_state > 2)
      current_state = 2;
    if (ImGui::Combo("Game State", &current_state, state_names, 3)) {
      state = current_state;
    }

    // Main palette (uint8_t)
    int main_pal = draft.main_palette;
    if (ImGui::InputInt("Main palette", &main_pal)) {
      draft.main_palette = static_cast<uint8_t>(std::clamp(main_pal, 0, 255));
    }

    gui::WidgetIdRegistry::Instance().RegisterWidget(
        "LyraMapProperties/input:MainPalette", "input",
        ImGui::GetID("Main palette"));
    // Area palette (uint8_t)
    int area_pal = draft.palette_set;
    if (ImGui::InputInt("Area palette", &area_pal)) {
      draft.palette_set = static_cast<uint8_t>(std::clamp(area_pal, 0, 255));
    }

    if (draft.background_palette) {
      ImGui::TextUnformatted(
          "Background colors: project override (128 colors)");
      ImGui::TextWrapped(
          "This map uses project colors. Native palettes and sprite colors "
          "stay unchanged.");
      if (ImGui::Button("Use native palette")) {
        draft.background_palette.reset();
        status_ = ApplyPropertyDraft(map_index, overworld, history);
      }
    } else {
      ImGui::TextUnformatted("Background colors: native palette");
    }

    // Animated GFX (uint8_t hex)
    int anim_sheet = draft.animated_sheet;
    if (ImGui::InputScalar("Animated GFX", ImGuiDataType_S32, &anim_sheet,
                           nullptr, nullptr, "%02X",
                           ImGuiInputTextFlags_CharsHexadecimal)) {
      draft.animated_sheet =
          static_cast<uint8_t>(std::clamp(anim_sheet, 0, 255));
    }

    // Custom GFX slots 0..7
    if (ImGui::TreeNode("Custom GFX Slots")) {
      for (int i = 0; i < 8; ++i) {
        ImGui::PushID(i);
        int sheet_val = draft.graphics_sheets[i];
        std::string label = absl::StrFormat("Custom GFX slot %d", i);
        if (ImGui::InputScalar(label.c_str(), ImGuiDataType_S32, &sheet_val,
                               nullptr, nullptr, "%02X",
                               ImGuiInputTextFlags_CharsHexadecimal)) {
          draft.graphics_sheets[i] =
              static_cast<uint8_t>(std::clamp(sheet_val, 0, 255));
        }
        ImGui::PopID();
      }
      ImGui::TreePop();
    }

    // Sprite set / palette for states 0..2 (-1 inherit)
    if (ImGui::TreeNode("Sprite Graphics & Palettes")) {
      for (int s = 0; s < 3; ++s) {
        ImGui::PushID(s);
        std::string gfx_label =
            absl::StrFormat("Sprite set (State %d, -1 inherit)", s);
        int gfx_val = draft.sprite_graphics[s];
        if (ImGui::InputInt(gfx_label.c_str(), &gfx_val)) {
          draft.sprite_graphics[s] = std::clamp(gfx_val, -1, 255);
        }

        std::string pal_label =
            absl::StrFormat("Sprite palette (State %d, -1 inherit)", s);
        int pal_val = draft.sprite_palettes[s];
        if (ImGui::InputInt(pal_label.c_str(), &pal_val)) {
          draft.sprite_palettes[s] = std::clamp(pal_val, -1, 255);
        }
        ImGui::PopID();
      }
      ImGui::TreePop();
    }

    const bool music_open = ImGui::TreeNode("Music and message");
    gui::WidgetIdRegistry::Instance().RegisterWidget(
        absl::StrFormat("LyraMapProperties/%02X/tree:MusicMessage", map_id),
        "tree", ImGui::GetItemID());
    if (music_open) {
      for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i);
        int value = draft.music[i];
        if (ImGui::InputInt("Music", &value))
          draft.music[i] = std::clamp(value, 0, 255);
        ImGui::PopID();
      }
      int value = draft.message_id;
      if (ImGui::InputScalar("Message ID", ImGuiDataType_S32, &value, nullptr,
                             nullptr, "%04X",
                             ImGuiInputTextFlags_CharsHexadecimal))
        draft.message_id = std::clamp(value, 0, 65535);
      gui::WidgetIdRegistry::Instance().RegisterWidget(
          absl::StrFormat("LyraMapProperties/%02X/input_scalar:MessageId",
                          map_id),
          "input_scalar", ImGui::GetItemID());
      ImGui::TreePop();
    }

    int copy_source = copy_source_map_;
    if (ImGui::InputScalar("Native source map", ImGuiDataType_S32, &copy_source,
                           nullptr, nullptr, "%02X",
                           ImGuiInputTextFlags_CharsHexadecimal)) {
      copy_source_map_ = std::clamp(copy_source, 0, 0x9F);
    }
    if (const auto* source = overworld.overworld_map(copy_source_map_)) {
      ImGui::Text("Source $%02X, area parent $%02X", copy_source_map_,
                  source->parent());
    }
    if (ImGui::Button("Copy graphics settings")) {
      auto copied =
          CopyNativeGraphicsSettings(overworld, copy_source_map_, draft);
      copy_failed_ = !copied.ok();
      if (copied.ok()) {
        draft = *std::move(copied);
        copy_message_ = absl::StrFormat(
            "Copied $%02X (area parent $%02X) into the $%02X draft",
            copy_source_map_,
            overworld.overworld_map(copy_source_map_)->parent(), map_id);
      } else {
        copy_message_ = copied.status().ToString();
      }
    }
    ImGui::TextWrapped(
        "Copies native sheets, palettes, animation and sprite sets. Background "
        "colors, overlays and runtime graphics are not copied. Apply "
        "Properties to keep these settings.");
    if (copy_failed_) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                         copy_message_.c_str());
    } else if (!copy_message_.empty()) {
      ImGui::TextWrapped("%s", copy_message_.c_str());
    }

    // Apply and Revert buttons
    if (ImGui::Button("Apply Properties")) {
      status_ = ApplyPropertyDraft(map_index, overworld, history);
      if (status_.ok()) {
        copy_message_.clear();
        copy_failed_ = false;
      }
    }
    gui::WidgetIdRegistry::Instance().RegisterWidget(
        "LyraMapProperties/button:Apply", "button", ImGui::GetItemID());
    ImGui::SameLine();
    if (ImGui::Button("Revert Properties")) {
      RevertPropertyDraft(map_index);
      status_ = absl::OkStatus();
      copy_message_.clear();
      copy_failed_ = false;
    }

    ImGui::PushTextWrapPos(0.0f);
    if (PropertyConflict(map_index)) {
      ImGui::TextColored(
          ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
          "$%02X properties changed after these edits began (for example by "
          "Undo). Apply is refused; Revert Properties to load the current "
          "values, then edit again.",
          map_id);
    } else if (PropertyPending(map_index)) {
      ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f),
                         "$%02X has unapplied property edits.", map_id);
    }
    std::vector<int> other_pending = PendingPropertyMaps();
    std::erase(other_pending, map_id);
    if (!other_pending.empty()) {
      ImGui::TextWrapped(
          "Unapplied properties on %s. Visit each map and Apply or Revert "
          "before saving the project.",
          FormatMapIds(other_pending).c_str());
    }

    if (!status_.ok() && !PropertyConflict(map_index)) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                         status_.ToString().c_str());
    }
    ImGui::PopTextWrapPos();

    ImGui::Separator();

    // Entity section for Lyra maps
    ImGui::TextUnformatted("Entities");

    if (placement_kind_ != 0) {
      ImGui::TextColored(
          ImVec4(1.0f, 0.9f, 0.2f, 1.0f),
          "Placement mode active (Click canvas to place, Esc to cancel)");
    }

    // Sprites
    if (ImGui::CollapsingHeader("Sprites", ImGuiTreeNodeFlags_DefaultOpen)) {
      DrawSpriteTypePicker("Sprite type", new_sprite_type_, sprite_filter_);
      if (ImGui::Button("Add sprite")) {
        core::LyraAuthoringState next_state = document_->authoring();
        const uint32_t new_id = AllocateSpriteId(next_state);
        next_state.maps[map_index].sprites.push_back({
            .id = new_id,
            .sprite_id = static_cast<uint8_t>(new_sprite_type_),
            .x = 16,
            .y = 16,
            .state = std::clamp(state, 0, 2),
        });
        (void)EditAuthoring(next_state, overworld, history);
      }
      gui::WidgetIdRegistry::Instance().RegisterWidget(
          "LyraMapProperties/button:AddSprite", "button", ImGui::GetItemID());
      ImGui::SameLine();
      if (ImGui::Button("Place sprite on canvas")) {
        placement_kind_ = 1;
        placement_type_ = new_sprite_type_;
        placement_state_ = std::clamp(state, 0, 2);
      }

      const auto& sprites = document_->authoring().maps[map_index].sprites;
      for (size_t i = 0; i < sprites.size(); ++i) {
        const auto& s = sprites[i];
        ImGui::PushID(static_cast<int>(s.id + 0x100000));
        std::string header_label =
            absl::StrFormat("Sprite #%u (ID: %u, State: %d) - %s", i, s.id,
                            s.state, SpriteTypeLabel(s.sprite_id));
        if (ImGui::CollapsingHeader((header_label + "###entity").c_str())) {
          core::LyraAuthoringState next_state = document_->authoring();
          auto& target = next_state.maps[map_index].sprites[i];
          bool changed = false;

          int spr_x = target.x;
          if (ImGui::SliderInt("X", &spr_x, 0, 31)) {
            target.x = spr_x;
            changed = true;
          }
          int spr_y = target.y;
          if (ImGui::SliderInt("Y", &spr_y, 0, 31)) {
            target.y = spr_y;
            changed = true;
          }
          int spr_type = target.sprite_id;
          if (ImGui::InputScalar("Sprite ID", ImGuiDataType_S32, &spr_type,
                                 nullptr, nullptr, "%02X",
                                 ImGuiInputTextFlags_CharsHexadecimal)) {
            target.sprite_id =
                static_cast<uint8_t>(std::clamp(spr_type, 0, 255));
            changed = true;
          }
          int picked_type = target.sprite_id;
          if (DrawSpriteTypePicker("Sprite type", picked_type,
                                   sprite_filter_)) {
            target.sprite_id = static_cast<uint8_t>(picked_type);
            changed = true;
          }
          int spr_st = target.state;
          if (ImGui::SliderInt("State", &spr_st, 0, 2)) {
            target.state = spr_st;
            changed = true;
          }

          if (ImGui::Button("Remove")) {
            next_state.maps[map_index].sprites.erase(
                next_state.maps[map_index].sprites.begin() + i);
            (void)EditAuthoring(next_state, overworld, history);
            ImGui::PopID();
            break;
          }

          if (changed) {
            (void)EditAuthoring(next_state, overworld, history);
            ImGui::PopID();
            break;
          }
        }
        ImGui::PopID();
      }
    }

    // Doors / Entrances
    if (ImGui::CollapsingHeader("Doors / Entrances",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::Button("Add entrance")) {
        core::LyraAuthoringState next_state = document_->authoring();
        const uint32_t new_id = AllocateDoorId(next_state);
        next_state.maps[map_index].doors.push_back({
            .id = new_id,
            .x = 16,
            .y = 16,
            .entrance_id = std::nullopt,
        });
        (void)EditAuthoring(next_state, overworld, history);
      }
      gui::WidgetIdRegistry::Instance().RegisterWidget(
          "LyraMapProperties/button:AddEntrance", "button", ImGui::GetItemID());
      ImGui::SameLine();
      if (ImGui::Button("Place entrance on canvas")) {
        placement_kind_ = 2;
        placement_type_ = 0;
      }

      const auto& doors = document_->authoring().maps[map_index].doors;
      for (size_t i = 0; i < doors.size(); ++i) {
        const auto& d = doors[i];
        ImGui::PushID(static_cast<int>(d.id + 0x200000));
        std::string header_label =
            absl::StrFormat("Entrance #%u (ID: %u)", i, d.id);
        if (ImGui::CollapsingHeader((header_label + "###entity").c_str())) {
          core::LyraAuthoringState next_state = document_->authoring();
          auto& target = next_state.maps[map_index].doors[i];
          bool changed = false;

          int door_x = target.x;
          if (ImGui::SliderInt("X", &door_x, 0, 31)) {
            target.x = door_x;
            changed = true;
          }
          int door_y = target.y;
          if (ImGui::SliderInt("Y", &door_y, 0, 31)) {
            target.y = door_y;
            changed = true;
          }
          int ent_id = target.entrance_id.value_or(-1);
          if (ImGui::InputInt("Entrance ID (-1 unassigned)", &ent_id)) {
            if (ent_id < 0) {
              target.entrance_id = std::nullopt;
            } else {
              target.entrance_id = std::min(ent_id, 0xFE);
            }
            changed = true;
          }

          if (ImGui::Button("Remove")) {
            next_state.maps[map_index].doors.erase(
                next_state.maps[map_index].doors.begin() + i);
            (void)EditAuthoring(next_state, overworld, history);
            ImGui::PopID();
            break;
          }

          if (changed) {
            (void)EditAuthoring(next_state, overworld, history);
            ImGui::PopID();
            break;
          }
        }
        ImGui::PopID();
      }
    }

    // Exits
    if (ImGui::CollapsingHeader("Exits", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::Button("Add exit")) {
        core::LyraAuthoringState next_state = document_->authoring();
        const uint32_t new_id = AllocateExitId(next_state);
        next_state.maps[map_index].exits.push_back({
            .id = new_id,
            .room_id = 0,
            .x = 16,
            .y = 16,
            .exit_row = std::nullopt,
        });
        (void)EditAuthoring(next_state, overworld, history);
      }
      gui::WidgetIdRegistry::Instance().RegisterWidget(
          "LyraMapProperties/button:AddExit", "button", ImGui::GetItemID());
      ImGui::SameLine();
      if (ImGui::Button("Place exit on canvas")) {
        placement_kind_ = 3;
        placement_type_ = 0;
      }

      const auto& exits = document_->authoring().maps[map_index].exits;
      for (size_t i = 0; i < exits.size(); ++i) {
        const auto& e = exits[i];
        ImGui::PushID(static_cast<int>(e.id + 0x300000));
        std::string header_label =
            absl::StrFormat("Exit #%u (ID: %u)", i, e.id);
        if (ImGui::CollapsingHeader((header_label + "###entity").c_str())) {
          core::LyraAuthoringState next_state = document_->authoring();
          auto& target = next_state.maps[map_index].exits[i];
          bool changed = false;

          int exit_x = target.x;
          if (ImGui::SliderInt("X", &exit_x, 0, 31)) {
            target.x = exit_x;
            changed = true;
          }
          int exit_y = target.y;
          if (ImGui::SliderInt("Y", &exit_y, 0, 31)) {
            target.y = exit_y;
            changed = true;
          }
          int room_id = target.room_id;
          if (ImGui::InputScalar("Room ID", ImGuiDataType_S32, &room_id,
                                 nullptr, nullptr, "%03X",
                                 ImGuiInputTextFlags_CharsHexadecimal)) {
            target.room_id = std::clamp(room_id, 0, 0x127);
            changed = true;
          }
          int row_val = target.exit_row.value_or(-1);
          if (ImGui::InputInt("Exit row (-1 unassigned)", &row_val)) {
            if (row_val < 0) {
              target.exit_row = std::nullopt;
            } else {
              target.exit_row = std::min(row_val, 78);
            }
            changed = true;
          }

          if (ImGui::Button("Remove")) {
            next_state.maps[map_index].exits.erase(
                next_state.maps[map_index].exits.begin() + i);
            (void)EditAuthoring(next_state, overworld, history);
            ImGui::PopID();
            break;
          }

          if (changed) {
            (void)EditAuthoring(next_state, overworld, history);
            ImGui::PopID();
            break;
          }
        }
        ImGui::PopID();
      }
    }

    ImGui::PopID();
  }

  // Flight points section for ANY map 0..BF when document is loaded
  if (ImGui::CollapsingHeader("Flight Points",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PushID("FlightPointsSection");

    if (ImGui::Button("Add flight point")) {
      core::LyraAuthoringState next_state = document_->authoring();
      const uint32_t new_id = AllocateFlightPointId(next_state);
      next_state.flight_points.push_back({
          .id = new_id,
          .map_id = std::clamp(map_id, 0, 0xBF),
          .x = 16,
          .y = 16,
          .name = "Flight point",
          .runtime_slot = std::nullopt,
      });
      (void)EditAuthoring(next_state, overworld, history);
    }
    gui::WidgetIdRegistry::Instance().RegisterWidget(
        "LyraMapProperties/button:AddFlight", "button", ImGui::GetItemID());
    ImGui::SameLine();
    if (ImGui::Button("Place flight point on canvas")) {
      placement_kind_ = 4;
      placement_type_ = 0;
    }

    const auto& flight_points = document_->authoring().flight_points;
    for (size_t i = 0; i < flight_points.size(); ++i) {
      const auto& f = flight_points[i];
      if (f.map_id != map_id) {
        continue;
      }
      ImGui::PushID(static_cast<int>(f.id + 0x400000));
      std::string header_label =
          absl::StrFormat("Flight Point #%u (%s)", i, f.name.c_str());
      if (ImGui::CollapsingHeader((header_label + "###entity").c_str())) {
        core::LyraAuthoringState next_state = document_->authoring();
        auto& target = next_state.flight_points[i];
        bool changed = false;

        ImGui::Text("ID: %u (read-only)", target.id);

        char name_buf[81] = {0};
        std::snprintf(name_buf, sizeof(name_buf), "%s", target.name.c_str());
        if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
          target.name = name_buf;
          changed = true;
        }

        int f_map = target.map_id;
        if (ImGui::InputScalar("Map ID", ImGuiDataType_S32, &f_map, nullptr,
                               nullptr, "%02X",
                               ImGuiInputTextFlags_CharsHexadecimal)) {
          target.map_id = std::clamp(f_map, 0, 0xBF);
          changed = true;
        }

        int f_x = target.x;
        if (ImGui::SliderInt("X", &f_x, 0, 31)) {
          target.x = f_x;
          changed = true;
        }

        int f_y = target.y;
        if (ImGui::SliderInt("Y", &f_y, 0, 31)) {
          target.y = f_y;
          changed = true;
        }

        if (ImGui::Button("Remove")) {
          next_state.flight_points.erase(next_state.flight_points.begin() + i);
          (void)EditAuthoring(next_state, overworld, history);
          ImGui::PopID();
          break;
        }

        if (changed) {
          (void)EditAuthoring(next_state, overworld, history);
          ImGui::PopID();
          break;
        }
      }
      ImGui::PopID();
    }

    ImGui::PopID();
  }

  DrawEdgeConnections(map_id, overworld, history);
  if (!status_.ok())
    ImGui::TextWrapped("%s", status_.ToString().c_str());
}

void LyraTerrainCanvas::DrawEdgeConnections(int map_id,
                                            zelda3::Overworld& overworld,
                                            UndoManager& history) {
  const auto& connections = document_->authoring().edge_connections;
  const auto connected = [&](int other) {
    return std::any_of(connections.begin(), connections.end(),
                       [&](const core::LyraEdgeConnection& c) {
                         return c.map_a == std::min(map_id, other) &&
                                c.map_b == std::max(map_id, other);
                       });
  };
  bool has_neighbor = false;
  std::vector<int> open_neighbors;
  for (const int delta : {-Document::kColumns, Document::kColumns, -1, 1}) {
    const int other = map_id + delta;
    if (!core::LyraEdgeMapsAdjacent(map_id, other))
      continue;
    has_neighbor = true;
    if (!connected(other))
      open_neighbors.push_back(other);
  }
  if (!has_neighbor ||
      !ImGui::CollapsingHeader("Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  ImGui::PushID("EdgeConnectionsSection");
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f),
                     "Saved with project; does not open the route in the game");
  ImGui::TextDisabled("Tile16 positions 0–31 along the edge");

  const bool full = connections.size() >= Document::kMaxEdgeConnections;
  const char* preview = full                     ? "Connection limit reached"
                        : open_neighbors.empty() ? "All neighbors drafted"
                                                 : "Choose neighbor";
  ImGui::TextUnformatted("Add connection");
  ImGui::BeginDisabled(full || open_neighbors.empty());
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::BeginCombo("##AddConnection", preview)) {
    for (const int other : open_neighbors) {
      ImGui::PushID(other);
      const std::string option = absl::StrFormat(
          "%s edge: %s", FacingEdge(map_id, other), NamedMap(other));
      if (ImGui::Selectable(option.c_str())) {
        core::LyraAuthoringState next_state = document_->authoring();
        next_state.edge_connections.push_back({
            .id = AllocateEdgeConnectionId(next_state),
            .map_a = std::min(map_id, other),
            .map_b = std::max(map_id, other),
            .passage = std::nullopt,
        });
        (void)EditAuthoring(next_state, overworld, history);
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  ImGui::EndDisabled();
  gui::WidgetIdRegistry::Instance().RegisterWidget(
      "LyraMapProperties/combo:AddConnection", "combo",
      ImGui::GetID("##AddConnection"));

  bool any = false;
  for (size_t i = 0; i < connections.size(); ++i) {
    const auto& connection = connections[i];
    if (connection.map_a != map_id && connection.map_b != map_id)
      continue;
    any = true;
    ImGui::PushID(static_cast<int>(connection.id));
    ImGui::Separator();
    ImGui::Text("$%02X %s ↔ $%02X %s", connection.map_a,
                FacingEdge(connection.map_a, connection.map_b),
                connection.map_b,
                FacingEdge(connection.map_b, connection.map_a));
    for (const int id : {connection.map_a, connection.map_b}) {
      if (const std::string name = MapName(id); !name.empty())
        ImGui::TextDisabled("$%02X: %s", id, name.c_str());
    }

    core::LyraEdgeConnection edited = connection;
    bool assigned = edited.passage.has_value();
    if (ImGui::Checkbox("Assign passage", &assigned)) {
      edited.passage = assigned ? std::optional<core::LyraEdgePassage>(
                                      core::LyraEdgePassage{0, 0})
                                : std::nullopt;
    }
    if (!edited.passage) {
      ImGui::TextUnformatted("Passage: unassigned (no opening drafted)");
    } else {
      auto& passage = *edited.passage;
      ImGui::Text("Passage: tiles %d–%d", passage.first_tile,
                  passage.last_tile);
      ImGui::TextDisabled(
          "%s; first and last included",
          connection.map_b - connection.map_a == Document::kColumns
              ? "Columns (left to right)"
              : "Rows (top to bottom)");
      ImGui::PushItemWidth(ImGui::GetFontSize() * 7.0f);
      DrawEdgeTilePicker("First tile", passage.first_tile, 0,
                         Document::kMapSize - 1);
      passage.last_tile = std::max(passage.first_tile, passage.last_tile);
      DrawEdgeTilePicker("Last tile", passage.last_tile, 0,
                         Document::kMapSize - 1);
      passage.first_tile = std::min(passage.first_tile, passage.last_tile);
      ImGui::PopItemWidth();
    }
    if (edited != connection) {
      core::LyraAuthoringState next_state = document_->authoring();
      next_state.edge_connections[i] = std::move(edited);
      (void)EditAuthoring(next_state, overworld, history);
      ImGui::PopID();
      break;
    }

    if (ImGui::Button("Remove connection")) {
      core::LyraAuthoringState next_state = document_->authoring();
      next_state.edge_connections.erase(next_state.edge_connections.begin() +
                                        i);
      (void)EditAuthoring(next_state, overworld, history);
      ImGui::PopID();
      break;
    }
    ImGui::PopID();
  }
  if (!any)
    ImGui::TextDisabled("No connections drafted for $%02X.", map_id);

  ImGui::PopTextWrapPos();
  ImGui::PopID();
}

void LyraTerrainCanvas::DrawEntities(const gui::CanvasRuntime& canvas,
                                     int world, int state) {
  if (!document_ || !canvas.draw_list || !std::isfinite(canvas.scale) ||
      canvas.scale <= 0) {
    return;
  }

  ImDrawList* draw = canvas.draw_list;
  const float scale = canvas.scale;
  const ImVec2 origin = canvas.canvas_p0;
  const ImVec2 scroll = canvas.scrolling;

  auto ToScreen = [&](int map_id, int x, int y) -> ImVec2 {
    const int local_map = map_id % 64;
    const int world_x = (local_map % 8) * 512 + x * 16;
    const int world_y = (local_map / 8) * 512 + y * 16;
    return ImVec2(origin.x + scroll.x + world_x * scale,
                  origin.y + scroll.y + world_y * scale);
  };

  const float marker_size = 16.0f * scale;

  // Draw project map entities (sprites, doors, exits)
  for (int map_idx = 0; map_idx < Document::kMapCount; ++map_idx) {
    const int map_id = Document::kFirstMap + map_idx;
    if (map_id / 0x40 != world) {
      continue;
    }
    const auto& map_auth = document_->authoring().maps[map_idx];

    // Exits (Distinct color: yellow / orange-yellow)
    for (const auto& exit : map_auth.exits) {
      const ImVec2 p_min = ToScreen(map_id, exit.x, exit.y);
      const ImVec2 p_max(p_min.x + marker_size, p_min.y + marker_size);
      draw->AddRectFilled(p_min, p_max, IM_COL32(230, 190, 40, 180));
      draw->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 220));
      draw->AddText(ImVec2(p_min.x + 2, p_min.y + 2), IM_COL32(0, 0, 0, 255),
                    "exit");
    }

    // Doors / Entrances (Distinct color: green)
    for (const auto& door : map_auth.doors) {
      const ImVec2 p_min = ToScreen(map_id, door.x, door.y);
      const ImVec2 p_max(p_min.x + marker_size, p_min.y + marker_size);
      draw->AddRectFilled(p_min, p_max, IM_COL32(50, 190, 80, 180));
      draw->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 220));
      draw->AddText(ImVec2(p_min.x + 2, p_min.y + 2), IM_COL32(0, 0, 0, 255),
                    "entrance");
    }

    // Sprites matching current state (Distinct color: red / magenta)
    for (const auto& sprite : map_auth.sprites) {
      if (sprite.state != state) {
        continue;
      }
      DrawSpritePreview(canvas, map_id, sprite);
      const ImVec2 p_min = ToScreen(map_id, sprite.x, sprite.y);
      const ImVec2 p_max(p_min.x + marker_size, p_min.y + marker_size);

      draw->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 220));
      draw->AddText(ImVec2(p_min.x + 2, p_min.y + 2),
                    IM_COL32(255, 255, 255, 255), "S");
    }
  }

  // Flight points matching explicit map_id / 0x40 == world (Distinct color: cyan / blue)
  for (const auto& flight : document_->authoring().flight_points) {
    if (flight.map_id / 0x40 != world) {
      continue;
    }
    const ImVec2 p_min = ToScreen(flight.map_id, flight.x, flight.y);
    const ImVec2 p_max(p_min.x + marker_size, p_min.y + marker_size);
    draw->AddRectFilled(p_min, p_max, IM_COL32(40, 160, 220, 180));
    draw->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 220));
    draw->AddText(ImVec2(p_min.x + 2, p_min.y + 2), IM_COL32(0, 0, 0, 255),
                  "flight");
  }
}

void LyraTerrainCanvas::HandleEntityInput(const gui::CanvasRuntime& canvas,
                                          int world,
                                          zelda3::Overworld& overworld,
                                          UndoManager& history) {
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    placement_kind_ = 0;
  }

  if (placement_kind_ == 0 || !canvas.hovered || !document_) {
    return;
  }

  if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    return;
  }

  if (!std::isfinite(canvas.scale) || canvas.scale <= 0) {
    return;
  }

  const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
  const float world_x_px =
      (mouse_pos.x - canvas.canvas_p0.x - canvas.scrolling.x) / canvas.scale;
  const float world_y_px =
      (mouse_pos.y - canvas.canvas_p0.y - canvas.scrolling.y) / canvas.scale;

  if (!std::isfinite(world_x_px) || !std::isfinite(world_y_px)) {
    return;
  }

  if (world_x_px < 0 || world_x_px >= 4096 || world_y_px < 0 ||
      world_y_px >= 4096)
    return;
  const int tile_x = static_cast<int>(std::floor(world_x_px / 16.0f));
  const int tile_y = static_cast<int>(std::floor(world_y_px / 16.0f));

  if (tile_x < 0 || tile_x > 255 || tile_y < 0 || tile_y > 255) {
    return;
  }

  const int map_col = tile_x / 32;
  const int map_row = tile_y / 32;
  if (map_col < 0 || map_col >= 8 || map_row < 0 || map_row >= 8) {
    return;
  }

  const int local_map = map_row * 8 + map_col;
  const int explicit_map_id = world * 0x40 + local_map;
  const int local_x = tile_x % 32;
  const int local_y = tile_y % 32;

  core::LyraAuthoringState next_state = document_->authoring();

  if (placement_kind_ == 4) {
    // Flight point (allowed on any map 0..BF)
    if (explicit_map_id >= 0 && explicit_map_id <= 0xBF) {
      const uint32_t new_id = AllocateFlightPointId(next_state);
      next_state.flight_points.push_back({
          .id = new_id,
          .map_id = explicit_map_id,
          .x = local_x,
          .y = local_y,
          .name = "Flight point",
          .runtime_slot = std::nullopt,
      });
      absl::Status edit_status = EditAuthoring(next_state, overworld, history);
      if (edit_status.ok()) {
        placement_kind_ = 0;
      }
    }
    return;
  }

  // Sprites, doors, exits: only maps A0..BF
  if (explicit_map_id < Document::kFirstMap ||
      explicit_map_id >= Document::kFirstMap + Document::kMapCount) {
    return;
  }

  const int map_index = explicit_map_id - Document::kFirstMap;

  if (placement_kind_ == 1) {
    // Sprite
    const uint32_t new_id = AllocateSpriteId(next_state);
    next_state.maps[map_index].sprites.push_back({
        .id = new_id,
        .sprite_id = static_cast<uint8_t>(placement_type_),
        .x = local_x,
        .y = local_y,
        .state = std::clamp(placement_state_, 0, 2),
    });
    absl::Status edit_status = EditAuthoring(next_state, overworld, history);
    if (edit_status.ok()) {
      placement_kind_ = 0;
    }
  } else if (placement_kind_ == 2) {
    // Door / Entrance
    const uint32_t new_id = AllocateDoorId(next_state);
    next_state.maps[map_index].doors.push_back({
        .id = new_id,
        .x = local_x,
        .y = local_y,
        .entrance_id = std::nullopt,
    });
    absl::Status edit_status = EditAuthoring(next_state, overworld, history);
    if (edit_status.ok()) {
      placement_kind_ = 0;
    }
  } else if (placement_kind_ == 3) {
    // Exit
    const uint32_t new_id = AllocateExitId(next_state);
    next_state.maps[map_index].exits.push_back({
        .id = new_id,
        .room_id = 0,
        .x = local_x,
        .y = local_y,
        .exit_row = std::nullopt,
    });
    absl::Status edit_status = EditAuthoring(next_state, overworld, history);
    if (edit_status.ok()) {
      placement_kind_ = 0;
    }
  }
}

}  // namespace yaze::editor
