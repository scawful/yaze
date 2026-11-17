#include "zelda3/overworld/overworld.h"

#include <algorithm>
#include <set>

#include "absl/strings/str_format.h"
#include "core/features.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/snes.h"
#include "util/macro.h"

namespace yaze::zelda3 {
namespace {
OverworldAreaProfile ProfileFromMap(const OverworldMap& map) {
  OverworldAreaProfile value;
  value.graphics = map.area_graphics();
  value.palette = map.area_palette();
  for (int phase = 0; phase < 4; ++phase)
    value.music[phase] = map.area_music(phase);
  for (int phase = 0; phase < 3; ++phase) {
    value.sprite_graphics[phase] = map.sprite_graphics(phase);
    value.sprite_palettes[phase] = map.sprite_palette(phase);
  }
  return value;
}
void SetProfile(OverworldMap& map, const OverworldAreaProfile& value) {
  map.set_area_graphics(value.graphics);
  map.set_area_palette(value.palette);
  for (int phase = 0; phase < 4; ++phase)
    *map.mutable_area_music(phase) = value.music[phase];
  for (int phase = 0; phase < 3; ++phase) {
    map.set_sprite_graphics(phase, value.sprite_graphics[phase]);
    map.set_sprite_palette(phase, value.sprite_palettes[phase]);
  }
}
}  // namespace

absl::Status Overworld::LoadNativeAreaProfiles() {
  native_area_profiles_.clear();
  native_effective_area_profiles_.clear();
  for (int map = 0; map < materialized_map_count(); ++map) {
    auto read = [&](int id) -> absl::StatusOr<uint8_t> {
      const auto row = rom::LyraLayoutRecord(descriptor_layout_, rom_->data(),
                                             rom_->size(), id, map);
      if (!row)
        return absl::FailedPreconditionError(
            "Native raw profile is unavailable");
      return (*row)[0];
    };
    OverworldAreaProfile value;
    ASSIGN_OR_RETURN(value.graphics, read(map >= 128 ? 27 : 26));
    ASSIGN_OR_RETURN(value.palette, read(31));
    for (int phase = 0; phase < 4; ++phase) {
      ASSIGN_OR_RETURN(value.music[phase], read(32 + phase));
    }
    for (int phase = 0; phase < 3; ++phase) {
      if (map < 128) {
        const int index = map < 64 ? map + phase * 64 : map + 128;
        ASSIGN_OR_RETURN(value.sprite_graphics[phase],
                         rom_->ReadByte(kOverworldSpriteset + index));
        ASSIGN_OR_RETURN(value.sprite_palettes[phase],
                         rom_->ReadByte(kOverworldSpritePaletteIds + index));
      } else {
        constexpr std::array<int, 3> gfx = {24, 36, 39};
        constexpr std::array<int, 3> pal = {25, 37, 38};
        ASSIGN_OR_RETURN(value.sprite_graphics[phase], read(gfx[phase]));
        ASSIGN_OR_RETURN(value.sprite_palettes[phase], read(pal[phase]));
      }
    }
    native_area_profiles_.push_back(value);
    native_effective_area_profiles_.push_back(
        ProfileFromMap(overworld_maps_[map]));
  }
  ASSIGN_OR_RETURN(native_geometry_baseline_,
                   PlanExpandedOverworldGeometry(overworld_maps_));
  return absl::OkStatus();
}

std::vector<OverworldAreaProfile> Overworld::CaptureNativeAreaProfiles() const {
  auto profiles = native_area_profiles_;
  if (profiles.size() != overworld_maps_.size())
    return {};
  // A root's live edits are authoritative. A child's effective parent values
  // must never replace its dormant raw source, including across Save/Undo.
  for (const auto& map : overworld_maps_)
    if (map.parent() == map.index())
      profiles[map.index()] = ProfileFromMap(map);
    else if (map.uses_native_screen_appearance()) {
      profiles[map.index()].graphics = map.area_graphics();
      profiles[map.index()].palette = map.area_palette();
    }
  return profiles;
}

void Overworld::ApplyNativeAreaProfiles() {
  for (auto& map : overworld_maps_) {
    SetProfile(map, native_area_profiles_[map.parent()]);
    if (map.uses_native_screen_appearance()) {
      map.set_area_graphics(native_area_profiles_[map.index()].graphics);
      map.set_area_palette(native_area_profiles_[map.index()].palette);
    }
    native_effective_area_profiles_[map.index()] = ProfileFromMap(map);
    SyncAreaProperties(map.index());
    map.SetNotBuilt();
  }
  ClearGraphicsConfigCache();
}

absl::StatusOr<OverworldAreaSnapshot> Overworld::CaptureNativeAreaSnapshot()
    const {
  if (!is_loaded_ || !rom::IsNativeSpecialOverworldLayout(descriptor_layout_) ||
      native_area_profiles_.size() != overworld_maps_.size())
    return absl::FailedPreconditionError(
        "Load the native overworld before resizing");
  OverworldAreaSnapshot snapshot;
  for (const auto& map : overworld_maps_)
    snapshot.geometry.push_back(
        {map.parent(), map.area_size(), map.large_index()});
  const auto checked = PlanExpandedOverworldGeometry(snapshot.geometry);
  if (!checked.ok())
    return checked.status();
  snapshot.profiles = CaptureNativeAreaProfiles();
  for (const auto& map : overworld_maps_) {
    OverworldPageProperties page;
    page.background = map.area_specific_bg_color();
    page.overlay = map.subscreen_overlay();
    page.message = map.message_id();
    page.main_palette = map.main_palette();
    page.animated = map.animated_gfx();
    page.mosaic = map.mosaic_expanded();
    for (int slot = 0; slot < 8; ++slot)
      page.graphics[slot] = map.custom_tileset(slot);
    page.appearance_flags = map.native_appearance_flags();
    page.appearance_bg_palette = map.native_appearance_bg_palette();
    snapshot.pages.push_back(page);
  }

  for (int phase = 0; phase < 3; ++phase) {
    for (const auto& sprite : all_sprites_[phase])
      snapshot.sprites[phase].push_back(
          {sprite.map_id(), sprite.nx(), sprite.ny(), sprite.placement_id()});
    snapshot.loaded_sprite_maps[phase].assign(
        sprite_maps_loaded_[phase].begin(),
        sprite_maps_loaded_[phase].begin() + materialized_map_count());
  }
  snapshot.items.reserve(all_items_.size());
  for (const auto& item : all_items_) {
    snapshot.items.push_back({item.room_map_id_, item.game_x_, item.game_y_});
  }
  return snapshot;
}

absl::Status Overworld::RestoreNativeAreaSnapshot(
    const OverworldAreaSnapshot& snapshot) {
  if (!is_loaded_ || !rom::IsNativeSpecialOverworldLayout(descriptor_layout_) ||
      snapshot.geometry.size() != overworld_maps_.size() ||
      snapshot.profiles.size() != overworld_maps_.size() ||
      snapshot.pages.size() != overworld_maps_.size())
    return absl::FailedPreconditionError(
        "Native area snapshot does not match the model");
  RETURN_IF_ERROR(rom_->CheckLayoutSaveable());
  const auto checked = PlanExpandedOverworldGeometry(snapshot.geometry);
  if (!checked.ok())
    return checked.status();
  if (snapshot.items.size() != all_items_.size()) {
    return absl::FailedPreconditionError(
        "Item edits prevent restoring this area snapshot");
  }
  for (size_t row = 0; row < all_items_.size(); ++row) {
    if (all_items_[row].deleted) {
      continue;
    }
    const auto& value = snapshot.items[row];
    if (value.owner >= snapshot.geometry.size() || value.x >= 64 ||
        value.y >= 64 || snapshot.geometry[value.owner].parent != value.owner) {
      return absl::InvalidArgumentError(
          "Invalid native item ownership snapshot");
    }
  }
  for (size_t map = 0; map < snapshot.pages.size(); ++map) {
    const auto& page = snapshot.pages[map];
    const bool has_appearance = descriptor_layout_.revision >= 4 && map >= 0x80;
    if ((!has_appearance && page.appearance_flags != 0) ||
        (page.appearance_flags != 0 && page.appearance_flags != 1 &&
         page.appearance_flags != 3) ||
        std::any_of(page.appearance_bg_palette.begin(),
                    page.appearance_bg_palette.end(),
                    [](uint16_t color) { return color > 0x7FFF; }))
      return absl::InvalidArgumentError(
          "Invalid native screen appearance snapshot");
  }
  std::array<std::vector<size_t>, 3> sprite_rows;
  for (int phase = 0; phase < 3; ++phase) {
    if (snapshot.sprites[phase].size() > all_sprites_[phase].size() ||
        snapshot.loaded_sprite_maps[phase].size() != overworld_maps_.size())
      return absl::FailedPreconditionError(
          "Native sprite row identity changed after resizing");
    for (size_t row = 0; row < snapshot.sprites[phase].size(); ++row) {
      const auto& sprite = snapshot.sprites[phase][row];
      size_t target = row;
      if (sprite.placement_id) {
        target = all_sprites_[phase].size();
        for (size_t i = 0; i < all_sprites_[phase].size(); ++i) {
          if (all_sprites_[phase][i].placement_id() != sprite.placement_id)
            continue;
          if (target != all_sprites_[phase].size())
            return absl::FailedPreconditionError(
                "Duplicate sprite identity in area history");
          target = i;
        }
      }
      if (target >= all_sprites_[phase].size() ||
          all_sprites_[phase][target].placement_id() != sprite.placement_id ||
          std::find(sprite_rows[phase].begin(), sprite_rows[phase].end(),
                    target) != sprite_rows[phase].end())
        return absl::FailedPreconditionError(
            "Sprite identity changed after area snapshot");
      sprite_rows[phase].push_back(target);
      if (sprite.owner >= snapshot.geometry.size() || sprite.x >= 64 ||
          sprite.y >= 64 ||
          (!all_sprites_[phase][target].deleted() &&
           (snapshot.geometry[sprite.owner].parent != sprite.owner ||
            !snapshot.loaded_sprite_maps[phase][sprite.owner])))
        return absl::InvalidArgumentError(
            "Invalid native sprite ownership snapshot");
    }
    for (size_t row = 0; row < all_sprites_[phase].size(); ++row) {
      if (std::find(sprite_rows[phase].begin(), sprite_rows[phase].end(),
                    row) == sprite_rows[phase].end() &&
          (!all_sprites_[phase][row].deleted() ||
           !all_sprites_[phase][row].placement_id()))
        return absl::FailedPreconditionError(
            "A new live sprite prevents restoring this area snapshot");
    }
  }
  // No allocation, reader or file is mutated here. All checks precede the
  // synchronous model change, so a refused Undo leaves the current edit intact.
  native_area_profiles_ = snapshot.profiles;
  for (size_t i = 0; i < snapshot.geometry.size(); ++i) {
    const auto& row = snapshot.geometry[i];
    auto& map = overworld_maps_[i];
    map.SetParent(row.parent);
    map.SetAreaSize(row.size, row.quadrant);
    const auto& page = snapshot.pages[i];
    map.set_area_specific_bg_color(page.background);
    map.set_subscreen_overlay(page.overlay);
    map.set_message_id(page.message);
    map.set_main_palette(page.main_palette);
    map.set_animated_gfx(page.animated);
    for (int slot = 0; slot < 8; ++slot)
      map.set_custom_tileset(slot, page.graphics[slot]);
    for (int q = 0; q < 4; ++q)
      map.set_mosaic_expanded(q, page.mosaic[q]);
    map.set_native_appearance_flags(page.appearance_flags);
    map.set_native_appearance_bg_palette(page.appearance_bg_palette);
  }
  for (int phase = 0; phase < 3; ++phase) {
    sprite_maps_loaded_[phase] = {};
    for (size_t map = 0; map < snapshot.geometry.size(); ++map)
      sprite_maps_loaded_[phase][map] = snapshot.loaded_sprite_maps[phase][map];
    for (size_t row = 0; row < snapshot.sprites[phase].size(); ++row) {
      const auto& value = snapshot.sprites[phase][row];
      all_sprites_[phase][sprite_rows[phase][row]].SetOverworldArea(
          value.owner, value.x, value.y);
    }
  }
  for (size_t row = 0; row < all_items_.size(); ++row) {
    if (all_items_[row].deleted) {
      continue;
    }
    const auto& value = snapshot.items[row];
    all_items_[row].room_map_id_ = value.owner;
    all_items_[row].map_id_ = value.owner;
    all_items_[row].game_x_ = value.x;
    all_items_[row].game_y_ = value.y;
  }
  ApplyNativeAreaProfiles();
  rom_->set_dirty(true);
  return absl::OkStatus();
}

absl::Status Overworld::ConfigureNativeMultiAreaMap(int parent,
                                                    AreaSizeEnum size) {
  ASSIGN_OR_RETURN(auto after, CaptureNativeAreaSnapshot());
  if (static_cast<int>(size) < 0 || static_cast<int>(size) > 3)
    return absl::InvalidArgumentError("Unknown overworld area size");
  const int width =
      size == AreaSizeEnum::LargeArea || size == AreaSizeEnum::WideArea ? 2 : 1;
  const int height =
      size == AreaSizeEnum::LargeArea || size == AreaSizeEnum::TallArea ? 2 : 1;
  if (parent % 8 + width > 8 || parent % 64 / 8 + height > 8 ||
      parent + (height - 1) * 8 + width > materialized_map_count())
    return absl::InvalidArgumentError(
        "Area size crosses an overworld boundary");
  const auto before = after.geometry;
  std::set<int> old_parents = {before[parent].parent};
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      old_parents.insert(before[parent + y * 8 + x].parent);
  // Detach complete old groups, including those intersected by the new area.
  // A later Undo restores their exact former grouping, not just one size enum.
  for (int map = 0; map < materialized_map_count(); ++map)
    if (old_parents.contains(before[map].parent))
      after.geometry[map] = {map, AreaSizeEnum::SmallArea, 0};
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      after.geometry[parent + y * 8 + x] = {parent, size, y * 2 + x};
  const auto plan = PlanExpandedOverworldGeometry(after.geometry);
  if (!plan.ok())
    return plan.status();
  std::vector<bool> affected(before.size(), false);
  for (size_t i = 0; i < before.size(); ++i)
    affected[i] = before[i] != after.geometry[i];
  if (std::none_of(affected.begin(), affected.end(), [](bool v) { return v; }))
    return absl::OkStatus();
  auto touches = [&](int map) {
    return map >= 0 && map < static_cast<int>(affected.size()) && affected[map];
  };
  // Those domains have no native geometry-coupled writer yet. Keep their
  // authored rows intact and report the precise dependency before mutation.
  for (const auto* list : {&all_entrances_, &all_holes_})
    for (const auto& e : *list)
      if (!e.deleted && touches(e.map_id_))
        return absl::FailedPreconditionError(
            "Area resize needs entrance/hole migration first");
  for (const auto& e : all_exits_)
    if (!e.deleted_ && touches(e.map_id_))
      return absl::FailedPreconditionError(
          "Area resize needs return-exit migration first");
  if (!core::FeatureFlags::get().overworld.kSaveOverworldItems) {
    auto item_on_touched_area = [&](int map_id, int x, int y) {
      if (touches(map_id))
        return true;
      if (x >= 0 && x < 4096 && y >= 0 && y < 4096 && map_id >= 0 &&
          map_id < materialized_map_count()) {
        const int page = (map_id / 64) * 64 + (y / 512) * 8 + (x / 512);
        if (touches(page))
          return true;
      }
      return false;
    };
    for (const auto& item : all_items_) {
      if (!item.deleted &&
          item_on_touched_area(item.room_map_id_, item.x_, item.y_)) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Enable item saving before resizing areas with items (map $%02X)",
            item.room_map_id_));
      }
    }
    constexpr size_t kItemFields = 8;
    const auto& baseline = native_nonterrain_baseline_[3];
    const size_t base_count =
        baseline.empty() ? 0 : static_cast<size_t>(baseline[0]);
    for (size_t idx = 0; idx < base_count; ++idx) {
      const size_t off = 1 + idx * kItemFields;
      if (off + 7 < baseline.size() && baseline[off + 7] == 0) {
        const int base_map = static_cast<int>(baseline[off]);
        if (item_on_touched_area(base_map, static_cast<int>(baseline[off + 4]),
                                 static_cast<int>(baseline[off + 5]))) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Enable item saving before resizing areas with pending items "
              "(map $%02X)",
              base_map));
        }
      }
    }
  }
  for (size_t i = 0; i < before.size(); ++i) {
    if (!affected[i] || before[i].parent == static_cast<int>(i) ||
        after.geometry[i].parent != static_cast<int>(i)) {
      continue;
    }
    const auto bomb = rom::LyraLayoutRecord(descriptor_layout_, rom_->data(),
                                            rom_->size(), 22, i);
    const auto pointer = rom::LyraLayoutRecord(descriptor_layout_, rom_->data(),
                                               rom_->size(), 23, i);
    if (!bomb || !pointer)
      return absl::FailedPreconditionError("Native item source is unavailable");
    if ((*bomb)[0] != 0 || (*bomb)[1] != 0)
      return absl::FailedPreconditionError(absl::StrFormat(
          "Map 0x%02X has dormant bomb-door data; preserve it before "
          "splitting",
          i));
    const uint16_t word = (*pointer)[0] | ((*pointer)[1] << 8);
    if ((word & 0x8000u) != 0) {
      ASSIGN_OR_RETURN(const auto item_bank,
                       rom_->ReadByte(overworldItemsAddressBank));
      const int bank = item_bank & 0x7F;
      if (bank != 0x7E && bank != 0x7F) {
        const int pc = SnesToPc((bank << 16) | word);
        if (pc >= 0 && static_cast<size_t>(pc + 1) < rom_->size() &&
            (rom_->data()[pc] != 0xFF || rom_->data()[pc + 1] != 0xFF)) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Map 0x%02X has dormant item data; preserve it before splitting",
              i));
        }
      }
    }
  }
  for (size_t row = 0; row < all_items_.size(); ++row) {
    const auto& item = all_items_[row];
    if (item.deleted || !touches(item.room_map_id_))
      continue;
    if (item.x_ < 0 || item.x_ >= 4096 || item.y_ < 0 || item.y_ >= 4096)
      return absl::InvalidArgumentError(
          "Item position cannot be rebased to an area");
    const int page =
        item.room_map_id_ / 64 * 64 + item.y_ / 512 * 8 + item.x_ / 512;
    if (page >= materialized_map_count() ||
        before[page].parent != item.room_map_id_)
      return absl::FailedPreconditionError(
          "Move offscreen area items inside their area before resizing");
    const int owner = after.geometry[page].parent;
    const int x = (item.x_ - owner % 8 * 512) / 16;
    const int y = (item.y_ - owner % 64 / 8 * 512) / 16;
    after.items[row] = {static_cast<uint16_t>(owner), static_cast<uint8_t>(x),
                        static_cast<uint8_t>(y)};
  }
  for (int phase = 0; phase < 3; ++phase) {
    for (size_t i = 0; i < before.size(); ++i) {
      if (!affected[i])
        continue;
      if (before[i].parent == static_cast<int>(i) &&
          !sprite_maps_loaded_[phase][i])
        return absl::FailedPreconditionError(
            "Load area sprites before resizing");
      after.loaded_sprite_maps[phase][i] =
          after.geometry[i].parent == static_cast<int>(i);
    }
    for (size_t row = 0; row < all_sprites_[phase].size(); ++row) {
      const auto& sprite = all_sprites_[phase][row];
      if (sprite.deleted() || !touches(sprite.map_id()))
        continue;
      if (sprite.x() < 0 || sprite.x() >= 4096 || sprite.y() < 0 ||
          sprite.y() >= 4096 || sprite.x() % 16 || sprite.y() % 16)
        return absl::InvalidArgumentError(
            "Sprite position cannot be rebased to an area");
      const int page =
          sprite.map_id() / 64 * 64 + sprite.y() / 512 * 8 + sprite.x() / 512;
      if (page >= materialized_map_count() ||
          before[page].parent != sprite.map_id())
        return absl::FailedPreconditionError(
            "Move offscreen area sprites inside their area before resizing");
      const int owner = after.geometry[page].parent;
      const int x = (sprite.x() - owner % 8 * 512) / 16;
      const int y = (sprite.y() - owner % 64 / 8 * 512) / 16;
      after.sprites[phase][row] = {
          static_cast<uint8_t>(owner), static_cast<uint8_t>(x),
          static_cast<uint8_t>(y), sprite.placement_id()};
    }
    for (size_t i = 0; i < before.size(); ++i) {
      if (!affected[i] || !after.loaded_sprite_maps[phase][i] ||
          sprite_maps_loaded_[phase][i])
        continue;
      ASSIGN_OR_RETURN(
          const auto dormant,
          ReadOverworldSpriteList(*rom_, descriptor_layout_, phase, i));
      // A dormant former-root stream can be redundant with the current live
      // area. Match packed fields and multiplicity in world space before
      // replacing its row; never activate or discard a distinct dormant actor.
      std::vector<bool> matched(all_sprites_[phase].size(), false);
      for (size_t pos = 0; pos + 1 < dormant.size(); pos += 3) {
        const int x = i % 8 * 512 + (dormant[pos + 1] & 0x3F) * 16;
        const int y = i % 64 / 8 * 512 + (dormant[pos] & 0x3F) * 16;
        bool found = false;
        for (size_t row = 0; row < all_sprites_[phase].size(); ++row) {
          const auto& sprite = all_sprites_[phase][row];
          if (!matched[row] && !sprite.deleted() &&
              sprite.map_id() == before[i].parent && sprite.x() == x &&
              sprite.y() == y && sprite.id() == dormant[pos + 2] &&
              sprite.overworld_x_flags() == (dormant[pos + 1] & 0xC0) &&
              sprite.overworld_y_flags() == (dormant[pos] & 0xC0)) {
            matched[row] = true;
            found = true;
            break;
          }
        }
        if (!found)
          return absl::FailedPreconditionError(
              absl::StrFormat("Map 0x%02X phase %d has distinct dormant "
                              "sprites; preserve them before splitting",
                              i, phase));
      }
    }
  }
  return RestoreNativeAreaSnapshot(after);
}

