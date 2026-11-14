#include "zelda3/overworld/overworld_grouping_preflight.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"

namespace yaze::zelda3 {
namespace {

constexpr int kGridColumns = 8;
constexpr int kGridRows = 8;
constexpr int kGridCells = kGridColumns * kGridRows;  // one world page

const GroupingMapSnapshot* FindSnapshot(
    const std::vector<GroupingMapSnapshot>& current, int map_id) {
  for (const auto& row : current) {
    if (row.map_id == map_id)
      return &row;
  }
  return nullptr;
}

}  // namespace

const char* AreaSizeEnumName(AreaSizeEnum size) {
  switch (size) {
    case AreaSizeEnum::SmallArea:
      return "small";
    case AreaSizeEnum::LargeArea:
      return "large";
    case AreaSizeEnum::WideArea:
      return "wide";
    case AreaSizeEnum::TallArea:
      return "tall";
  }
  return "unknown";
}

absl::StatusOr<AreaSizeEnum> ParseAreaSizeName(absl::string_view name) {
  const std::string lower = absl::AsciiStrToLower(name);
  if (lower == "small")
    return AreaSizeEnum::SmallArea;
  if (lower == "large")
    return AreaSizeEnum::LargeArea;
  if (lower == "wide")
    return AreaSizeEnum::WideArea;
  if (lower == "tall")
    return AreaSizeEnum::TallArea;
  return absl::InvalidArgumentError(
      "Area size must be small, large, wide, or tall");
}

absl::StatusOr<std::vector<int>> ProposedGroupingFootprint(int parent,
                                                           AreaSizeEnum size) {
  if (parent < 0 || parent >= 0xC0)
    return absl::InvalidArgumentError("Parent map must be $00-$BF");
  if (size == AreaSizeEnum::SmallArea)
    return absl::InvalidArgumentError(
        "Grouping preflight requires large, wide, or tall");

  const int width =
      size == AreaSizeEnum::LargeArea || size == AreaSizeEnum::WideArea ? 2 : 1;
  const int height =
      size == AreaSizeEnum::LargeArea || size == AreaSizeEnum::TallArea ? 2 : 1;
  const int page = parent / kGridCells;
  const int local = parent % kGridCells;
  if (local % kGridColumns + width > kGridColumns ||
      local / kGridColumns + height > kGridRows)
    return absl::InvalidArgumentError(
        "Proposed footprint crosses an overworld page boundary");

  std::vector<int> maps;
  maps.reserve(static_cast<size_t>(width * height));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      maps.push_back(page * kGridCells + local + y * kGridColumns + x);
    }
  }
  return maps;
}

