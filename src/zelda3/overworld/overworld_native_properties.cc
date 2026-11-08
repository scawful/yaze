#include "zelda3/overworld/overworld.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "rom/lyra_layout_descriptor.h"
#include "util/macro.h"

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
  const auto profiles = CaptureNativeAreaProfiles();
  if (profiles.size() != overworld_maps_.size())
    return {};
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
    const auto& profile = profiles[index];
    add(index >= 128 ? 27 : 26, index, profile.graphics);
    add(31, index, profile.palette);
    for (int phase = 0; phase < 4; ++phase)
      add(32 + phase, index, profile.music[phase]);
    if (index >= 64 && index < 160)
      add(28, index, profile.music[0]);
    if (index >= 128) {
      constexpr std::array<uint16_t, 3> kGraphics = {24, 36, 39};
      constexpr std::array<uint16_t, 3> kPalettes = {25, 37, 38};
      for (int phase = 0; phase < 3; ++phase) {
        add(kGraphics[phase], index, profile.sprite_graphics[phase]);
        add(kPalettes[phase], index, profile.sprite_palettes[phase]);
      }
    }
    if (rom::IsNativeSpecialOverworldLayout(descriptor_layout_) &&
        descriptor_layout_.revision == 4) {
      add(410, index, map.native_appearance_flags());
      if (index >= 0x80) {
        std::vector<uint8_t> pal(256);
        for (size_t i = 0; i < 128; ++i) {
          const uint16_t w = map.native_appearance_bg_palette()[i];
          pal[i * 2] = static_cast<uint8_t>(w);
          pal[i * 2 + 1] = static_cast<uint8_t>(w >> 8);
        }
        records.push_back({411, static_cast<uint16_t>(index), std::move(pal)});
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
  ASSIGN_OR_RETURN(auto geometry, PrepareNativeGeometrySave());
  edits.insert(edits.end(), std::make_move_iterator(geometry.begin()),
               std::make_move_iterator(geometry.end()));
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
  const auto profiles = CaptureNativeAreaProfiles();
  for (const auto& m : overworld_maps_) {
    const int i = m.index();
    const bool child = m.parent() != i;
    if (profiles.size() != overworld_maps_.size() ||
        native_effective_area_profiles_.size() != overworld_maps_.size() ||
        m.parent() < 0 || m.parent() >= materialized_map_count()) {
      add(6, {-2});
      continue;
    }
    const auto& old = native_effective_area_profiles_[i];
    const auto& parent = profiles[m.parent()];
    auto inherited = [&](int value, int loaded, int parent_value) {
      // Existing children may be refreshed lazily after a parent property edit.
      // Neither their unchanged effective value nor the propagated parent value
      // changes the child's cached raw slot. Other child mutations stay refused.
      add(6, {child && value != loaded && value != parent_value ? value : -1});
    };
    if (m.uses_native_screen_appearance()) {
      add(6, {-1});
      add(6, {-1});
    } else {
      inherited(m.area_graphics(), old.graphics, parent.graphics);
      inherited(m.area_palette(), old.palette, parent.palette);
    }
    for (int phase = 0; phase < 4; ++phase)
      inherited(m.area_music(phase), old.music[phase], parent.music[phase]);
    for (int phase = 0; phase < 3; ++phase) {
      inherited(m.sprite_graphics(phase), old.sprite_graphics[phase],
                parent.sprite_graphics[phase]);
      inherited(m.sprite_palette(phase), old.sprite_palettes[phase],
                parent.sprite_palettes[phase]);
      // Original LW/DW sprite selector editing remains a separate capability.
      if (i < 128)
        add(6, {profiles[i].sprite_graphics[phase],
                profiles[i].sprite_palettes[phase]});
    }
  }
  auto endpoints = CaptureNativeEndpointState();
  state[1] = std::move(endpoints[0]);
  state[2] = std::move(endpoints[1]);
  add(3, {static_cast<int64_t>(all_items_.size())});
  for (const auto& e : all_items_)
    add(3, {e.room_map_id_, e.id_, e.game_x_, e.game_y_, e.x_, e.y_, e.bg2_,
            e.deleted});
  state[4] = CaptureNativeSpriteState();
  state[5].insert(state[5].end(), all_tiles_types_.begin(),
                  all_tiles_types_.end());
  const auto& diggable = diggable_tiles_.GetRawData();
  state[5].insert(state[5].end(), diggable.begin(), diggable.end());
  return state;
}

std::vector<int64_t> Overworld::CaptureNativeSpriteState() const {
  std::vector<int64_t> state;
  for (const auto& list : all_sprites_) {
    state.push_back(static_cast<int64_t>(list.size()));
    for (const auto& e : list) {
      state.insert(state.end(), {e.map_id(), e.id(), e.x(), e.y(), e.deleted(),
                                 e.overworld_x_flags(), e.overworld_y_flags(),
                                 e.placement_id().has_value()
                                     ? static_cast<int64_t>(*e.placement_id())
                                     : static_cast<int64_t>(-1)});
    }
  }
  return state;
}

bool Overworld::HasPendingNativeSpriteChanges() const {
  if (!loaded_map_tiles_valid_ ||
      !rom::IsNativeSpecialOverworldLayout(descriptor_layout_))
    return false;
  // Disabled sprite saving must not make the session forget edits or newly
  // reserved identities after another domain has cleared the ROM dirty flag.
  return CaptureNativeSpriteState() != native_nonterrain_baseline_[4] ||
         native_authoring_metadata_draft_ !=
             native_authoring_metadata_baseline_;
}

}  // namespace yaze::zelda3
