#include "cli/handlers/game/overworld_sprite_edit_commands.h"

#include <algorithm>
#include <string>
#include <system_error>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/handlers/game/overworld_inspect.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "rom/transaction.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld.h"
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

  const auto layout = zelda3::GetOverworldSpriteLayout(*rom);
  if (parent >= layout.counts[phase]) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Phase %d has no sprite list for area 0x%02X in this ROM "
        "(%d entries)",
        phase, parent, layout.counts[phase]));
  }
  const int pointer_pc = layout.tables[phase] + parent * 2;
  ASSIGN_OR_RETURN(auto old_bytes,
                   zelda3::ReadOverworldSpriteList(*rom, pointer_pc));
  const auto before = Decode(old_bytes);
  auto after = before;

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
      }
      after.push_back({static_cast<uint8_t>(y), static_cast<uint8_t>(x),
                       static_cast<uint8_t>(id)});
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
      break;
    }
    case OverworldSpriteEditAction::kRemove: {
      ASSIGN_OR_RETURN(int index, check_index("index"));
      after.erase(after.begin() + index);
      formatter.AddField("index", index);
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
