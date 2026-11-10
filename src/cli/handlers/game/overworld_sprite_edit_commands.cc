#include "cli/handlers/game/overworld_sprite_edit_commands.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/handlers/game/overworld_inspect.h"
#include "cli/util/hex_util.h"
#include "core/features.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/native_overworld_authoring_metadata.h"
#include "rom/rom.h"
#include "rom/transaction.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_layout.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_sprite_io.h"
#include "zelda3/overworld/overworld_version_helper.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

struct Entry {
  uint8_t y;  // raw byte: flags in bits 6-7, tile in bits 0-5
  uint8_t x;
  uint8_t id;
};

bool operator==(const Entry& a, const Entry& b) {
  return a.y == b.y && a.x == b.x && a.id == b.id;
}

// Parsed operands of the requested edit; index is -1 when not given.
struct SpriteOp {
  int index = -1;
  int x = 0;
  int y = 0;
  int id = 0;
};

std::vector<Entry> Decode(const zelda3::OverworldSpriteBytes& bytes) {
  std::vector<Entry> out;
  for (size_t i = 0; i + 3 <= bytes.size() && bytes[i] != 0xFF; i += 3)
    out.push_back({bytes[i], bytes[i + 1], bytes[i + 2]});
  return out;
}

zelda3::OverworldSpriteBytes Encode(const std::vector<Entry>& entries) {
  zelda3::OverworldSpriteBytes out;
  for (const auto& e : entries) {
    out.push_back(e.y);
    out.push_back(e.x);
    out.push_back(e.id);
  }
  out.push_back(0xFF);
  return out;
}

std::string Hex(const std::vector<uint8_t>& bytes) {
  std::vector<std::string> parts;
  parts.reserve(bytes.size());
  for (uint8_t b : bytes)
    parts.push_back(absl::StrFormat("%02X", b));
  return absl::StrJoin(parts, " ");
}

std::string Snes(int pc) {
  // LoROM: bank = pc / 0x8000, address = $8000 | (pc & 0x7FFF).
  return absl::StrFormat("$%02X:%04X", pc / 0x8000, 0x8000 | (pc & 0x7FFF));
}

void AddEntries(resources::OutputFormatter& formatter, const std::string& key,
                const std::vector<Entry>& entries) {
  formatter.BeginArray(key);
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& e = entries[i];
    formatter.BeginObject();
    formatter.AddField("index", static_cast<int>(i));
    formatter.AddField("sprite_id", absl::StrFormat("0x%02X", e.id));
    formatter.AddField("name", zelda3::ResolveSpriteName(e.id));
    formatter.AddField("tile",
                       absl::StrFormat("(%d,%d)", e.x & 0x3F, e.y & 0x3F));
    formatter.AddField("bytes",
                       absl::StrFormat("%02X %02X %02X", e.y, e.x, e.id));
    formatter.EndObject();
  }
  formatter.EndArray();
}

absl::StatusOr<int> RequiredInt(const resources::ArgumentParser& parser,
                                const std::string& name) {
  if (!parser.GetString(name).has_value())
    return absl::InvalidArgumentError(
        absl::StrFormat("--%s is required", name));
  return parser.GetInt(name);
}

int AreaOriginX(int map) {
  return (map % 8) * 512;
}
int AreaOriginY(int map) {
  return ((map % 64) / 8) * 512;
}

// Same packing as Overworld::PrepareSpriteEdits.
Entry EntryFromSprite(const zelda3::Sprite& sprite) {
  const int map = sprite.map_id();
  const int x = (sprite.x() - AreaOriginX(map)) / 16;
  const int y = (sprite.y() - AreaOriginY(map)) / 16;
  return {static_cast<uint8_t>(y | sprite.overworld_y_flags()),
          static_cast<uint8_t>(x | sprite.overworld_x_flags()), sprite.id()};
}

// Live rows owned by one area, in model order (the stored list order).
std::vector<size_t> NativeRows(const std::vector<zelda3::Sprite>& records,
                               int map) {
  std::vector<size_t> rows;
  for (size_t i = 0; i < records.size(); ++i)
    if (!records[i].deleted() && records[i].map_id() == map)
      rows.push_back(i);
  return rows;
}

using NativeLists = std::array<std::map<int, std::vector<Entry>>, 3>;

NativeLists CaptureNativeLists(zelda3::Overworld& overworld) {
  NativeLists lists;
  for (int phase = 0; phase < 3; ++phase)
    for (const auto& sprite : *overworld.mutable_sprites(phase))
      if (!sprite.deleted())
        lists[phase][sprite.map_id()].push_back(EntryFromSprite(sprite));
  return lists;
}

