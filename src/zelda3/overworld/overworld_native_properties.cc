#include "zelda3/overworld/overworld.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <unordered_set>
#include <utility>

#include "absl/strings/str_format.h"
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
        descriptor_layout_.revision >= 4) {
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
    std::vector<int64_t> slice;
    auto push_slice = [&](std::initializer_list<int64_t> values) {
      slice.insert(slice.end(), values.begin(), values.end());
    };
    if (profiles.size() != overworld_maps_.size() ||
        native_effective_area_profiles_.size() != overworld_maps_.size() ||
        m.parent() < 0 || m.parent() >= materialized_map_count()) {
      push_slice({-2});
    } else {
      const auto& old = native_effective_area_profiles_[i];
      const auto& parent = profiles[m.parent()];
      auto inherited = [&](int value, int loaded, int parent_value) {
        // Existing children may be refreshed lazily after a parent property
        // edit. Neither their unchanged effective value nor the propagated
        // parent value changes the child's cached raw slot. Other child
        // mutations stay refused.
        push_slice(
            {child && value != loaded && value != parent_value ? value : -1});
      };
      if (m.uses_native_screen_appearance()) {
        push_slice({-1, -1});
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
        // Original LW/DW sprite selector editing remains a separate
        // capability.
        if (i < 128)
          push_slice({profiles[i].sprite_graphics[phase],
                      profiles[i].sprite_palettes[phase]});
      }
    }
    add(6, {i, static_cast<int64_t>(slice.size())});
    state[6].insert(state[6].end(), slice.begin(), slice.end());
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

namespace {

std::optional<int> FindFirstChangedItemMapId(
    const std::vector<int64_t>& current, const std::vector<int64_t>& baseline) {
  constexpr size_t kItemFields = 8;
  const size_t cur_count =
      current.empty() ? 0 : (current.size() - 1) / kItemFields;
  const size_t base_count =
      baseline.empty() ? 0 : (baseline.size() - 1) / kItemFields;
  const size_t shared = std::min(cur_count, base_count);
  for (size_t idx = 0; idx < shared; ++idx) {
    const size_t offset = 1 + idx * kItemFields;
    if (!std::equal(current.begin() + offset,
                    current.begin() + offset + kItemFields,
                    baseline.begin() + offset)) {
      const int64_t map_id =
          cur_count < base_count ? baseline[offset] : current[offset];
      if (map_id >= 0) {
        return static_cast<int>(map_id);
      }
      if (current[offset] >= 0) {
        return static_cast<int>(current[offset]);
      }
      return std::nullopt;
    }
  }
  if (cur_count > shared) {
    const int64_t map_id = current[1 + shared * kItemFields];
    if (map_id >= 0) {
      return static_cast<int>(map_id);
    }
  } else if (base_count > shared) {
    const int64_t map_id = baseline[1 + shared * kItemFields];
    if (map_id >= 0) {
      return static_cast<int>(map_id);
    }
  }
  return std::nullopt;
}

std::optional<int> FindFirstChangedChildPropertyMapId(
    const std::vector<int64_t>& current, const std::vector<int64_t>& baseline) {
  size_t cur_pos = 1;
  size_t base_pos = 1;
  while (cur_pos + 1 < current.size() && base_pos + 1 < baseline.size()) {
    const int64_t cur_map = current[cur_pos];
    const int64_t cur_len = current[cur_pos + 1];
    const int64_t base_map = baseline[base_pos];
    const int64_t base_len = baseline[base_pos + 1];
    if (cur_len < 0 || base_len < 0 ||
        cur_pos + 2 + static_cast<size_t>(cur_len) > current.size() ||
        base_pos + 2 + static_cast<size_t>(base_len) > baseline.size()) {
      const int64_t map_id = cur_map >= 0 ? cur_map : base_map;
      if (map_id >= 0) {
        return static_cast<int>(map_id);
      }
      return std::nullopt;
    }
    const size_t len = static_cast<size_t>(cur_len);
    if (cur_map != base_map || cur_len != base_len ||
        !std::equal(current.begin() + cur_pos + 2,
                    current.begin() + cur_pos + 2 + len,
                    baseline.begin() + base_pos + 2)) {
      const int64_t map_id = cur_map >= 0 ? cur_map : base_map;
      if (map_id >= 0) {
        return static_cast<int>(map_id);
      }
      return std::nullopt;
    }
    cur_pos += 2 + len;
    base_pos += 2 + static_cast<size_t>(base_len);
  }
  if (cur_pos + 1 < current.size() && current[cur_pos] >= 0) {
    return static_cast<int>(current[cur_pos]);
  }
  if (base_pos + 1 < baseline.size() && baseline[base_pos] >= 0) {
    return static_cast<int>(baseline[base_pos]);
  }
  return std::nullopt;
}

}  // namespace

std::string Overworld::DescribeNativeNonTerrainRefusal(
    size_t group, const std::vector<int64_t>& current,
    const std::vector<int64_t>& baseline) const {
  if (group == 3) {
    if (const auto map_id = FindFirstChangedItemMapId(current, baseline)) {
      return absl::StrFormat(
          "Native overworld save cannot publish pending items on map $%02X",
          *map_id);
    }
    return "Native overworld save cannot publish pending items";
  }

  if (group == 5) {
    std::unordered_set<uint16_t> changed_tile8;
    const size_t min_len = std::min(current.size(), baseline.size());
    const size_t type_len = std::min<size_t>(kNumTileTypes, min_len);
    for (size_t idx = 0; idx < type_len; ++idx) {
      if (current[idx] != baseline[idx]) {
        changed_tile8.insert(static_cast<uint16_t>(idx));
      }
    }
    std::unordered_set<uint16_t> changed_diggable_tile16;
    const size_t max_len = std::max(current.size(), baseline.size());
    for (size_t idx = kNumTileTypes; idx < max_len; ++idx) {
      const uint8_t cur_byte =
          idx < current.size() ? static_cast<uint8_t>(current[idx]) : 0;
      const uint8_t base_byte =
          idx < baseline.size() ? static_cast<uint8_t>(baseline[idx]) : 0;
      const uint8_t diff = static_cast<uint8_t>(cur_byte ^ base_byte);
      if (diff == 0) {
        continue;
      }
      const size_t bit_base = (idx - kNumTileTypes) * 8;
      for (int bit = 0; bit < 8; ++bit) {
        if ((diff & (1u << bit)) != 0) {
          changed_diggable_tile16.insert(static_cast<uint16_t>(bit_base + bit));
        }
      }
    }

    auto world_blockset = [&](int world) -> const OverworldBlockset* {
      switch (world) {
        case 0:
          return &map_tiles_.light_world;
        case 1:
          return &map_tiles_.dark_world;
        case 2:
          return &map_tiles_.special_world;
        default:
          return nullptr;
      }
    };
    std::optional<int> first_map;
    const int map_count = materialized_map_count();
    for (int map_id = 0; map_id < map_count && !first_map; ++map_id) {
      const OverworldBlockset* grid = world_blockset(map_id / 0x40);
      if (!grid) {
        continue;
      }
      const int left = (map_id % 8) * 32;
      const int top = ((map_id % 0x40) / 8) * 32;
      if (static_cast<size_t>(left + 32) > grid->size()) {
        continue;
      }
      const bool has_page_override = std::any_of(
          page_tile16_.begin(), page_tile16_.end(),
          [map_id](const auto& row) { return row.map_id == map_id; });
      std::vector<gfx::Tile16> page_defs;
      const std::vector<gfx::Tile16>* defs = &tiles16_;
      if (has_page_override) {
        page_defs = Tiles16ForMap(map_id);
        defs = &page_defs;
      }
      for (int x = 0; x < 32 && !first_map; ++x) {
        if (static_cast<size_t>(top + 32) > (*grid)[left + x].size()) {
          continue;
        }
        for (int y = 0; y < 32; ++y) {
          const uint16_t tile16_id = (*grid)[left + x][top + y];
          if (changed_diggable_tile16.contains(tile16_id)) {
            first_map = map_id;
            break;
          }
          if (tile16_id < defs->size()) {
            const auto& t = (*defs)[tile16_id];
            if (changed_tile8.contains(t.tile0_.id_) ||
                changed_tile8.contains(t.tile1_.id_) ||
                changed_tile8.contains(t.tile2_.id_) ||
                changed_tile8.contains(t.tile3_.id_)) {
              first_map = map_id;
              break;
            }
          }
        }
      }
    }
    if (first_map) {
      return absl::StrFormat(
          "Native overworld save cannot publish pending tile types on map "
          "$%02X",
          *first_map);
    }
    if (current_map_ >= 0 && current_map_ < materialized_map_count()) {
      return absl::StrFormat(
          "Native overworld save cannot publish pending tile types (map $%02X "
          "active)",
          current_map_);
    }
    return "Native overworld save cannot publish pending tile types";
  }

  if (group == 6) {
    if (const auto map_id =
            FindFirstChangedChildPropertyMapId(current, baseline)) {
      return absl::StrFormat(
          "Native overworld save cannot publish pending child-map properties "
          "on map $%02X",
          *map_id);
    }
    return "Native overworld save cannot publish pending child-map properties";
  }

  return "Native overworld save cannot publish pending geometry, inherited "
         "properties or entities";
}

bool Overworld::HasPendingNativeSpriteChanges() const {
  if (!loaded_map_tiles_valid_ ||
      !rom::IsNativeSpecialOverworldLayout(descriptor_layout_))
    return false;
  // Disabled sprite saving must not make the session forget edits or newly
  // reserved identities after another domain has cleared the ROM dirty flag.
  return CaptureNativeSpriteState() != native_nonterrain_baseline_[4] ||
         HasPendingNativeAuthoringMetadataChanges(1);
}

}  // namespace yaze::zelda3