absl::StatusOr<std::vector<rom::NativeOverworldPropertyEdit>>
Overworld::PrepareNativeGeometrySave() const {
  ASSIGN_OR_RETURN(const auto rows,
                   PlanExpandedOverworldGeometry(overworld_maps_));
  if (rows.size() != native_geometry_baseline_.size())
    return absl::FailedPreconditionError(
        "Native geometry baseline is unavailable");
  std::vector<rom::NativeOverworldPropertyEdit> edits;
  for (size_t map = 0; map < rows.size(); ++map) {
    const auto& row = rows[map];
    if (row == native_geometry_baseline_[map])
      continue;
    for (int phase = 0; phase < 3; ++phase)
      if (sprite_maps_loaded_[phase][map] != (row.parent == map))
        return absl::FailedPreconditionError(
            "Use the area resize command to preserve sprite ownership");

    edits.push_back({7, static_cast<uint16_t>(map), {row.parent}});
    edits.push_back(
        {30, static_cast<uint16_t>(map), {static_cast<uint8_t>(row.size)}});
    for (size_t n = 0; n < row.words.size(); ++n)
      edits.push_back({static_cast<uint16_t>(8 + n),
                       static_cast<uint16_t>(map),
                       {static_cast<uint8_t>(row.words[n]),
                        static_cast<uint8_t>(row.words[n] >> 8)}});
  }
  return edits;
}

