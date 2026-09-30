#include "zelda3/graphics/resolved_graphics_context.h"

#include <string_view>

#include "absl/strings/str_format.h"
#include "app/gfx/types/sheet_role_palette_table.h"
#include "core/source_artifact_publisher.h"
#include "core/terrain_catalog.h"
#include "nlohmann/json.hpp"
#include "zelda3/dungeon/room.h"
#include "zelda3/graphics_sheet_store.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::zelda3 {
namespace {

using gfx::DefaultBindingFor;
using gfx::RoleForBlocksetSlot;
using gfx::RoleForRoomsetSlot;
using gfx::SheetRole;

std::string RoleName(SheetRole role) {
  switch (role) {
    case SheetRole::kOverworldMain:
      return "overworld_main";
    case SheetRole::kOverworldGfx:
      return "overworld_gfx";
    case SheetRole::kOverworldAreaAux1:
      return "overworld_area_aux1";
    case SheetRole::kOverworldAreaAux2:
      return "overworld_area_aux2";
    case SheetRole::kOverworldAnimated:
      return "overworld_animated";
    case SheetRole::kSpriteAux1:
      return "sprite_aux1";
    case SheetRole::kHud:
      return "hud";
    case SheetRole::kDungeonMain:
      return "dungeon_main";
    case SheetRole::kUnclassified:
      return "unclassified";
  }
  return "unclassified";
}

std::string PaletteGroupFor(SheetRole role) {
  return std::string(DefaultBindingFor(role).palette_group_name);
}

uint64_t MixRevision(uint64_t revision, int slot, uint64_t sheet_revision) {
  revision ^=
      sheet_revision + 0x9e3779b97f4a7c15ULL * static_cast<uint64_t>(slot + 1);
  return revision;
}

uint64_t SheetRevision(const GameData* game_data, int sheet_id) {
  if (game_data == nullptr || sheet_id < 0 ||
      sheet_id >= GraphicsSheetStore::kSheetCount) {
    return 0;
  }
  return game_data->sheet_store.Revision(static_cast<uint16_t>(sheet_id));
}

void FillSheetShape(ResolvedGraphicsSlot* slot) {
  slot->width = static_cast<int>(GraphicsSheetStore::kSheetWidth);
  slot->height = static_cast<int>(GraphicsSheetStore::kSheetHeight);
}

std::string RomSha256(const Rom& rom) {
  const auto& bytes = rom.vector();
  return core::ComputeSourceArtifactSha256(std::string_view(
      reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

void AttachIdentity(ResolvedGraphicsContext* context, const Rom* rom,
                    const GraphicsContextIdentity& identity,
                    std::string_view surface_bytes) {
  context->project_name = identity.project_name;
  if (rom != nullptr && rom->is_loaded()) {
    context->rom_sha256 = RomSha256(*rom);
  }
  context->graphics_fingerprint = core::TerrainFingerprint(surface_bytes);
  if (identity.terrain_catalog == nullptr) {
    context->terrain_catalog = "not_attached";
    return;
  }
  context->terrain_catalog = "attached";
  const auto& document = identity.terrain_catalog->document();
  if (!document.contains("entries") || !document["entries"].is_array()) {
    return;
  }
  for (const auto& entry : document["entries"]) {
    if (!entry.contains("bindings") || !entry["bindings"].is_array()) {
      continue;
    }
    for (const auto& binding : entry["bindings"]) {
      if (binding.contains("adapter") &&
          binding["adapter"] == "alttp.overworld.tile16.v1") {
        ++context->terrain_adapter_entries;
        break;
      }
    }
  }
}

nlohmann::json SlotJson(const ResolvedGraphicsSlot& slot) {
  nlohmann::json json = {
      {"slot", slot.slot},
      {"role", slot.role},
      {"palette_group", slot.palette_group},
      {"sheet_id", slot.sheet_id},
      {"source", slot.source},
      {"inherited_sheet", slot.inherited_sheet},
      {"loader_agrees", slot.loader_agrees},
      {"store_revision", std::to_string(slot.store_revision)},
      {"width", slot.width},
      {"height", slot.height},
      {"composite", slot.composite},
  };
  if (slot.composite) {
    json["composite_upper_sheet"] = slot.composite_upper_sheet;
    json["composite_upper_source"] = slot.composite_upper_source;
    json["composite_upper_revision"] =
        std::to_string(slot.composite_upper_revision);
  }
  return json;
}

bool DeathMountain(int area) {
  return area == 0x03 || area == 0x05 || area == 0x07 || area == 0x43 ||
         area == 0x45 || area == 0x47;
}

}  // namespace

std::string ResolvedGraphicsContext::ToJson() const {
  nlohmann::json document = {
      {"schema", schema},
      {"surface", surface},
      {"graphics_loaded", graphics_loaded},
      {"id", id},
      {"parent_id", parent_id},
      {"project_name", project_name},
      {"rom_sha256", rom_sha256},
      {"graphics_revision", std::to_string(graphics_revision)},
      {"graphics_fingerprint", graphics_fingerprint},
      {"terrain_catalog", terrain_catalog},
      {"terrain_adapter_entries", terrain_adapter_entries},
      {"palette_colors_resolved", palette_colors_resolved},
      {"loader_agrees", loader_agrees},
      {"slots", nlohmann::json::array()},
      {"palettes", nlohmann::json::array()},
      {"alternates", nlohmann::json::array()},
  };
  for (const auto& slot : slots) {
    document["slots"].push_back(SlotJson(slot));
  }
  for (const auto& palette : palettes) {
    document["palettes"].push_back({
        {"name", palette.name},
        {"source", palette.source},
        {"selector", palette.selector},
        {"inherited", palette.inherited},
    });
  }
  for (const auto& alternate : alternates) {
    document["alternates"].push_back({
        {"name", alternate.name},
        {"state", alternate.state},
        {"detail", alternate.detail},
    });
  }
  return document.dump();
}

ResolvedGraphicsContext DescribeOverworldGraphicsContext(
    const OverworldMap& map, const GraphicsContextIdentity& identity) {
  ResolvedGraphicsContext context;
  context.surface = "overworld_map";
  context.id = map.index();
  context.parent_id = map.parent();
  context.graphics_loaded = map.main_gfx_id() != 0;
  if (map.rom_ == nullptr) {
    AttachIdentity(&context, nullptr, identity, "");
    return context;
  }
  if (!context.graphics_loaded) {
    AttachIdentity(&context, map.rom_, identity, "");
    return context;
  }

  Rom& rom = *map.rom_;
  const auto version = OverworldVersionHelper::GetVersion(rom);
  const auto constants = map.version_constants();
  const auto props = map.area_render_properties();
  const bool custom_tiles =
      OverworldVersionHelper::SupportsCustomTileGFX(version) &&
      rom[OverworldCustomTileGFXGroupEnabled] != 0;
  const int world_offset = (map.parent() & 0xC0) >> 3;
  uint8_t main_sheets[8] = {};
  for (int i = 0; i < 8; ++i) {
    main_sheets[i] =
        rom[constants.kOverworldGfxGroups2 + (map.main_gfx_id() * 8) + i];
  }
  uint8_t area_sheets[4] = {};
  for (int i = 0; i < 4; ++i) {
    area_sheets[i] =
        rom[constants.kOverworldGfxGroups1 + (map.area_graphics() * 4) + i];
  }

  std::string fingerprint_bytes;
  context.loader_agrees = true;
  context.slots.reserve(16);
  for (int slot = 0; slot < 16; ++slot) {
    ResolvedGraphicsSlot resolved;
    resolved.slot = slot;
    resolved.sheet_id = map.static_graphics(slot);
    FillSheetShape(&resolved);
    resolved.store_revision = SheetRevision(map.game_data_, resolved.sheet_id);
    context.graphics_revision =
        MixRevision(context.graphics_revision, slot, resolved.store_revision);
    fingerprint_bytes.push_back(static_cast<char>(resolved.sheet_id));

    if (slot < 8) {
      resolved.role = RoleName(RoleForBlocksetSlot(slot));
      resolved.palette_group = PaletteGroupFor(RoleForBlocksetSlot(slot));
      resolved.inherited_sheet = main_sheets[slot];
      uint8_t predicted = main_sheets[slot];
      resolved.source = "main_group";
      if (!custom_tiles && slot >= 3 && slot <= 6 &&
          area_sheets[slot - 3] != 0) {
        predicted = area_sheets[slot - 3];
        resolved.source = "area_override";
      }
      if (custom_tiles) {
        uint8_t custom_sheet = props.custom_gfx_ids[slot];
        if (custom_sheet == 0xFF) {
          predicted =
              rom[OverworldCustomDefaultGFXGroups + world_offset + slot];
          resolved.source = "world_default";
        } else {
          predicted = custom_sheet;
          resolved.source = "custom_tile_gfx";
        }
      }
      resolved.loader_agrees = predicted == resolved.sheet_id;
    } else if (slot < 12) {
      static constexpr uint8_t kStaticSpriteSheets[] = {0x73, 0x74, 0x79, 0x7A};
      resolved.role = "static_sprite";
      resolved.palette_group = PaletteGroupFor(SheetRole::kSpriteAux1);
      resolved.source = "static_sprite";
      resolved.inherited_sheet = kStaticSpriteSheets[slot - 8];
      resolved.loader_agrees = resolved.sheet_id == resolved.inherited_sheet;
    } else {
      resolved.role = "spriteset";
      resolved.palette_group = PaletteGroupFor(SheetRole::kSpriteAux1);
      resolved.source = "spriteset";
      const int set = map.sprite_graphics(map.game_state());
      const int predicted = static_cast<uint8_t>(
          rom[constants.kSpriteBlocksetPointer + (set * 4) + (slot - 12)] +
          0x73);
      resolved.inherited_sheet = set;
      resolved.loader_agrees = predicted == resolved.sheet_id;
    }
    context.loader_agrees = context.loader_agrees && resolved.loader_agrees;
    context.slots.push_back(resolved);
  }

  auto& slot7 = context.slots[7];
  slot7.composite = true;
  const uint8_t raw_animated = props.animated_gfx;
  const bool custom_animated = version != OverworldVersion::kVanilla &&
                               rom[OverworldCustomAnimatedGFXEnabled] != 0;
  uint8_t predicted_upper = raw_animated;
  if (custom_animated) {
    if (raw_animated == 0x00 || raw_animated == 0xFF) {
      predicted_upper = rom[OverworldCustomDefaultGFXGroups + world_offset + 7];
      slot7.composite_upper_source = "world_default";
    } else {
      slot7.composite_upper_source = "animated_table";
    }
  } else if (raw_animated == 0x00 || raw_animated == 0xFF) {
    predicted_upper = DeathMountain(map.parent()) ? 0x59 : 0x5B;
    slot7.composite_upper_source = "vanilla_default";
  } else {
    slot7.composite_upper_source = "animated_table";
  }
  slot7.composite_upper_sheet = map.animated_sheet();
  slot7.composite_upper_revision =
      SheetRevision(map.game_data_, slot7.composite_upper_sheet);
  if (slot7.composite_upper_sheet != slot7.sheet_id) {
    context.graphics_revision = MixRevision(context.graphics_revision, 16,
                                            slot7.composite_upper_revision);
  }
  const bool upper_agrees = predicted_upper == slot7.composite_upper_sheet;
  slot7.loader_agrees = slot7.loader_agrees && upper_agrees;
  context.loader_agrees = context.loader_agrees && upper_agrees;
  fingerprint_bytes.push_back(static_cast<char>(slot7.composite_upper_sheet));

  context.palettes.push_back({
      "area",
      OverworldVersionHelper::SupportsAreaEnum(version)
          ? "expanded_screen_palette"
          : "vanilla_screen_palette",
      map.area_palette(),
      false,
  });
  context.palettes.push_back({
      "main",
      map.has_inherited_area_properties() ? "parent_area" : "area_main_palette",
      props.main_palette,
      map.has_inherited_area_properties(),
  });
  const int area_palette = map.area_palette();
  const uint8_t aux1 =
      rom[constants.kOverworldMapPaletteGroup + (area_palette * 4)];
  const uint8_t aux2 =
      rom[constants.kOverworldMapPaletteGroup + (area_palette * 4) + 1];
  const uint8_t animated =
      rom[constants.kOverworldMapPaletteGroup + (area_palette * 4) + 2];
  context.palettes.push_back({"aux1", "overworld_aux", aux1, aux1 == 0xFF});
  context.palettes.push_back({"aux2", "overworld_aux", aux2, aux2 == 0xFF});
  context.palettes.push_back(
      {"animated", "overworld_animated", animated, animated == 0xFF});
  const int sprite_palette = map.sprite_palette(map.game_state());
  const uint8_t sprite0 =
      rom[kOverworldSpritePaletteGroup + (sprite_palette * 2)];
  const uint8_t sprite1 =
      rom[kOverworldSpritePaletteGroup + (sprite_palette * 2) + 1];
  context.palettes.push_back(
      {"sprite0", "sprite_palette_group", sprite0, sprite0 == 0xFF});
  context.palettes.push_back(
      {"sprite1", "sprite_palette_group", sprite1, sprite1 == 0xFF});
  const bool custom_bg =
      OverworldVersionHelper::SupportsCustomBGColors(version) &&
      rom[OverworldCustomAreaSpecificBGEnabled] != 0;
  context.palettes.push_back({
      "backdrop",
      custom_bg ? "area_specific_bg" : "world_default",
      custom_bg             ? map.area_specific_bg_color()
      : map.parent() < 0x40 ? 0
      : map.parent() < 0x80 ? 1
                            : 2,
      false,
  });
  context.palette_colors_resolved = !map.current_palette().empty();

  for (int state = 0; state < 3; ++state) {
    context.alternates.push_back({
        absl::StrFormat("sprite_state_%d", state),
        state == map.game_state() ? "active" : "recorded",
        absl::StrFormat("graphics=0x%02X palette=0x%02X",
                        map.sprite_graphics(state), map.sprite_palette(state)),
    });
  }
  context.alternates.push_back(
      {"day_night_tint", "unsupported", "not resolved by this loader"});
  context.alternates.push_back({"runtime_palette_transfer", "unsupported",
                                "not resolved by this loader"});

  AttachIdentity(&context, &rom, identity, fingerprint_bytes);
  return context;
}

ResolvedGraphicsContext DescribeDungeonGraphicsContext(
    const Room& room, const GraphicsContextIdentity& identity) {
  ResolvedGraphicsContext context;
  context.surface = "dungeon_room";
  context.id = room.id();
  context.parent_id = room.id();
  context.graphics_loaded = room.resolved_main_blockset_ != 0xFF;
  if (!context.graphics_loaded || room.rom_ == nullptr ||
      room.game_data_ == nullptr) {
    AttachIdentity(&context, room.rom_, identity, "");
    context.graphics_loaded = false;
    return context;
  }

  const GameData& data = *room.game_data_;
  const auto blocks = room.blocks();
  const uint8_t main_index = room.resolved_main_blockset_;
  const bool main_ok = main_index < data.main_blockset_ids.size();
  const bool room_ok = room.blockset() < data.room_blockset_ids.size();
  const size_t sprite_index = static_cast<size_t>(room.spriteset()) + 64;
  const bool sprites_ok = sprite_index < data.spriteset_ids.size();
  static constexpr uint8_t kStaticSpriteSheets[] = {115, 125, 121, 122};

  std::string fingerprint_bytes;
  context.loader_agrees = main_ok;
  context.slots.reserve(16);
  for (int slot = 0; slot < 16; ++slot) {
    ResolvedGraphicsSlot resolved;
    resolved.slot = slot;
    resolved.sheet_id = blocks[slot];
    FillSheetShape(&resolved);
    resolved.store_revision = SheetRevision(&data, resolved.sheet_id);
    context.graphics_revision =
        MixRevision(context.graphics_revision, slot, resolved.store_revision);
    fingerprint_bytes.push_back(static_cast<char>(resolved.sheet_id));

    if (slot < 8) {
      if (slot == 7) {
        resolved.role = "main_blockset";
        resolved.palette_group = PaletteGroupFor(SheetRole::kDungeonMain);
      } else if (slot >= 3) {
        // The shared roomset table also serves the overworld, but dungeon
        // sheets use the room's dungeon palette, not overworld aux palettes.
        resolved.role = "room_blockset";
        resolved.palette_group = PaletteGroupFor(SheetRole::kDungeonMain);
      } else {
        resolved.role = "main_blockset";
        resolved.palette_group = PaletteGroupFor(SheetRole::kDungeonMain);
      }
      resolved.source = "main_blockset";
      uint8_t predicted = 0;
      if (main_ok) {
        predicted = data.main_blockset_ids[main_index][slot];
        resolved.inherited_sheet = predicted;
      }
      if (slot >= 3 && slot <= 6 && room_ok) {
        const uint8_t room_sheet =
            data.room_blockset_ids[room.blockset()][slot - 3];
        if (room_sheet != 0) {
          predicted = room_sheet;
          resolved.source = "room_blockset";
        }
      }
      resolved.loader_agrees = main_ok && predicted == resolved.sheet_id;
    } else if (slot < 12) {
      resolved.role = "static_sprite";
      resolved.palette_group = PaletteGroupFor(SheetRole::kSpriteAux1);
      resolved.source = "static_sprite";
      resolved.inherited_sheet = kStaticSpriteSheets[slot - 8];
      resolved.loader_agrees = resolved.sheet_id == resolved.inherited_sheet;
    } else {
      resolved.role = "spriteset";
      resolved.palette_group = PaletteGroupFor(SheetRole::kSpriteAux1);
      resolved.source = sprites_ok ? "spriteset" : "spriteset_missing";
      resolved.inherited_sheet = room.spriteset();
      const int predicted =
          sprites_ok ? static_cast<uint8_t>(
                           data.spriteset_ids[sprite_index][slot - 12] + 115)
                     : 0;
      resolved.loader_agrees = predicted == resolved.sheet_id;
    }
    context.loader_agrees = context.loader_agrees && resolved.loader_agrees;
    context.slots.push_back(resolved);
  }

  context.palettes.push_back({"header", "room_header", room.palette(), false});
  if (room.palette() < data.paletteset_ids.size()) {
    static constexpr const char* kSetNames[] = {
        "dungeon_main_offset", "sprite_aux1", "sprite_aux3", "sprite_aux3_b"};
    for (int i = 0; i < 4; ++i) {
      context.palettes.push_back({kSetNames[i], "dungeon_palette_set",
                                  data.paletteset_ids[room.palette()][i],
                                  false});
    }
  }
  context.palettes.push_back({"dungeon_main", "palette_pointer",
                              room.ResolveDungeonPaletteId(), false});
  context.palette_colors_resolved = !room.rendered_dungeon_palette().empty();
  context.alternates.push_back(
      {"entrance_world", "unresolved",
       "the room does not retain the entrance overworld"});
  context.alternates.push_back(
      {"sprite_environment", "light_world_fallback",
       "preview uses the outdoor light-world sprite environment"});

  AttachIdentity(&context, room.rom_, identity, fingerprint_bytes);
  return context;
}

}  // namespace yaze::zelda3
