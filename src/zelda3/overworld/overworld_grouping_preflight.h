#ifndef YAZE_ZELDA3_OVERWORLD_OVERWORLD_GROUPING_PREFLIGHT_H_
#define YAZE_ZELDA3_OVERWORLD_OVERWORLD_GROUPING_PREFLIGHT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::zelda3 {

/// One map's current native grouping + profile fields used for compatibility.
struct GroupingMapSnapshot {
  int map_id = -1;
  int parent = -1;
  AreaSizeEnum size = AreaSizeEnum::SmallArea;
  uint8_t area_graphics = 0;
  uint8_t area_palette = 0;
  uint8_t main_palette = 0;
  uint8_t animated_gfx = 0;
  uint16_t subscreen_overlay = 0x00FF;
};

struct GroupingContactLabel {
  std::string id;     // Stable key, e.g. "beach_99_98".
  std::string label;  // Human text; never treated as collision proof.
};

struct GroupingPreflightRequest {
  int parent = -1;
  AreaSizeEnum size = AreaSizeEnum::SmallArea;
  std::vector<int> members;  // Must match the size footprint exactly.
  std::vector<GroupingContactLabel> contacts;
  std::vector<GroupingMapSnapshot> current;  // Known maps; missing → error.
};

struct GroupingMemberReport {
  int map_id = -1;
  int current_parent = -1;
  AreaSizeEnum current_size = AreaSizeEnum::SmallArea;
  bool already_in_proposed = false;
  bool foreign_parent = false;
  uint8_t area_graphics = 0;
  uint8_t area_palette = 0;
  uint8_t main_palette = 0;
  uint8_t animated_gfx = 0;
  uint16_t subscreen_overlay = 0x00FF;
};

struct GroupingProfileMismatch {
  std::string field;
  std::vector<int> maps;
  std::vector<int> values;
};

struct GroupingPreflightReport {
  bool ok = false;
  int parent = -1;
  AreaSizeEnum proposed_size = AreaSizeEnum::SmallArea;
  std::vector<int> proposed_footprint;
  std::vector<int> requested_members;
  std::vector<GroupingMemberReport> members;
  std::vector<GroupingProfileMismatch> profile_mismatches;
  std::vector<GroupingContactLabel> contacts;
  std::vector<std::string> blocking;  // Malformed / foreign-parent.
  std::vector<std::string> warnings;  // Profile mismatches; contacts note.
  std::string contact_disclaimer =
      "Design contact labels are not collision proof.";
};

/// Expected member maps for parent + size using the 8-wide overworld grid.
absl::StatusOr<std::vector<int>> ProposedGroupingFootprint(int parent,
                                                           AreaSizeEnum size);

/// Read-only preflight: footprint match, foreign-parent refusal, profile
/// mismatches. Does not write geometry or treat contacts as collision proof.
absl::StatusOr<GroupingPreflightReport> BuildGroupingPreflightReport(
    const GroupingPreflightRequest& request);

const char* AreaSizeEnumName(AreaSizeEnum size);
absl::StatusOr<AreaSizeEnum> ParseAreaSizeName(absl::string_view name);

std::string GroupingPreflightToJson(const GroupingPreflightReport& report);
std::string FormatGroupingPreflightText(const GroupingPreflightReport& report);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_OVERWORLD_GROUPING_PREFLIGHT_H_