absl::Status ApplyNativeSpriteOp(zelda3::Overworld& overworld,
                                 OverworldSpriteEditAction action, int phase,
                                 int parent, const SpriteOp& op) {
  auto& records = *overworld.mutable_sprites(phase);
  const auto rows = NativeRows(records, parent);
  const int world_x = AreaOriginX(parent) + op.x * 16;
  const int world_y = AreaOriginY(parent) + op.y * 16;
  switch (action) {
    case OverworldSpriteEditAction::kAdd: {
      if (op.index >= 0)
        records[rows[op.index]].set_deleted(true);
      static const std::vector<uint8_t> kNoGraphics;
      zelda3::Sprite sprite(kNoGraphics, static_cast<uint8_t>(parent),
                            static_cast<uint8_t>(op.id),
                            static_cast<uint8_t>(op.x),
                            static_cast<uint8_t>(op.y), world_x, world_y);
      RETURN_IF_ERROR(overworld.ReserveNativeAuthoringSpritePlacement(
          phase, parent, &sprite));
      records.push_back(std::move(sprite));
      break;
    }
    case OverworldSpriteEditAction::kMove: {
      auto& sprite = records[rows[op.index]];
      sprite.x_ = world_x;
      sprite.y_ = world_y;
      sprite.SetOverworldArea(static_cast<uint8_t>(parent),
                              static_cast<uint8_t>(op.x),
                              static_cast<uint8_t>(op.y));
      sprite.UpdateCoordinates(world_x, world_y);
      break;
    }
    case OverworldSpriteEditAction::kRemove:
      records[rows[op.index]].set_deleted(true);
      break;
  }
  return absl::OkStatus();
}

// The normal native Save transaction, limited to the sprite domain.
absl::Status SaveNativeSprites(zelda3::Overworld& overworld) {
  const auto old_flags = core::FeatureFlags::get();
  auto restore_flags =
      absl::MakeCleanup([&] { core::FeatureFlags::get() = old_flags; });
  auto& flags = core::FeatureFlags::get();
  flags.overworld.kSaveOverworldMaps = false;
  flags.overworld.kSaveOverworldProperties = false;
  flags.overworld.kSaveOverworldSprites = true;
  flags.overworld.kSaveOverworldEntrances = false;
  flags.overworld.kSaveOverworldExits = false;
  flags.overworld.kSaveOverworldItems = false;
  flags.kSaveAllPalettes = false;
  flags.kSaveGfxGroups = false;
  return overworld.SaveNativeTerrain();
}

// A native sprite Save may only rewrite the storage PlanNativeOverworldSpriteSave
// owns: maps $00-$9F pack into the original list region and its three pointer
// mirrors; tail maps, pointers and metadata live in directory tables. The
// revision 3/4 directory (1 KiB at PC 0x147000) changes only in its CRCs.
constexpr std::array<std::pair<size_t, size_t>, 5> kNativeSpriteFixedRanges = {{
    {0x4C881, 0x4D62E},
    {0x141438, 0x141438 + 320},
    {0x141578, 0x141578 + 320},
    {0x1416B8, 0x1416B8 + 320},
    {0x147000, 0x147000 + 1024},
}};

absl::StatusOr<std::vector<int>> CheckNativeSpriteOwnership(
    const std::vector<uint8_t>& before, const Rom& after, int* changed_bytes) {
  const auto layout = after.InspectLyraLayout();
  if (!rom::IsNativeSpecialOverworldLayout(layout))
    return absl::DataLossError("Native directory is no longer valid");
  const auto& bytes = after.vector();
  if (bytes.size() != before.size())
    return absl::DataLossError("Native sprite Save resized the ROM");
  auto sprite_owned = [](uint16_t id) {
    return id == 16 || id == 17 || id == 18 || id == 100 ||
           id == rom::kNativeAuthoringMetadataTableId;
  };
  std::vector<int> tables;
  *changed_bytes = 0;
  for (size_t pc = 0; pc < bytes.size(); ++pc) {
    if (bytes[pc] == before[pc])
      continue;
    ++*changed_bytes;
    if (std::any_of(kNativeSpriteFixedRanges.begin(),
                    kNativeSpriteFixedRanges.end(), [pc](const auto& range) {
                      return pc >= range.first && pc < range.second;
                    }))
      continue;
    const auto owner = std::find_if(
        layout.entries.begin(), layout.entries.end(), [pc](const auto& e) {
          return pc >= e.pc_address &&
                 pc < e.pc_address + size_t{e.count} * e.stride;
        });
    if (owner == layout.entries.end() || !sprite_owned(owner->id))
      return absl::DataLossError(absl::StrFormat(
          "Native sprite Save changed unowned byte PC $%06X%s", pc,
          owner == layout.entries.end()
              ? ""
              : absl::StrFormat(" (table %d)", owner->id)));
    if (std::find(tables.begin(), tables.end(), owner->id) == tables.end())
      tables.push_back(owner->id);
  }
  std::sort(tables.begin(), tables.end());
  return tables;
}

}  // namespace