absl::StatusOr<GroupingPreflightReport> BuildGroupingPreflightReport(
    const GroupingPreflightRequest& request) {
  ASSIGN_OR_RETURN(auto footprint,
                   ProposedGroupingFootprint(request.parent, request.size));

  GroupingPreflightReport report;
  report.parent = request.parent;
  report.proposed_size = request.size;
  report.proposed_footprint = footprint;
  report.requested_members = request.members;
  report.contacts = request.contacts;

  std::vector<int> requested = request.members;
  std::sort(requested.begin(), requested.end());
  if (std::adjacent_find(requested.begin(), requested.end()) !=
      requested.end()) {
    report.blocking.push_back(
        absl::StrFormat("Requested members {%s} contain duplicates",
                        absl::StrJoin(request.members, ",")));
  }
  auto expected = footprint;
  std::sort(expected.begin(), expected.end());
  // Exact membership: after rejecting duplicates, the unique sorted list must
  // equal the proposed footprint. Do not unique-erase before comparing when
  // reporting the mismatch list — duplicates already blocked above.
  std::vector<int> unique_requested = requested;
  unique_requested.erase(
      std::unique(unique_requested.begin(), unique_requested.end()),
      unique_requested.end());
  if (unique_requested != expected) {
    report.blocking.push_back(absl::StrFormat(
        "Requested members {%s} do not match %s footprint {%s}",
        absl::StrJoin(unique_requested, ","), AreaSizeEnumName(request.size),
        absl::StrJoin(expected, ",")));
  }

  const std::set<int> proposed(footprint.begin(), footprint.end());
  std::set<int> touched_parents;
  std::map<std::string, std::map<int, std::vector<int>>> field_values;
  for (int map_id : footprint) {
    const auto* snap = FindSnapshot(request.current, map_id);
    if (snap == nullptr) {
      report.blocking.push_back(
          absl::StrFormat("Missing current snapshot for map $%02X", map_id));
      continue;
    }

    GroupingMemberReport member;
    member.map_id = map_id;
    member.current_parent = snap->parent;
    member.current_size = snap->size;
    member.already_in_proposed = snap->parent == request.parent;
    member.area_graphics = snap->area_graphics;
    member.area_palette = snap->area_palette;
    member.main_palette = snap->main_palette;
    member.animated_gfx = snap->animated_gfx;
    member.subscreen_overlay = snap->subscreen_overlay;

    touched_parents.insert(snap->parent);
    if (snap->size != AreaSizeEnum::SmallArea)
      touched_parents.insert(map_id);

    // Foreign = owned by a different multi-area parent (not self, not proposed).
    if (snap->parent != map_id && snap->parent != request.parent) {
      member.foreign_parent = true;
      report.blocking.push_back(absl::StrFormat(
          "Map $%02X belongs to foreign parent $%02X", map_id, snap->parent));
    }
    report.members.push_back(member);

    field_values["area_graphics"][snap->area_graphics].push_back(map_id);
    field_values["area_palette"][snap->area_palette].push_back(map_id);
    field_values["main_palette"][snap->main_palette].push_back(map_id);
    field_values["animated_gfx"][snap->animated_gfx].push_back(map_id);
    field_values["subscreen_overlay"][snap->subscreen_overlay].push_back(
        map_id);
  }

  // Any current multi-area group that intersects the proposal must be fully
  // absorbed. Partial regroup that strands siblings is out of scope.
  for (int parent : touched_parents) {
    const auto* parent_snap = FindSnapshot(request.current, parent);
    if (parent_snap == nullptr || parent_snap->size == AreaSizeEnum::SmallArea)
      continue;
    auto current_or = ProposedGroupingFootprint(parent, parent_snap->size);
    if (!current_or.ok()) {
      report.blocking.push_back(absl::StrFormat(
          "Current multi-area parent $%02X has an invalid %s footprint", parent,
          AreaSizeEnumName(parent_snap->size)));
      continue;
    }
    for (int map_id : *current_or) {
      if (FindSnapshot(request.current, map_id) == nullptr) {
        report.blocking.push_back(absl::StrFormat(
            "Missing current snapshot for map $%02X (sibling of multi-area "
            "parent $%02X)",
            map_id, parent));
        continue;
      }
      if (!proposed.contains(map_id)) {
        report.blocking.push_back(absl::StrFormat(
            "Map $%02X would be orphaned from current %s parent $%02X; "
            "partial regroup is unsupported",
            map_id, AreaSizeEnumName(parent_snap->size), parent));
      }
    }
  }

  for (const auto& [field, by_value] : field_values) {
    if (by_value.size() <= 1)
      continue;
    GroupingProfileMismatch mismatch;
    mismatch.field = field;
    for (const auto& [value, maps] : by_value) {
      mismatch.values.push_back(value);
      mismatch.maps.insert(mismatch.maps.end(), maps.begin(), maps.end());
    }
    report.profile_mismatches.push_back(mismatch);
    report.warnings.push_back(
        absl::StrFormat("%s differs across members (not auto-fixed)", field));
  }

  if (!report.contacts.empty())
    report.warnings.push_back(report.contact_disclaimer);

  report.ok = report.blocking.empty();
  return report;
}

