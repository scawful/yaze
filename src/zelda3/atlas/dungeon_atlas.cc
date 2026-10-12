#include "zelda3/atlas/dungeon_atlas.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <tuple>
#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "core/hack_manifest.h"
#include "rom/rom.h"
#include "util/rom_hash.h"
#include "zelda3/atlas/atlas_defaults.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_header_destination.h"

namespace yaze::zelda3::atlas {

namespace {

constexpr int kRooms = kAtlasRoomCount;

std::string RoomHex(int room) {
  return absl::StrFormat("0x%02X", room);
}

uint32_t LoRomPc(uint32_t snes) {
  return ((snes >> 16) & 0x7F) * 0x8000 + (snes & 0x7FFF);
}

bool IsHexString(const std::string& text) {
  return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
  });
}

// Display data for one census owner.
struct OwnerDisplay {
  bool own_area = false;  // False: the room falls through to annotation
                          // areas, then INT / OTHER.
  std::string code;
  std::string name;
  AtlasColor color;
  Origin origin;
  Origin color_origin;
};

class AreaTable {
 public:
  AtlasArea& Get(const std::string& key) {
    auto it = index_.find(key);
    if (it != index_.end()) {
      return areas_[it->second];
    }
    index_[key] = areas_.size();
    areas_.push_back({});
    areas_.back().key = key;
    return areas_.back();
  }
  bool Has(const std::string& key) const { return index_.contains(key); }
  std::vector<AtlasArea> Take() { return std::move(areas_); }

 private:
  std::vector<AtlasArea> areas_;
  std::map<std::string, size_t> index_;
};

std::string ExpandLabel(const std::string& templ, const std::string& kind,
                        int n) {
  const std::string base =
      templ.empty() ? absl::AsciiStrToUpper(kind) + " {n}" : templ;
  return absl::StrReplaceAll(base, {{"{n}", std::to_string(n)}});
}

}  // namespace

const char* RoomStatusName(RoomStatus status) {
  switch (status) {
    case RoomStatus::kInUse:
      return "in_use";
    case RoomStatus::kFree:
      return "free";
    case RoomStatus::kReclaimable:
      return "reclaimable";
    case RoomStatus::kOrphan:
      return "orphan";
    case RoomStatus::kReserved:
      return "reserved";
  }
  return "in_use";
}

const char* LinkKindName(LinkKind kind) {
  switch (kind) {
    case LinkKind::kEntrance:
      return "entrance";
    case LinkKind::kHole:
      return "hole";
    case LinkKind::kSpawn:
      return "spawn";
    case LinkKind::kStair:
      return "stair";
    case LinkKind::kPit:
      return "pit";
    case LinkKind::kWarpTiles:
      return "warp_tiles";
    case LinkKind::kWarpTag:
      return "warp_tag";
    case LinkKind::kPitDamage:
      return "pit_damage";
    case LinkKind::kTagPit:
      return "tag_pit";
    case LinkKind::kAnnotation:
      return "annotation";
    case LinkKind::kSymbol:
      return "symbol";
  }
  return "?";
}

absl::StatusOr<std::map<std::string, uint32_t>> ReadWlaSymbols(
    const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot read symbol file %s", path));
  }
  std::map<std::string, uint32_t> symbols;
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    const size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos || line.size() < start + 8 ||
        line[start + 2] != ':') {
      continue;
    }
    const std::string bank = line.substr(start, 2);
    const std::string addr = line.substr(start + 3, 4);
    if (!IsHexString(bank) || !IsHexString(addr)) {
      continue;
    }
    const size_t name_start = line.find_first_not_of(" \t", start + 7);
    if (name_start == std::string::npos || name_start == start + 7) {
      continue;
    }
    const size_t name_end = line.find_first_of(" \t", name_start);
    const std::string name = line.substr(name_start, name_end - name_start);
    const uint32_t snes =
        (std::stoul(bank, nullptr, 16) << 16) | std::stoul(addr, nullptr, 16);
    symbols.emplace(name, snes);
  }
  return symbols;
}