bool IsOracleProjectRomPath(const std::filesystem::path& rom_path) {
  std::error_code ec;
  auto canonical = std::filesystem::weakly_canonical(rom_path, ec);
  if (ec)
    canonical = std::filesystem::absolute(rom_path, ec);
  const auto checkout = canonical.parent_path().parent_path();
  return std::filesystem::exists(checkout / "Oracle_main.asm", ec);
}

std::string OverworldSpriteEditCommandHandler::GetName() const {
  switch (action_) {
    case OverworldSpriteEditAction::kAdd:
      return "overworld-add-sprite";
    case OverworldSpriteEditAction::kMove:
      return "overworld-move-sprite";
    case OverworldSpriteEditAction::kRemove:
      return "overworld-remove-sprite";
  }
  return "overworld-sprite-edit";
}

std::string OverworldSpriteEditCommandHandler::GetDescription() const {
  switch (action_) {
    case OverworldSpriteEditAction::kAdd:
      return "Add a sprite to one overworld phase list (dry-run by default)";
    case OverworldSpriteEditAction::kMove:
      return "Move a sprite in one overworld phase list (dry-run by default)";
    case OverworldSpriteEditAction::kRemove:
      return "Remove a sprite from one overworld phase list (dry-run by "
             "default)";
  }
  return "";
}

std::string OverworldSpriteEditCommandHandler::GetUsage() const {
  const std::string common =
      " [--write] [--allow-project-rom] [--format <json|text>]";
  switch (action_) {
    case OverworldSpriteEditAction::kAdd:
      return "overworld-add-sprite --screen <hex> --phase <0|1|2> --id <hex> "
             "--x <tile> --y <tile> [--replace-index <n>]" +
             common;
    case OverworldSpriteEditAction::kMove:
      return "overworld-move-sprite --screen <hex> --phase <0|1|2> --index <n> "
             "--x <tile> --y <tile> [--expect-id <hex>]" +
             common;
    case OverworldSpriteEditAction::kRemove:
      return "overworld-remove-sprite --screen <hex> --phase <0|1|2> --index "
             "<n> [--expect-id <hex>]" +
             common;
  }
  return "";
}

absl::Status OverworldSpriteEditCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"screen", "phase"}));
  switch (action_) {
    case OverworldSpriteEditAction::kAdd:
      return parser.RequireArgs({"id", "x", "y"});
    case OverworldSpriteEditAction::kMove:
      return parser.RequireArgs({"index", "x", "y"});
    case OverworldSpriteEditAction::kRemove:
      return parser.RequireArgs({"index"});
  }
  return absl::OkStatus();
}