std::string GroupingPreflightToJson(const GroupingPreflightReport& report) {
  nlohmann::ordered_json json;
  json["schema"] = "yaze.overworld-grouping-preflight.v1";
  json["ok"] = report.ok;
  json["parent"] = report.parent;
  json["proposed_size"] = AreaSizeEnumName(report.proposed_size);
  json["proposed_footprint"] = report.proposed_footprint;
  json["requested_members"] = report.requested_members;
  json["contact_disclaimer"] = report.contact_disclaimer;
  json["blocking"] = report.blocking;
  json["warnings"] = report.warnings;

  nlohmann::ordered_json members = nlohmann::ordered_json::array();
  for (const auto& member : report.members) {
    members.push_back({
        {"map_id", member.map_id},
        {"current_parent", member.current_parent},
        {"current_size", AreaSizeEnumName(member.current_size)},
        {"already_in_proposed", member.already_in_proposed},
        {"foreign_parent", member.foreign_parent},
        {"area_graphics", member.area_graphics},
        {"area_palette", member.area_palette},
        {"main_palette", member.main_palette},
        {"animated_gfx", member.animated_gfx},
        {"subscreen_overlay", member.subscreen_overlay},
    });
  }
  json["members"] = members;

  nlohmann::ordered_json mismatches = nlohmann::ordered_json::array();
  for (const auto& mismatch : report.profile_mismatches) {
    mismatches.push_back({{"field", mismatch.field},
                          {"maps", mismatch.maps},
                          {"values", mismatch.values}});
  }
  json["profile_mismatches"] = mismatches;

  nlohmann::ordered_json contacts = nlohmann::ordered_json::array();
  for (const auto& contact : report.contacts) {
    contacts.push_back({{"id", contact.id},
                        {"label", contact.label},
                        {"collision_proof", false}});
  }
  json["contacts"] = contacts;
  return json.dump(2);
}

std::string FormatGroupingPreflightText(const GroupingPreflightReport& report) {
  std::ostringstream out;
  out << "Overworld grouping preflight\n";
  out << absl::StrFormat("  ok: %s\n", report.ok ? "true" : "false");
  out << absl::StrFormat("  parent: $%02X  size: %s\n", report.parent,
                         AreaSizeEnumName(report.proposed_size));
  out << "  proposed footprint:";
  for (int map_id : report.proposed_footprint)
    out << absl::StrFormat(" $%02X", map_id);
  out << "\n";
  out << "  members:\n";
  for (const auto& member : report.members) {
    out << absl::StrFormat(
        "    $%02X  current_parent=$%02X  current_size=%s"
        "  gfx=%02X pal=%02X main=%02X anim=%02X overlay=%04X%s%s\n",
        member.map_id, member.current_parent,
        AreaSizeEnumName(member.current_size), member.area_graphics,
        member.area_palette, member.main_palette, member.animated_gfx,
        member.subscreen_overlay,
        member.already_in_proposed ? "  in_proposed" : "",
        member.foreign_parent ? "  FOREIGN" : "");
  }
  if (!report.profile_mismatches.empty()) {
    out << "  profile mismatches:\n";
    for (const auto& mismatch : report.profile_mismatches) {
      out << absl::StrFormat("    %s maps={%s} values={%s}\n", mismatch.field,
                             absl::StrJoin(mismatch.maps, ","),
                             absl::StrJoin(mismatch.values, ","));
    }
  }
  if (!report.contacts.empty()) {
    out << "  contacts (design labels, not collision proof):\n";
    for (const auto& contact : report.contacts)
      out << absl::StrFormat("    %s: %s\n", contact.id, contact.label);
  }
  if (!report.blocking.empty()) {
    out << "  blocking:\n";
    for (const auto& item : report.blocking)
      out << "    - " << item << "\n";
  }
  if (!report.warnings.empty()) {
    out << "  warnings:\n";
    for (const auto& item : report.warnings)
      out << "    - " << item << "\n";
  }
  return out.str();
}

}  // namespace yaze::zelda3