absl::StatusOr<DungeonAtlasInput> CollectDungeonAtlasInput(
    Rom* rom, const DungeonAtlasCollectOptions& options) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }
  DungeonAtlasInput input;
  input.extras.resize(kRooms);

  RoomCensusOptions census_options;
  census_options.project = options.registry;
  census_options.manifest = options.manifest;
  census_options.vanilla_rom = options.vanilla_rom;
  census_options.room_visitor = [&input](int room_id, const Room& room) {
    if (room_id < 0 || room_id >= kRooms) {
      return;
    }
    auto& extras = input.extras[room_id];
    for (const auto& door : room.GetDoors()) {
      const auto [tile_x, tile_y] = door.GetTileCoords();
      AtlasDoorFact fact;
      fact.direction = static_cast<int>(door.direction);
      fact.tile_x = tile_x;
      fact.tile_y = tile_y;
      fact.type = static_cast<int>(door.type);
      fact.type_name = std::string(door.GetTypeName());
      extras.doors.push_back(std::move(fact));
    }
    for (const auto& chest : room.GetChests()) {
      extras.chests.push_back({static_cast<int>(chest.id), chest.size});
    }
    for (const auto& pot : room.GetPotItems()) {
      extras.pot_items.push_back(pot.item);
    }
    for (const auto& sprite : room.GetSprites()) {
      extras.key_drops_small += sprite.key_drop() == 1 ? 1 : 0;
      extras.key_drops_big += sprite.key_drop() == 2 ? 1 : 0;
    }
  };
  auto census = CollectRoomCensusInput(rom, census_options);
  if (!census.ok()) {
    return census.status();
  }
  input.census = std::move(census).value();

  if (options.registry != nullptr) {
    for (const auto& dungeon : options.registry->dungeons) {
      for (const auto& room : dungeon.rooms) {
        if (room.id >= 0 && room.id < kRooms && !room.name.empty()) {
          input.registry_room_names.emplace(room.id, room.name);
        }
      }
    }
  }

  input.rom.name = std::filesystem::path(rom->filename()).filename().string();
  input.rom.size = rom->size();
  input.rom.sha1 = util::ComputeSha1Hex(rom->data(), rom->size());

  input.symbol_reads.resize(options.symbol_links.size());
  if (!options.symbol_links.empty() && !options.symbols_path.empty()) {
    auto symbols = ReadWlaSymbols(options.symbols_path);
    if (!symbols.ok()) {
      return symbols.status();
    }
    input.symbols_path = options.symbols_path;
    const auto& data = rom->vector();
    for (size_t i = 0; i < options.symbol_links.size(); ++i) {
      const auto& link = options.symbol_links[i];
      auto it = symbols->find(link.symbol);
      if (it == symbols->end()) {
        continue;
      }
      const size_t pc = LoRomPc(it->second);
      const size_t bytes = static_cast<size_t>(link.count) * link.width;
      if (pc + bytes > data.size()) {
        continue;
      }
      auto& read = input.symbol_reads[i];
      read.found = true;
      for (int n = 0; n < link.count; ++n) {
        const size_t at = pc + static_cast<size_t>(n) * link.width;
        read.values.push_back(link.width == 2 ? (data[at] | (data[at + 1] << 8))
                                              : data[at]);
      }
    }
  }
  return input;
}

