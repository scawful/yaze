#include "zelda3/overworld/overworld.h"

#include <algorithm>
#include <utility>

namespace yaze::zelda3 {

std::vector<rom::NativeOverworldPropertyEdit>
Overworld::CaptureNativePropertyRecords() const {
  std::vector<rom::NativeOverworldPropertyEdit> records;
  auto add = [&](uint16_t table, int map, uint16_t value, bool word = false) {
    std::vector<uint8_t> bytes = {static_cast<uint8_t>(value)};
    if (word)
      bytes.push_back(static_cast<uint8_t>(value >> 8));
    records.push_back({table, static_cast<uint16_t>(map), std::move(bytes)});
  };
  for (const auto& map : overworld_maps_) {
    const int index = map.index();
    add(1, index, map.area_specific_bg_color(), true);
    add(2, index, map.main_palette());
    uint8_t mosaic = 0;
    for (int quadrant = 0; quadrant < 4; ++quadrant)
      if (map.mosaic_expanded()[quadrant])
        mosaic |= 0x08 >> quadrant;
    add(3, index, mosaic);
    add(4, index, map.animated_gfx());
    add(5, index, map.subscreen_overlay(), true);
    std::vector<uint8_t> graphics;
    for (int slot = 0; slot < 8; ++slot)
      graphics.push_back(map.custom_tileset(slot));
    records.push_back({6, static_cast<uint16_t>(index), std::move(graphics)});
    add(19, index, map.message_id(), true);
    if (map.parent() != index)
      continue;
    add(index >= 128 ? 27 : 26, index, map.area_graphics());
    add(31, index, map.area_palette());
    for (int phase = 0; phase < 4; ++phase)
      add(32 + phase, index, map.area_music(phase));
    if (index >= 64 && index < 160)
      add(28, index, map.area_music(0));
    if (index >= 128) {
      constexpr std::array<uint16_t, 3> kGraphics = {24, 36, 39};
      constexpr std::array<uint16_t, 3> kPalettes = {25, 37, 38};
      for (int phase = 0; phase < 3; ++phase) {
        add(kGraphics[phase], index, map.sprite_graphics(phase));
        add(kPalettes[phase], index, map.sprite_palette(phase));
      }
    }
  }
  return records;
}

absl::StatusOr<std::vector<rom::NativeOverworldPropertyEdit>>
Overworld::PrepareNativePropertySave() const {
  std::vector<rom::NativeOverworldPropertyEdit> edits;
  const auto& baseline = native_nonterrain_baseline_[0];
  size_t cursor = 0;
  for (auto record : CaptureNativePropertyRecords()) {
    const size_t length = record.value.size();
    if (cursor + 3 + length > baseline.size() ||
        baseline[cursor] != record.table_id ||
        baseline[cursor + 1] != record.map_id ||
        baseline[cursor + 2] != static_cast<int64_t>(length))
      return absl::FailedPreconditionError(
          "Native property model identity changed");
    cursor += 3;
    if (!std::equal(record.value.begin(), record.value.end(),
                    baseline.begin() + cursor)) {
      if (record.table_id == 3) {
        const auto raw = rom::LyraLayoutRecord(descriptor_layout_, rom_->data(),
                                               rom_->size(), 3, record.map_id);
        if (!raw)
          return absl::FailedPreconditionError(
              "Native mosaic source unavailable");
        record.value[0] |= (*raw)[0] & 0xF0;
      }
      edits.push_back(std::move(record));
    }
    cursor += length;
  }
  if (cursor != baseline.size())
    return absl::FailedPreconditionError(
        "Native property baseline is incomplete");
  return edits;
}

Overworld::NativeModelState Overworld::CaptureNativeNonTerrainState() const {
  NativeModelState state;
  auto add = [&](int group, std::initializer_list<int64_t> values) {
    state[group].insert(state[group].end(), values.begin(), values.end());
  };
  for (const auto& edit : CaptureNativePropertyRecords()) {
    add(0,
        {edit.table_id, edit.map_id, static_cast<int64_t>(edit.value.size())});
    state[0].insert(state[0].end(), edit.value.begin(), edit.value.end());
  }
  add(6, {materialized_map_count()});
  auto inherited = [&](int value, int parent_value) {
    const size_t offset = state[6].size();
    const auto& loaded = native_nonterrain_baseline_[6];
    // Refresh may copy an edited parent's effective selector into children.
    // Accept that model propagation without making the child's dormant raw
    // slot writable. Unchanged child values also retain their load baseline.
    if (value == parent_value && offset < loaded.size())
      add(6, {loaded[offset]});
    else
      add(6, {value});
  };
  for (const auto& m : overworld_maps_) {
    add(6, {m.index(), m.parent(), static_cast<int>(m.area_size()),
            m.large_index()});
    // Parent-owned fields on a child cannot be serialized as its own raw row.
    // Retain the native child's original slots until an area-level edit owns
    // both the parent mutation and the corresponding model propagation.
    if (m.parent() != m.index()) {
      const auto* parent = overworld_map(m.parent());
      if (!parent) {
        add(6, {-1});
        continue;
      }
      inherited(m.area_graphics(), parent->area_graphics());
      inherited(m.area_palette(), parent->area_palette());
      for (int phase = 0; phase < 4; ++phase)
        inherited(m.area_music(phase), parent->area_music(phase));
    }
    if (m.parent() != m.index() || m.index() < 128) {
      for (int phase = 0; phase < 3; ++phase) {
        const auto* parent = overworld_map(m.parent());
        if (m.parent() != m.index() && parent) {
          inherited(m.sprite_graphics(phase), parent->sprite_graphics(phase));
          inherited(m.sprite_palette(phase), parent->sprite_palette(phase));
        } else {
          add(6, {m.sprite_graphics(phase), m.sprite_palette(phase)});
        }
      }
    }
  }
  for (const auto* list : {&all_entrances_, &all_holes_}) {
    add(1, {static_cast<int64_t>(list->size())});
    for (const auto& e : *list)
      add(1, {e.map_id_, e.map_pos_, e.entrance_id_, e.is_hole_, e.deleted,
              e.x_, e.y_, e.game_x_, e.game_y_});
  }
  add(2, {static_cast<int64_t>(all_exits_.size())});
  for (const auto& e : all_exits_)
    add(2, {e.map_id_,
            e.room_id_,
            e.map_pos_,
            e.y_scroll_,
            e.x_scroll_,
            e.y_player_,
            e.x_player_,
            e.y_camera_,
            e.x_camera_,
            e.scroll_mod_y_,
            e.scroll_mod_x_,
            e.door_type_1_,
            e.door_type_2_,
            e.is_hole_,
            e.deleted_,
            e.is_automatic_,
            e.x_,
            e.y_,
            e.game_x_,
            e.game_y_});
  add(3, {static_cast<int64_t>(all_items_.size())});
  for (const auto& e : all_items_)
    add(3, {e.room_map_id_, e.id_, e.game_x_, e.game_y_, e.x_, e.y_, e.bg2_,
            e.deleted});
  for (const auto& list : all_sprites_) {
    add(4, {static_cast<int64_t>(list.size())});
    for (const auto& e : list)
      add(4, {e.map_id(), e.id(), e.x(), e.y(), e.deleted(),
              e.overworld_x_flags(), e.overworld_y_flags()});
  }
  state[5].insert(state[5].end(), all_tiles_types_.begin(),
                  all_tiles_types_.end());
  const auto& diggable = diggable_tiles_.GetRawData();
  state[5].insert(state[5].end(), diggable.begin(), diggable.end());
  return state;
}

}  // namespace yaze::zelda3
