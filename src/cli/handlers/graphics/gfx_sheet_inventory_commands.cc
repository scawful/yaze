#include "cli/handlers/graphics/gfx_sheet_inventory_commands.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "core/project.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/oracle_rom_safety_preflight.h"
#include "zelda3/gfx_sheet_inventory.h"

namespace yaze {
namespace cli {
namespace {

absl::StatusOr<std::set<uint16_t>> ParseSheetList(const std::string& value,
                                                  const char* flag) {
  std::set<uint16_t> sheets;
  for (absl::string_view part : absl::StrSplit(value, ',')) {
    part = absl::StripAsciiWhitespace(part);
    if (part.empty()) {
      continue;
    }
    absl::string_view digits = part;
    int base = 10;
    if (absl::ConsumePrefix(&digits, "0x") ||
        absl::ConsumePrefix(&digits, "0X") ||
        absl::ConsumePrefix(&digits, "$")) {
      base = 16;
    }
    uint32_t sheet = 0;
    const bool ok = base == 16 ? absl::SimpleHexAtoi(digits, &sheet)
                               : absl::SimpleAtoi(digits, &sheet);
    if (!ok || sheet >= zelda3::kGfxSheetCount) {
      return absl::InvalidArgumentError(
          absl::StrFormat("--%s: '%s' is not a sheet id (0-%zu)", flag,
                          std::string(part), zelda3::kGfxSheetCount - 1));
    }
    sheets.insert(static_cast<uint16_t>(sheet));
  }
  return sheets;
}

}  // namespace

absl::Status GfxSheetInventoryCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  zelda3::GfxSheetInventoryOptions options =
      core::BuildGfxSheetInventoryOptions(project_);
  if (auto reserved = parser.GetString("reserved"); reserved.has_value()) {
    ASSIGN_OR_RETURN(auto sheets, ParseSheetList(*reserved, "reserved"));
    options.reserved_sheets.insert(sheets.begin(), sheets.end());
  }
  if (auto flagged = parser.GetString("flagged"); flagged.has_value()) {
    ASSIGN_OR_RETURN(auto sheets, ParseSheetList(*flagged, "flagged"));
    options.flagged_sheets.insert(sheets.begin(), sheets.end());
  }
  if (auto csv_path = parser.GetString("labels-csv"); csv_path.has_value()) {
    std::ifstream file(*csv_path);
    if (!file) {
      return absl::NotFoundError("Cannot read --labels-csv " + *csv_path);
    }
    std::stringstream text;
    text << file.rdbuf();
    options.sprite_value_labels = zelda3::ParseSpritesetLabelCsv(text.str());
  }

  const auto areas = zelda3::CollectOverworldAreaGfx(*rom);
  const auto rooms = zelda3::CollectRoomGfx(*rom);
  ASSIGN_OR_RETURN(const zelda3::GfxSheetInventory inventory,
                   zelda3::BuildGfxSheetInventory(*rom, areas, rooms, options));

  nlohmann::json document = nlohmann::json::object();
  document["rom"] = rom->filename();
  if (auto sha = zelda3::ComputeSha256(rom->filename()); sha.ok()) {
    document["rom_sha256"] = *sha;
  }
  nlohmann::json reserved_rules = nlohmann::json::object();
  for (uint16_t sheet : options.reserved_sheets) {
    reserved_rules[absl::StrFormat("0x%02X", sheet)] =
        "reserved by project config";
  }
  document["rules"] = {
      {"sprite_sheet", "spriteset value + 0x73 = gfx index"},
      {"dungeon_spriteset", "room header spriteset + 0x40 = spriteset index"},
      {"reserved", reserved_rules},
  };
  document.update(zelda3::GfxSheetInventoryToJson(inventory));

  if (auto out_path = parser.GetString("out"); out_path.has_value()) {
    std::ofstream out(*out_path, std::ios::trunc);
    if (!out) {
      return absl::PermissionDeniedError("Cannot write --out " + *out_path);
    }
    out << document.dump(1) << "\n";
    if (!out) {
      return absl::DataLossError("Failed writing --out " + *out_path);
    }
  }

  if (formatter.IsJson()) {
    for (const auto& [key, value] : document.items()) {
      formatter.AddRawJsonField(key, value.dump());
    }
    return absl::OkStatus();
  }

  // Text: one line per sprite sheet that has free space, then totals.
  int free_blocks = 0;
  int unreferenced = 0;
  for (const auto& sheet : inventory.sheets) {
    const auto free = sheet.FreeBlocks();
    free_blocks += static_cast<int>(free.size());
    unreferenced += sheet.unreferenced ? 1 : 0;
    if (!sheet.sprite_value.has_value() || free.empty()) {
      continue;
    }
    formatter.AddField(
        absl::StrFormat("sheet 0x%02X (value 0x%02X)", sheet.gfx,
                        *sheet.sprite_value),
        absl::StrFormat("%zu free 16x16 [%s]%s%s; spritesets %zu, areas %zu, "
                        "rooms %zu",
                        free.size(), absl::StrJoin(free, ","),
                        sheet.flagged ? " FLAGGED" : "",
                        sheet.labels.empty()
                            ? ""
                            : (" " + absl::StrJoin(sheet.labels, "; ")).c_str(),
                        sheet.usage.spritesets.size(),
                        sheet.usage.ow_areas_sprite.size(),
                        sheet.usage.rooms_sprite.size()));
  }
  formatter.AddField("sheets", static_cast<int>(inventory.sheets.size()));
  formatter.AddField("overworld_areas", static_cast<int>(areas.size()));
  formatter.AddField("rooms", static_cast<int>(rooms.size()));
  formatter.AddField("free_16x16_blocks", free_blocks);
  formatter.AddField("unreferenced_sheets", unreferenced);
  formatter.AddField("reserved_sheets",
                     static_cast<int>(options.reserved_sheets.size()));
  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