DungeonAtlas BuildDungeonAtlas(const DungeonAtlasInput& input,
                               const AtlasAnnotations& annotations) {
  DungeonAtlas atlas;
  const RoomCensus census = BuildRoomCensus(input.census);
  atlas.rom = input.rom;
  atlas.title = annotations.title;
  atlas.title_origin = annotations.title_origin;
  atlas.owners_from_project = census.owners_from_project;
  atlas.expanded_entrance_tables = census.expanded_entrance_tables;
  atlas.symbols = input.symbols_path;
  atlas.annotation_files = annotations.files;
  atlas.legend_notes = annotations.legend_notes_dungeon;

  std::vector<RoomCensusRoomFacts> facts = input.census.rooms;
  facts.resize(kRooms);
  std::vector<DungeonAtlasRoomExtras> extras = input.extras;
  extras.resize(kRooms);

  const auto& palette = AtlasAreaPalette();
  size_t next_color = 0;
  auto palette_color = [&]() {
    return palette[next_color++ % palette.size()];
  };

  // ---- Owner display ----------------------------------------------------
  auto annotated_by_owner =
      [&](const std::string& id) -> const DungeonAnnotation* {
    for (const auto& dungeon : annotations.dungeons) {
      if (!dungeon.owner.empty() && dungeon.owner == id) {
        return &dungeon;
      }
    }
    return nullptr;
  };
  auto annotated_by_id = [&](int dungeon_id) -> const DungeonAnnotation* {
    for (const auto& dungeon : annotations.dungeons) {
      if (dungeon.owner.empty() && dungeon.dungeon_id == dungeon_id) {
        return &dungeon;
      }
    }
    return nullptr;
  };

  AreaTable areas;
  std::vector<OwnerDisplay> owners(census.owners.size());
  for (size_t i = 0; i < census.owners.size(); ++i) {
    const auto& owner = census.owners[i];
    OwnerDisplay& display = owners[i];
    if (owner.interior) {
      continue;  // Annotation areas first, then INT (below).
    }
    const DungeonAnnotation* entry = nullptr;
    if (owner.from_project) {
      entry = annotated_by_owner(owner.id);
      display.own_area = true;
      display.origin = Origin::Registry();
    } else {
      entry = annotated_by_id(owner.dungeon_id);
      // With a project registry, derived owners are dungeon IDs the project
      // does not list: OTHER, unless an annotation (not a default) names
      // them. The vanilla table only fits ROMs that still look vanilla.
      if (entry != nullptr && entry->origin.kind == Provenance::kDefault &&
          census.owners_from_project) {
        entry = nullptr;
      }
      display.own_area = !census.owners_from_project || entry != nullptr;
      display.origin = Origin::Rom();
    }
    if (!display.own_area) {
      continue;
    }
    const bool names_apply =
        entry != nullptr && (entry->origin.kind != Provenance::kDefault ||
                             census.vanilla_owner_names);
    display.code = names_apply && !entry->code.empty() ? entry->code : owner.id;
    display.name =
        names_apply && !entry->name.empty() ? entry->name : owner.name;
    if (names_apply && (!entry->code.empty() || !entry->name.empty())) {
      display.origin = entry->origin;
    }
    if (entry != nullptr && entry->color.has_value()) {
      display.color = *entry->color;
      display.color_origin = entry->color_origin;
    } else {
      display.color = palette_color();
      display.color_origin = Origin::Default();
    }
    AtlasArea& area = areas.Get(display.code);
    area.code = display.code;
    area.name = display.name;
    area.color = display.color;
    area.kind = "dungeon";
    area.owner = owner.id;
    area.origin = display.origin;
    area.color_origin = display.color_origin;
  }

  std::map<int, const AreaAnnotation*> area_of;
  for (const auto& area : annotations.areas) {
    for (int room : area.rooms) {
      area_of[room] = &area;  // Later areas win, as in room_data.py.
    }
    AtlasArea& out = areas.Get(area.key);
    out.code = area.code.empty() ? area.key : area.code;
    out.name = area.name.empty() ? area.key : area.name;
    out.kind = area.key == "INT" ? "interior" : "area";
    out.origin = area.origin;
    if (area.color.has_value()) {
      out.color = *area.color;
      out.color_origin = area.origin;
    } else if (area.key == "INT") {
      out.color = kAtlasInteriorColor;
      out.color_origin = Origin::Default();
    } else {
      out.color = palette_color();
      out.color_origin = Origin::Default();
    }
  }
  auto ensure_fallback_area = [&](const std::string& key) {
    if (areas.Has(key)) {
      return;
    }
    AtlasArea& area = areas.Get(key);
    area.code = key == "OTHER" ? "ID?" : key == "NONE" ? "" : key;
    area.color_origin = Origin::Default();
    area.origin = Origin::Default();
    if (key == "INT") {
      const DungeonAnnotation* entry = annotated_by_id(0xFF);
      area.kind = "interior";
      area.name =
          entry != nullptr && !entry->name.empty() ? entry->name : "Interiors";
      area.color = entry != nullptr && entry->color.has_value()
                       ? *entry->color
                       : kAtlasInteriorColor;
      if (entry != nullptr) {
        area.origin = entry->origin;
        area.color_origin = entry->color_origin;
      }
    } else if (key == "OTHER") {
      area.kind = "other";
      area.name = "Dungeon ID not in project";
      area.color = kAtlasOtherColor;
    } else {
      area.kind = "none";
      area.name = "No owner";
      area.color = kAtlasNoOwnerColor;
    }
  };

  // ---- Rooms: area, status, name ----------------------------------------
  atlas.rooms.resize(kRooms);
  for (int r = 0; r < kRooms; ++r) {
    const RoomCensusEntry& entry = census.rooms[r];
    DungeonAtlasRoom& room = atlas.rooms[r];
    room.room_id = r;
    room.reached = entry.reached;
    room.empty = entry.empty;
    room.interior = entry.interior;
    room.objects = entry.object_count;
    room.sprites = entry.sprite_count;
    room.vanilla_similarity = entry.vanilla_similarity;
    room.reasons = entry.reasons;
    room.tag1 = facts[r].tag1;
    room.tag2 = facts[r].tag2;
    room.blockset = facts[r].blockset;
    room.pit_damage = facts[r].in_pit_damage_table;
    if (entry.owner_index >= 0) {
      const auto& owner = census.owners[entry.owner_index];
      room.owner = owner.id;
      room.owner_name = owner.name;
    }

    const OwnerDisplay* display =
        entry.owner_index >= 0 ? &owners[entry.owner_index] : nullptr;
    const bool from_project =
        entry.owner_index >= 0 && census.owners[entry.owner_index].from_project;
    if (display != nullptr && display->own_area) {
      room.area = display->code;
      room.area_origin = from_project ? Origin::Registry() : Origin::Rom();
    } else if (auto it = area_of.find(r); it != area_of.end()) {
      room.area = it->second->key;
      room.area_origin = it->second->origin;
    } else if (room.interior) {
      room.area = "INT";
      room.area_origin = Origin::Rom();
      ensure_fallback_area("INT");
    } else if (entry.owner_index >= 0) {
      room.area = "OTHER";
      room.area_origin = Origin::Rom();
      ensure_fallback_area("OTHER");
    } else {
      room.area = "NONE";
      room.area_origin = Origin::Rom();
      ensure_fallback_area("NONE");
    }

    switch (entry.status) {
      case RoomCensusStatus::kFree:
        room.status = RoomStatus::kFree;
        break;
      case RoomCensusStatus::kReclaimable:
        room.status = RoomStatus::kReclaimable;
        break;
      case RoomCensusStatus::kInUse:
        room.status = entry.orphan ? RoomStatus::kOrphan : RoomStatus::kInUse;
        break;
    }
    room.status_origin = Origin::Rom();

    if (auto it = annotations.rooms.find(r); it != annotations.rooms.end()) {
      room.note = it->second.note;
      if (!it->second.name.empty()) {
        room.name = it->second.name;
        room.name_origin = it->second.origin;
      }
    }
    if (room.name.empty()) {
      if (auto it = input.registry_room_names.find(r);
          it != input.registry_room_names.end()) {
        room.name = it->second;
        room.name_origin = Origin::Registry();
      }
    }

    // Doors on the outer wall.
    room.doors = OuterWallDoors(extras[r].doors);

    // Badges.
    const auto& items = annotations.items;
    for (const auto& chest : extras[r].chests) {
      const bool known = chest.item == items.small_key ||
                         chest.item == items.compass ||
                         chest.item == items.big_key || chest.item == items.map;
      if (chest.big) {
        ++room.badges.big_chest;
      }
      if (chest.item == items.big_key) {
        ++room.badges.bk_chest;
      } else if (chest.item == items.small_key) {
        ++room.badges.k_chest;
      } else if (chest.item == items.compass) {
        ++room.badges.c;
      } else if (chest.item == items.map) {
        ++room.badges.m;
      } else if (!known && !chest.big) {
        ++room.badges.other;
      }
    }
    room.badges.k_pot = static_cast<int>(std::count(extras[r].pot_items.begin(),
                                                    extras[r].pot_items.end(),
                                                    items.pot_small_key));
    room.badges.k_drop = extras[r].key_drops_small;
    room.badges.bk_drop = extras[r].key_drops_big;
  }

  // ---- Links out ---------------------------------------------------------
  const RoomLinkFactsLookup lookup = [&](int id) -> const RoomLinkFacts* {
    return IsDungeonRoomId(id) ? &facts[id] : nullptr;
  };
  for (int r = 0; r < kRooms; ++r) {
    const RoomCensusRoomFacts& f = facts[r];
    DungeonAtlasRoom& room = atlas.rooms[r];
    std::vector<int> stair_targets;
    bool has_warp_tag_links = false;
    for (const auto& source :
         CollectRoomLinks(r, lookup, input.census.warp_tag_ids)) {
      AtlasLink link;
      link.from = r;
      link.to = source.to;
      if (source.kind == RoomReferenceKind::kStair ||
          source.kind == RoomReferenceKind::kWarpTag) {
        const bool stair = source.kind == RoomReferenceKind::kStair;
        if (source.to < 0) {
          atlas.warnings.push_back(absl::StrFormat(
              "room %s %s slot %d points past $127 (%s); dropped", RoomHex(r),
              stair ? "stair" : "warp tag", source.slot + 1,
              RoomHex(source.raw_to)));
          continue;
        }
        if (stair) {
          if (source.to == r ||
              std::find(stair_targets.begin(), stair_targets.end(),
                        source.to) != stair_targets.end()) {
            continue;
          }
          stair_targets.push_back(source.to);
          link.kind = LinkKind::kStair;
          if (f.stair_slot_estimated[source.slot]) {
            link.note = "slot by placement order";
          }
        } else {
          link.kind = LinkKind::kWarpTag;
          has_warp_tag_links = true;
        }
        link.index = source.slot + 1;
      } else if (source.kind == RoomReferenceKind::kHolewarp) {
        if (source.to < 0) {
          atlas.warnings.push_back(
              absl::StrFormat("room %s holewarp points past $127 (%s); dropped",
                              RoomHex(r), RoomHex(source.raw_to)));
          continue;
        }
        if (source.to == r) {
          continue;
        }
        if (!source.strong) {
          link.kind = LinkKind::kPitDamage;
        } else {
          switch (source.holewarp_cause) {
            case RoomHolewarpCause::kWarpTiles:
              // A warp tag replaces the raw holewarp destination with its
              // quadrant slot. Keep the existing Atlas presentation of those
              // slots instead of also showing the overwritten raw target.
              if (has_warp_tag_links) {
                continue;
              }
              link.kind = LinkKind::kWarpTiles;
              break;
            case RoomHolewarpCause::kTag:
              link.kind = LinkKind::kTagPit;
              break;
            case RoomHolewarpCause::kSprite:
              link.note = f.hole_sprite;
              [[fallthrough]];
            case RoomHolewarpCause::kPits:
              link.kind = LinkKind::kPit;
              break;
            case RoomHolewarpCause::kNone:
              continue;
          }
        }
      } else {
        // Atlas retains its ordinary-door presentation and ignores unused
        // header bytes; only shared active header/hole links become pills.
        continue;
      }
      room.links_out.push_back(std::move(link));
    }
  }

  // Annotation links (runtime-only).
  for (const auto& note : annotations.links) {
    if (note.view != AtlasView::kDungeon || note.from < 0 ||
        note.from >= kRooms) {
      continue;
    }
    DungeonAtlasRoom& room = atlas.rooms[note.from];
    for (const auto& existing : room.links_out) {
      if (existing.to == note.to && note.to >= 0 &&
          ((note.kind == "pit" && existing.kind == LinkKind::kPit) ||
           (note.kind == "warp" && existing.kind == LinkKind::kWarpTiles) ||
           (note.kind == "stair" && existing.kind == LinkKind::kStair))) {
        atlas.warnings.push_back(absl::StrFormat(
            "%s: %s link %s -> %s repeats a ROM fact", note.origin.ref,
            note.kind, RoomHex(note.from), RoomHex(note.to)));
        break;
      }
    }
    AtlasLink link;
    link.kind = LinkKind::kAnnotation;
    link.from = note.from;
    link.to = note.to;
    link.style = note.kind;
    link.label = note.label;
    link.note = note.note;
    link.origin = note.origin;
    room.links_out.push_back(std::move(link));
  }
  // ---- Links in ----------------------------------------------------------
  for (const auto& fact : input.census.entrances) {
    if (fact.room_id < 0 || fact.room_id >= kRooms) {
      continue;
    }
    LinkKind kind;
    switch (fact.kind) {
      case RoomReferenceKind::kEntrance:
        kind = LinkKind::kEntrance;
        break;
      case RoomReferenceKind::kHole:
        kind = LinkKind::kHole;
        break;
      case RoomReferenceKind::kSpawn:
        kind = LinkKind::kSpawn;
        break;
      default:
        continue;
    }
    auto& links = atlas.rooms[fact.room_id].links_in;
    const bool duplicate =
        std::any_of(links.begin(), links.end(), [&](const AtlasLink& seen) {
          return seen.kind == kind &&
                 (kind == LinkKind::kSpawn
                      ? seen.index == fact.entrance_id
                      : seen.entrance == fact.entrance_id &&
                            seen.ow_map == fact.ow_map);
        });
    if (duplicate) {
      continue;
    }
    AtlasLink link;
    link.kind = kind;
    link.to = fact.room_id;
    if (kind == LinkKind::kSpawn) {
      link.index = fact.entrance_id;
    } else {
      link.entrance = fact.entrance_id;
      link.ow_map = fact.ow_map;
    }
    link.origin = Origin::Rom();
    links.push_back(std::move(link));
  }
  for (size_t i = 0; i < annotations.symbol_links.size(); ++i) {
    const auto& symbol_link = annotations.symbol_links[i];
    const AtlasSymbolRead* read =
        i < input.symbol_reads.size() ? &input.symbol_reads[i] : nullptr;
    if (read == nullptr || !read->found) {
      atlas.warnings.push_back(
          input.symbols_path.empty()
              ? absl::StrFormat("%s: symbol link %s skipped: no .sym file",
                                symbol_link.origin.ref, symbol_link.symbol)
              : absl::StrFormat("%s: symbol %s not found in %s",
                                symbol_link.origin.ref, symbol_link.symbol,
                                std::filesystem::path(input.symbols_path)
                                    .filename()
                                    .string()));
      continue;
    }
    for (size_t n = 0; n < read->values.size(); ++n) {
      const int target = read->values[n];
      if (target < 0 || target >= kRooms) {
        atlas.warnings.push_back(absl::StrFormat(
            "symbol %s entry %d is room %s, past $127; dropped",
            symbol_link.symbol, static_cast<int>(n + 1), RoomHex(target)));
        continue;
      }
      AtlasLink link;
      link.kind = LinkKind::kSymbol;
      link.to = target;
      link.index = static_cast<int>(n + 1);
      link.style = symbol_link.kind;
      link.label = ExpandLabel(symbol_link.label, symbol_link.kind,
                               static_cast<int>(n + 1));
      link.origin = Origin::Symbol(symbol_link.symbol);
      atlas.rooms[target].links_in.push_back(std::move(link));
    }
  }

  // ---- Cross-area rings, reserved rooms, plans ---------------------------
  for (auto& room : atlas.rooms) {
    for (auto& link : room.links_out) {
      if (link.to >= 0 && link.to < kRooms) {
        link.cross_area = atlas.rooms[link.to].area != room.area;
      }
    }
  }
  auto make_reserved = [&](DungeonAtlasRoom& room, const Origin& origin) {
    if (room.status == RoomStatus::kFree ||
        room.status == RoomStatus::kReclaimable) {
      room.status = RoomStatus::kReserved;
      room.status_origin = origin;
    }
  };
  for (const auto& [room_id, note] : annotations.rooms) {
    if (note.reserved && room_id >= 0 && room_id < kRooms) {
      make_reserved(atlas.rooms[room_id], note.origin);
    }
  }
  for (auto& room : atlas.rooms) {
    for (const auto& link : room.links_in) {
      if (link.kind == LinkKind::kSymbol) {
        make_reserved(room, link.origin);
      }
    }
    for (const auto& link : room.links_out) {
      if (link.kind == LinkKind::kAnnotation && link.to >= 0 &&
          link.to < kRooms) {
        make_reserved(atlas.rooms[link.to], link.origin);
      }
    }
  }
  for (const auto& plan : annotations.plans) {
    AtlasPlan out;
    out.key = plan.key;
    out.label = plan.label.empty() ? plan.key : plan.label;
    out.name = plan.name;
    out.status = plan.status;
    out.has_color = plan.color.has_value();
    if (plan.color.has_value()) {
      out.color = *plan.color;
    }
    out.rooms = plan.rooms;
    out.note = plan.note;
    out.origin = plan.origin;
    for (int room : plan.rooms) {
      if (room >= 0 && room < kRooms) {
        atlas.rooms[room].plans.push_back(plan.key);
      }
    }
    atlas.plans.push_back(std::move(out));
  }

  // ---- Edges, areas, key table, counts -----------------------------------
  std::vector<std::vector<AtlasDoor>> doors_by_room(kRooms);
  for (int r = 0; r < kRooms; ++r) {
    doors_by_room[r] = atlas.rooms[r].doors;
  }
  atlas.edges = PairDoorEdges(doors_by_room);

  std::map<std::string, AtlasKeyTableRow> key_rows;
  for (const auto& room : atlas.rooms) {
    ++areas.Get(room.area).rooms;
    auto& row = key_rows[room.area];
    row.area = room.area;
    row.k_chest += room.badges.k_chest;
    row.k_pot += room.badges.k_pot;
    row.k_drop += room.badges.k_drop;
    row.bk += room.badges.bk_chest + room.badges.bk_drop;
    row.bc += room.badges.big_chest;
    row.c += room.badges.c;
    row.m += room.badges.m;
    switch (room.status) {
      case RoomStatus::kFree:
        ++atlas.counts.free;
        break;
      case RoomStatus::kReclaimable:
        ++atlas.counts.reclaimable;
        break;
      case RoomStatus::kInUse:
        ++atlas.counts.in_use;
        break;
      case RoomStatus::kOrphan:
        ++atlas.counts.orphan;
        break;
      case RoomStatus::kReserved:
        ++atlas.counts.reserved;
        break;
    }
    atlas.counts.interior += room.interior ? 1 : 0;
  }
  atlas.areas = areas.Take();
  for (const auto& area : atlas.areas) {
    const auto it = key_rows.find(area.key);
    if (it == key_rows.end()) {
      continue;
    }
    const auto& row = it->second;
    if (row.k_chest + row.k_pot + row.k_drop + row.bk + row.bc + row.c + row.m >
        0) {
      atlas.key_table.push_back(row);
    }
  }
  return atlas;
}

}  // namespace yaze::zelda3::atlas