absl::Status Overworld::ValidateNativeGeometrySaveFlags() const {
  const auto& flags = core::FeatureFlags::get().overworld;
  ASSIGN_OR_RETURN(const auto geometry, PrepareNativeGeometrySave());
  if (geometry.empty())
    return absl::OkStatus();
  if (!flags.kSaveOverworldProperties || !flags.kSaveOverworldSprites)
    return absl::FailedPreconditionError(
        "Enable both area-property and sprite saving to publish pending "
        "geometry");
  if (!flags.kSaveOverworldItems) {
    const int map_count = materialized_map_count();
    std::vector<bool> geometry_changed(map_count, false);
    for (const auto& edit : geometry) {
      if (edit.map_id < map_count) {
        geometry_changed[edit.map_id] = true;
      }
    }
    auto touches_item = [&](int map_id, int x, int y) {
      if (map_id >= 0 && map_id < map_count && geometry_changed[map_id]) {
        return true;
      }
      if (x >= 0 && x < 4096 && y >= 0 && y < 4096 && map_id >= 0 &&
          map_id < map_count) {
        const int page = (map_id / 64) * 64 + (y / 512) * 8 + (x / 512);
        if (page >= 0 && page < map_count && geometry_changed[page]) {
          return true;
        }
      }
      return false;
    };
    for (const auto& item : all_items_) {
      if (!item.deleted && touches_item(item.room_map_id_, item.x_, item.y_)) {
        return absl::FailedPreconditionError(
            "Enable item saving to publish pending area resize affecting "
            "items");
      }
    }
    constexpr size_t kItemFields = 8;
    const auto& baseline = native_nonterrain_baseline_[3];
    const size_t base_count =
        baseline.empty() ? 0 : static_cast<size_t>(baseline[0]);
    for (size_t idx = 0; idx < base_count; ++idx) {
      const size_t off = 1 + idx * kItemFields;
      if (off + 7 < baseline.size() && baseline[off + 7] == 0 &&
          touches_item(static_cast<int>(baseline[off]),
                       static_cast<int>(baseline[off + 4]),
                       static_cast<int>(baseline[off + 5]))) {
        return absl::FailedPreconditionError(
            "Enable item saving to publish pending area resize affecting "
            "items");
      }
    }
  }
  return absl::OkStatus();
}
}  // namespace yaze::zelda3