absl::Status OverworldSpriteEditCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = std::filesystem::path(rom->filename());
    invocation_context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status OverworldSpriteEditCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  RETURN_IF_ERROR(ValidateArgs(parser));
  if (!rom || !rom->is_loaded())
    return absl::FailedPreconditionError("ROM not loaded");

  int screen = 0;
  if (!util::ParseHexString(*parser.GetString("screen"), &screen))
    return absl::InvalidArgumentError("--screen must be hex (e.g. 0x40)");
  int phase = -1;
  if (!absl::SimpleAtoi(*parser.GetString("phase"), &phase) || phase < 0 ||
      phase > 2) {
    return absl::InvalidArgumentError("--phase must be 0, 1, or 2");
  }
  const bool do_write = parser.HasFlag("write");

  // Refuse writes to a ROM that lives in an Oracle source checkout before
  // touching anything; agents must edit a copy.
  const auto active_path = invocation_context.active_rom_path.value_or(
      std::filesystem::path(rom->filename()));
  if (do_write && !parser.HasFlag("allow-project-rom") &&
      IsOracleProjectRomPath(active_path)) {
    return absl::PermissionDeniedError(absl::StrFormat(
        "Refusing --write to %s: it is inside an Oracle of Secrets checkout "
        "(Oracle_main.asm next to its Roms/ folder). Copy the ROM (with "
        "Data/dungeons/custom_collision.json) and write the copy, or pass "
        "--allow-project-rom deliberately.",
        active_path.string()));
  }

  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));
  RETURN_IF_ERROR(overworld.resolved_layout().CheckOperation(
      zelda3::OverworldLayoutDomain::kSprites,
      do_write ? zelda3::OverworldLayoutOperation::kMutateModel
               : zelda3::OverworldLayoutOperation::kInspect,
      screen, phase));
  const auto* map = overworld.overworld_map(screen);
  if (map == nullptr)
    return absl::InvalidArgumentError(
        absl::StrFormat("Screen 0x%02X is out of range", screen));
  const int parent = map->parent();
  int cols = 1, rows = 1;
  switch (overworld.overworld_map(parent)->area_size()) {
    case zelda3::AreaSizeEnum::LargeArea:
      cols = rows = 2;
      break;
    case zelda3::AreaSizeEnum::WideArea:
      cols = 2;
      break;
    case zelda3::AreaSizeEnum::TallArea:
      rows = 2;
      break;
    default:
      if (overworld.overworld_map(parent)->is_large_map())
        cols = rows = 2;
      break;
  }

  const bool native =
      rom::IsNativeSpecialOverworldLayout(rom->InspectLyraLayout());
  int pointer_pc = -1;
  std::vector<Entry> before;
  if (native) {
    if (parent >= overworld.materialized_map_count())
      return absl::InvalidArgumentError(
          absl::StrFormat("Area 0x%02X is outside the native %d-map model",
                          parent, overworld.materialized_map_count()));
    const auto& records = *overworld.mutable_sprites(phase);
    for (size_t row : NativeRows(records, parent))
      before.push_back(EntryFromSprite(records[row]));
  } else {
    const auto layout = zelda3::GetOverworldSpriteLayout(*rom);
    if (parent >= layout.counts[phase]) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Phase %d has no sprite list for area 0x%02X in this ROM "
          "(%d entries)",
          phase, parent, layout.counts[phase]));
    }
    pointer_pc = layout.tables[phase] + parent * 2;
    ASSIGN_OR_RETURN(auto old_bytes,
                     zelda3::ReadOverworldSpriteList(*rom, pointer_pc));
    before = Decode(old_bytes);
  }
  auto after = before;
  SpriteOp op;

  auto check_tile = [&](int x, int y) -> absl::Status {
    if (x < 0 || y < 0 || x >= cols * 32 || y >= rows * 32) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Tile (%d,%d) is outside area 0x%02X (%dx%d tiles of 16px)", x, y,
          parent, cols * 32, rows * 32));
    }
    return absl::OkStatus();
  };
  auto check_index = [&](const std::string& flag) -> absl::StatusOr<int> {
    ASSIGN_OR_RETURN(int index, RequiredInt(parser, flag));
    if (index < 0 || index >= static_cast<int>(before.size())) {
      return absl::OutOfRangeError(
          absl::StrFormat("--%s %d out of range (list has %d entries)", flag,
                          index, static_cast<int>(before.size())));
    }
    if (auto expect = parser.GetString("expect-id"); expect.has_value()) {
      int id = 0;
      if (!util::ParseHexString(*expect, &id) || before[index].id != id) {
        return absl::FailedPreconditionError(
            absl::StrFormat("--expect-id %s does not match entry %d (0x%02X)",
                            *expect, index, before[index].id));
      }
    }
    return index;
  };

  formatter.AddField("mode", do_write ? "write" : "dry-run");
  formatter.AddField("action", GetName());
  formatter.AddField("requested_screen", absl::StrFormat("0x%02X", screen));
  formatter.AddField("parent_area", absl::StrFormat("0x%02X", parent));
  formatter.AddField("area_tiles",
                     absl::StrFormat("%dx%d", cols * 32, rows * 32));
  formatter.AddField("phase", phase);
  formatter.AddField("phase_name", overworld::SpritePhaseName(phase));

  switch (action_) {
    case OverworldSpriteEditAction::kAdd: {
      int id = 0;
      if (!util::ParseHexString(*parser.GetString("id"), &id) || id < 0 ||
          id > 0xFF) {
        return absl::InvalidArgumentError("--id must be hex 0x00-0xFF");
      }
      ASSIGN_OR_RETURN(int x, RequiredInt(parser, "x"));
      ASSIGN_OR_RETURN(int y, RequiredInt(parser, "y"));
      RETURN_IF_ERROR(check_tile(x, y));
      if (parser.GetString("replace-index").has_value()) {
        ASSIGN_OR_RETURN(int replace, check_index("replace-index"));
        formatter.AddField("replaced_index", replace);
        after.erase(after.begin() + replace);
        op.index = replace;
      }
      after.push_back({static_cast<uint8_t>(y), static_cast<uint8_t>(x),
                       static_cast<uint8_t>(id)});
      op.x = x;
      op.y = y;
      op.id = id;
      formatter.AddField("sprite_id", absl::StrFormat("0x%02X", id));
      formatter.AddField("sprite_name", zelda3::ResolveSpriteName(id));
      break;
    }
    case OverworldSpriteEditAction::kMove: {
      ASSIGN_OR_RETURN(int index, check_index("index"));
      ASSIGN_OR_RETURN(int x, RequiredInt(parser, "x"));
      ASSIGN_OR_RETURN(int y, RequiredInt(parser, "y"));
      RETURN_IF_ERROR(check_tile(x, y));
      after[index].x = static_cast<uint8_t>((after[index].x & 0xC0) | x);
      after[index].y = static_cast<uint8_t>((after[index].y & 0xC0) | y);
      formatter.AddField("index", index);
      op = {index, x, y, after[index].id};
      break;
    }
    case OverworldSpriteEditAction::kRemove: {
      ASSIGN_OR_RETURN(int index, check_index("index"));
      after.erase(after.begin() + index);
      formatter.AddField("index", index);
      op.index = index;
      break;
    }
  }

  AddEntries(formatter, "list_before", before);
  AddEntries(formatter, "list_after", after);
  // Exact duplicate entries are a common source of wasted list bytes.
  formatter.BeginArray("duplicate_entries_before");
  for (size_t i = 0; i < before.size(); ++i) {
    for (size_t j = 0; j < i; ++j) {
      if (before[i].x == before[j].x && before[i].y == before[j].y &&
          before[i].id == before[j].id) {
        formatter.AddArrayItem(absl::StrFormat("index %d duplicates index %d",
                                               static_cast<int>(i),
                                               static_cast<int>(j)));
        break;
      }
    }
  }
  formatter.EndArray();

  if (native) {
    formatter.AddField("strategy", "native-save");
    // A dry run performs the whole transaction on a private copy so the
    // session ROM never holds an unsaved native edit.
    std::optional<Rom> staged;
    std::optional<zelda3::Overworld> staged_overworld;
    Rom* target = rom;
    zelda3::Overworld* model = &overworld;
    if (!do_write) {
      Rom::LoadOptions rom_options;
      rom_options.load_resource_labels = false;
      staged.emplace();
      RETURN_IF_ERROR(staged->LoadFromData(rom->vector(), rom_options));
      staged_overworld.emplace(&*staged);
      RETURN_IF_ERROR(staged_overworld->Load(&*staged));
      target = &*staged;
      model = &*staged_overworld;
    }
    auto expected = CaptureNativeLists(*model);
    if (after.empty())
      expected[phase].erase(parent);
    else
      expected[phase][parent] = after;

    const auto before_rom = rom->vector();
    ScopedRomTransaction transaction(*target);
    RETURN_IF_ERROR(ApplyNativeSpriteOp(*model, action_, phase, parent, op));
    if (auto status = SaveNativeSprites(*model); !status.ok()) {
      formatter.AddField("save_error", std::string(status.message()));
      return status;
    }
    int changed = 0;
    ASSIGN_OR_RETURN(auto tables,
                     CheckNativeSpriteOwnership(before_rom, *target, &changed));
    formatter.BeginArray("changed_tables");
    for (int id : tables)
      formatter.AddArrayItem(absl::StrFormat("%d", id));
    formatter.EndArray();
    formatter.AddField("changed_bytes", changed);

    zelda3::Overworld reloaded(target);
    RETURN_IF_ERROR(reloaded.Load(target));
    if (CaptureNativeLists(reloaded) != expected)
      return absl::DataLossError(
          "Native sprite lists did not read back as planned");

    if (!do_write) {
      formatter.AddField("write_status", "dry-run");
      return absl::OkStatus();
    }
    Rom::SaveSettings save_settings;
    save_settings.require_backup = true;
    if (auto status = rom->SaveToFile(save_settings); !status.ok()) {
      formatter.AddField("save_error", std::string(status.message()));
      return status;
    }
    transaction.Commit();
    formatter.AddField("save_status", "saved");
    Rom reopened;
    RETURN_IF_ERROR(reopened.LoadFromFile(rom->filename()));
    if (reopened.vector() != rom->vector())
      return absl::DataLossError("Saved native ROM does not read back");
    formatter.AddField("verified_reload", true);
    return absl::OkStatus();
  }

  const auto new_bytes = Encode(after);
  formatter.AddField(
      "pointer_slot",
      absl::StrFormat("PC $%06X / SNES %s", pointer_pc, Snes(pointer_pc)));
  auto plan_or =
      zelda3::PlanOverworldSpriteListEdit(*rom, phase, parent, new_bytes);
  if (!plan_or.ok()) {
    formatter.AddField("strategy", "refused");
    formatter.AddField("error", std::string(plan_or.status().message()));
    return plan_or.status();
  }
  const auto& plan = *plan_or;
  formatter.AddField("strategy",
                     zelda3::OverworldSpriteEditStrategyName(plan.strategy));
  formatter.AddField("old_list",
                     absl::StrFormat("PC $%06X / SNES %s, %d bytes",
                                     plan.old_list_pc, Snes(plan.old_list_pc),
                                     static_cast<int>(plan.old_bytes.size())));
  formatter.AddField("new_list",
                     absl::StrFormat("PC $%06X / SNES %s, %d bytes",
                                     plan.new_list_pc, Snes(plan.new_list_pc),
                                     static_cast<int>(plan.new_bytes.size())));
  formatter.AddField("old_list_shared_with_slots",
                     static_cast<int>(plan.sharers.size()));
  formatter.AddField("region",
                     absl::StrFormat("PC $%06X-$%06X (%d unreferenced bytes)",
                                     plan.region_start, plan.region_end,
                                     plan.unreferenced_bytes));
  formatter.BeginArray("writes");
  const auto& rom_bytes = rom->vector();
  for (const auto& write : plan.writes) {
    const std::vector<uint8_t> old(
        rom_bytes.begin() + write.address,
        rom_bytes.begin() + write.address + write.bytes.size());
    formatter.BeginObject();
    formatter.AddField("pc", absl::StrFormat("$%06X", write.address));
    formatter.AddField("snes", Snes(write.address));
    formatter.AddField("length", static_cast<int>(write.bytes.size()));
    formatter.AddField("old", Hex(old));
    formatter.AddField("new", Hex(write.bytes));
    formatter.EndObject();
  }
  formatter.EndArray();

  if (!do_write || plan.writes.empty()) {
    formatter.AddField("write_status", do_write ? "not-needed" : "dry-run");
    return absl::OkStatus();
  }

  const auto before_rom = rom->vector();
  ScopedRomTransaction transaction(*rom);
  RETURN_IF_ERROR(zelda3::ApplyOverworldSpriteSave(*rom, plan.writes));
  Rom::SaveSettings save_settings;
  save_settings.require_backup = true;
  if (auto status = rom->SaveToFile(save_settings); !status.ok()) {
    formatter.AddField("save_error", std::string(status.message()));
    return status;
  }
  transaction.Commit();
  formatter.AddField("save_status", "saved");

  // Reopen the saved file: the list must read back exactly and no byte
  // outside the planned writes may differ from the pre-edit ROM.
  Rom reopened;
  RETURN_IF_ERROR(reopened.LoadFromFile(rom->filename()));
  ASSIGN_OR_RETURN(auto readback,
                   zelda3::ReadOverworldSpriteList(reopened, pointer_pc));
  if (readback != new_bytes)
    return absl::DataLossError("Saved sprite list does not read back");
  const auto& saved = reopened.vector();
  if (saved.size() != before_rom.size())
    return absl::DataLossError("Saved ROM size changed");
  int changed = 0;
  for (size_t i = 0; i < saved.size(); ++i) {
    if (saved[i] == before_rom[i])
      continue;
    ++changed;
    bool planned = false;
    for (const auto& write : plan.writes) {
      planned |= static_cast<int>(i) >= write.address &&
                 static_cast<int>(i) <
                     write.address + static_cast<int>(write.bytes.size());
    }
    if (!planned)
      return absl::DataLossError(
          absl::StrFormat("Unplanned byte changed at PC $%06X", i));
  }
  formatter.AddField("verified_reload", true);
  formatter.AddField("changed_bytes", changed);
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
