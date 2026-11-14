#include "cli/handlers/tools/overworld_grouping_preflight_commands.h"

#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "nlohmann/json.hpp"
#include "rom/lyra_layout_descriptor.h"
#include "util/hex.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_grouping_preflight.h"

namespace yaze::cli {
namespace {

absl::StatusOr<int> ParseMapId(absl::string_view text) {
  int value = 0;
  if (!util::ParseHexString(text, &value) || value < 0 || value > 0xBF)
    return absl::InvalidArgumentError(
        absl::StrFormat("Map id must be hex $00-$BF (got '%s')", text));
  return value;
}

absl::StatusOr<std::vector<int>> ParseMembers(absl::string_view text) {
  std::vector<int> members;
  for (absl::string_view part : absl::StrSplit(text, ',', absl::SkipEmpty())) {
    ASSIGN_OR_RETURN(int map_id, ParseMapId(absl::StripAsciiWhitespace(part)));
    members.push_back(map_id);
  }
  if (members.empty())
    return absl::InvalidArgumentError("--members must list at least one map");
  return members;
}

absl::StatusOr<zelda3::GroupingContactLabel> ParseContact(
    absl::string_view text) {
  const auto pos = text.find(':');
  if (pos == absl::string_view::npos || pos == 0 || pos + 1 >= text.size())
    return absl::InvalidArgumentError(
        "--contact expects id:label (design label only)");
  zelda3::GroupingContactLabel contact;
  contact.id = std::string(text.substr(0, pos));
  contact.label = std::string(text.substr(pos + 1));
  return contact;
}

}  // namespace

absl::Status OverworldGroupingPreflightCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (rom == nullptr || !rom->is_loaded())
    return absl::FailedPreconditionError("ROM not loaded");

  ASSIGN_OR_RETURN(const int parent, ParseMapId(*parser.GetString("parent")));
  ASSIGN_OR_RETURN(const auto size,
                   zelda3::ParseAreaSizeName(*parser.GetString("size")));
  ASSIGN_OR_RETURN(const auto members,
                   ParseMembers(*parser.GetString("members")));

  std::vector<zelda3::GroupingContactLabel> contacts;
  if (auto contact = parser.GetString("contact"); contact.has_value()) {
    for (absl::string_view part :
         absl::StrSplit(*contact, ';', absl::SkipEmpty())) {
      ASSIGN_OR_RETURN(auto row,
                       ParseContact(absl::StripAsciiWhitespace(part)));
      contacts.push_back(std::move(row));
    }
  }

  if (parser.HasFlag("experimental-page-tile16"))
    rom->EnableExperimentalPageTile16();

  const auto layout = rom->InspectLyraLayout();
  if (rom::IsNativeSpecialOverworldLayout(layout) && layout.revision == 5 &&
      !parser.HasFlag("experimental-page-tile16")) {
    return absl::FailedPreconditionError(
        "Revision5 native ROM requires --experimental-page-tile16");
  }

  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));

  zelda3::GroupingPreflightRequest request;
  request.parent = parent;
  request.size = size;
  request.members = members;
  request.contacts = std::move(contacts);

  ASSIGN_OR_RETURN(const auto footprint,
                   zelda3::ProposedGroupingFootprint(parent, size));
  std::set<int> needed(footprint.begin(), footprint.end());
  needed.insert(members.begin(), members.end());
  for (int map_id : needed) {
    if (map_id < 0 || map_id >= overworld.materialized_map_count())
      return absl::InvalidArgumentError(
          absl::StrFormat("Map $%02X is outside this ROM's overworld", map_id));
    const auto* map = overworld.overworld_map(map_id);
    if (map == nullptr)
      return absl::FailedPreconditionError(
          absl::StrFormat("Overworld map $%02X is unavailable", map_id));
    zelda3::GroupingMapSnapshot snap;
    snap.map_id = map_id;
    snap.parent = map->parent();
    snap.size = map->area_size();
    snap.area_graphics = map->area_graphics();
    snap.area_palette = map->area_palette();
    snap.main_palette = map->main_palette();
    snap.animated_gfx = map->animated_gfx();
    snap.subscreen_overlay = map->subscreen_overlay();
    request.current.push_back(snap);
  }

  ASSIGN_OR_RETURN(const auto report,
                   zelda3::BuildGroupingPreflightReport(request));

  if (formatter.IsJson()) {
    const auto json =
        nlohmann::ordered_json::parse(zelda3::GroupingPreflightToJson(report));
    for (auto it = json.begin(); it != json.end(); ++it)
      formatter.AddRawJsonField(it.key(), it.value().dump());
  } else {
    formatter.AddRawText(zelda3::FormatGroupingPreflightText(report));
  }
  return absl::OkStatus();
}

}  // namespace yaze::cli
