#include "cli/handlers/game/dungeon_spatial_commands.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cli/util/hex_util.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"
#include "zelda3/dungeon/door_position.h"
#include "zelda3/dungeon/game_tilemap_comparison.h"
#include "zelda3/dungeon/object_templates.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layout.h"
#include "zelda3/dungeon/room_spatial.h"
#include "zelda3/game_data.h"

namespace yaze::cli::handlers {
namespace {
using Json = nlohmann::json;

absl::StatusOr<int> Decimal(std::string_view text) {
  int value = 0;
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || parsed.ec != std::errc{} ||
      parsed.ptr != text.data() + text.size()) {
    return absl::InvalidArgumentError(
        "Expected a decimal Tile8 coordinate or index");
  }
  return value;
}

absl::StatusOr<int> RoomId(const resources::ArgumentParser& parser) {
  int room = -1;
  if (!util::ParseHexString(parser.GetString("room").value_or(""), &room) ||
      room < 0 || room >= zelda3::kNumberOfRooms) {
    return absl::InvalidArgumentError(
        "Room must be hexadecimal in 0x000..0x127");
  }
  return room;
}

std::string RomContentHash(const Rom& rom) {
  return core::ComputeSourceArtifactSha256(
      std::string_view(reinterpret_cast<const char*>(rom.data()), rom.size()));
}

Json Bounds(const zelda3::RoomSpatialBounds& bounds) {
  return {{"x", bounds.x},
          {"y", bounds.y},
          {"width", bounds.width},
          {"height", bounds.height}};
}

Json Cell(const zelda3::RoomSpatialCell& cell) {
  return {{"x", cell.x},
          {"y", cell.y},
          {"plane", cell.layer == 0 ? "upper" : "lower"},
          {"word", cell.word}};
}

Json WallJoins(const std::vector<zelda3::RoomWallJoinIssue>& issues,
               const std::string& coverage, size_t unassessed_objects) {
  Json result = {
      {"coverage", coverage},
      {"complete_room_qualification", false},
      {"unassessed_objects", unassessed_objects},
      {"status", !issues.empty()      ? "needs_reconnection"
                 : unassessed_objects ? "unassessed_native_walls"
                                      : "no_known_endpoint_loss"},
      {"scope",
       "Known native retaining-wall endpoint relations in architectural "
       "painter order; floor, water and decoration overpaints are excluded. "
       "This does not qualify other joins, final pixels or gameplay."},
      {"issues", Json::array()}};
  for (const auto& issue : issues) {
    Json value = {{"kind", issue.kind},
                  {"face_index", issue.face_index},
                  {"face_is_layout", issue.face_is_layout},
                  {"face_id", issue.face_id},
                  {"corner_index", issue.corner_index},
                  {"corner_is_layout", issue.corner_is_layout},
                  {"corner_id", issue.corner_id},
                  {"endpoint_bounds", Bounds(issue.endpoint_bounds)},
                  {"upper_cells", issue.upper_cells},
                  {"lower_cells", issue.lower_cells},
                  {"diagnostic", issue.diagnostic},
                  {"lost_cells", Json::array()},
                  {"overwriters", Json::array()}};
    for (const auto& cell : issue.lost_cells)
      value["lost_cells"].push_back(Cell(cell));
    for (const auto& writer : issue.overwriters) {
      value["overwriters"].push_back({{"object_index", writer.object_index},
                                      {"is_layout", writer.is_layout},
                                      {"object_id", writer.object_id},
                                      {"upper_cells", writer.upper_cells},
                                      {"lower_cells", writer.lower_cells}});
    }
    result["issues"].push_back(std::move(value));
  }
  return result;
}

Json Footprint(const zelda3::RoomObjectFootprint& object, bool include_cells) {
  size_t upper = 0;
  for (const auto& cell : object.cells)
    upper += cell.layer == 0;
  Json result = {
      {"object_index", object.object_index},
      {"object_id", object.object_id},
      {"size", object.size},
      {"anchor", {{"x", object.anchor_x}, {"y", object.anchor_y}}},
      {"draw_list", object.list_index},
      {"layout_object", object.is_layout},
      {"bounds", Bounds(object.bounds)},
      {"measured", object.measured},
      {"full_bounds_measured", object.full_bounds_measured},
      {"measurement_source", object.measurement_source},
      {"outside_room", object.outside_room},
      {"effective_yaze_bg",
       zelda3::EffectiveBgLayerLabel(object.layers.effective_bg_layer)},
      {"yaze_routing",
       zelda3::ObjectRenderRoutingLabel(object.layers.render_routing)},
      {"custom_override", object.layers.custom_override_active},
      {"upper_write_cells", upper},
      {"lower_write_cells", object.cells.size() - upper},
      {"diagnostics", object.diagnostics}};
  if (include_cells) {
    result["cells"] = Json::array();
    for (const auto& cell : object.cells)
      result["cells"].push_back(Cell(cell));
  }
  return result;
}

absl::StatusOr<zelda3::ObjectTemplatePlanOptions> PlanOptions(
    const resources::ArgumentParser& parser) {
  zelda3::ObjectTemplatePlanOptions options;
  if (const auto value = parser.GetString("clearance")) {
    const std::vector<std::string> parts = absl::StrSplit(*value, ',');
    if (parts.size() != 4 && parts.size() != 5) {
      return absl::InvalidArgumentError(
          "Clearance is x,y,width,height[,upper|lower|both]");
    }
    zelda3::ObjectTemplateClearance clearance;
    ASSIGN_OR_RETURN(clearance.bounds.x, Decimal(parts[0]));
    ASSIGN_OR_RETURN(clearance.bounds.y, Decimal(parts[1]));
    ASSIGN_OR_RETURN(clearance.bounds.width, Decimal(parts[2]));
    ASSIGN_OR_RETURN(clearance.bounds.height, Decimal(parts[3]));
    if (parts.size() == 5 && parts[4] != "both") {
      if (parts[4] != "upper" && parts[4] != "lower") {
        return absl::InvalidArgumentError(
            "Clearance plane must be upper, lower or both");
      }
      clearance.plane = parts[4] == "upper" ? 0 : 1;
    }
    options.clearance.push_back(clearance);
  }
  if (const auto value = parser.GetString("allow-existing")) {
    for (const auto part : absl::StrSplit(*value, ',')) {
      ASSIGN_OR_RETURN(const int index,
                       Decimal(std::string_view(part.data(), part.size())));
      if (index < 0)
        return absl::InvalidArgumentError(
            "Existing indices must be nonnegative");
      options.allowed_existing_indices.push_back(index);
    }
  }
  if (const auto value = parser.GetString("allow-pairs")) {
    for (const auto part : absl::StrSplit(*value, ',')) {
      const std::vector<std::string> pair = absl::StrSplit(part, ':');
      if (pair.size() != 2)
        return absl::InvalidArgumentError(
            "An allowed pair is template_index:existing_index");
      ASSIGN_OR_RETURN(const int first, Decimal(pair[0]));
      ASSIGN_OR_RETURN(const int second, Decimal(pair[1]));
      if (first < 0 || second < 0)
        return absl::InvalidArgumentError("Pair indices must be nonnegative");
      options.allowed_existing_pairs.emplace_back(first, second);
    }
  }
  return options;
}

const char* ConflictName(zelda3::ObjectTemplatePlanConflictKind kind) {
  using Kind = zelda3::ObjectTemplatePlanConflictKind;
  switch (kind) {
    case Kind::kInvalidTemplate:
      return "invalid_template";
    case Kind::kInvalidPlacement:
      return "invalid_placement";
    case Kind::kUnmeasuredFootprint:
      return "unmeasured_footprint";
    case Kind::kOutsideRoom:
      return "outside_room";
    case Kind::kClearance:
      return "clearance";
    case Kind::kExistingOverlap:
      return "existing_overlap";
    case Kind::kTemplateOverlap:
      return "ordered_template_overlap";
    case Kind::kWallJoinLoss:
      return "wall_join_loss";
    case Kind::kAssemblyConnection:
      return "assembly_connection";
  }
  return "unknown";
}

// Use the maintained Proposal Preview payload rather than introducing another
// review renderer. These are diagnostic annotations over the loaded room, not
// a rendered candidate or a promise that its terrain is traversable.
Json TemplateProposal(const zelda3::ObjectTemplatePlacementPlan& plan,
                      const zelda3::ObjectTemplatePlanOptions& options,
                      int room_id, const std::string& name) {
  Json placements = Json::array();
  Json reserved = Json::array();
  Json issues = Json::array();
  auto rectangle = [room_id](const zelda3::RoomSpatialBounds& bounds,
                             const std::string& label,
                             const std::string& detail) -> Json {
    // Invalid user clearances also receive a blocked-plan preview. Widen
    // before adding endpoints: their integer fields may span the int range.
    const auto clip = [](int64_t value) {
      return static_cast<int>(std::clamp<int64_t>(value, 0, 63));
    };
    const int x = clip(bounds.x);
    const int y = clip(bounds.y);
    const int right =
        std::max(x, clip(static_cast<int64_t>(bounds.x) + bounds.width - 1));
    const int bottom =
        std::max(y, clip(static_cast<int64_t>(bounds.y) + bounds.height - 1));
    return {{"type", "rect"},
            {"room", room_id},
            {"tiles", {x, y, right, bottom}},
            {"label", label},
            {"detail", detail}};
  };
  for (const auto& footprint : plan.footprints) {
    if (!footprint.measured || footprint.bounds.width <= 0 ||
        footprint.bounds.height <= 0)
      continue;
    placements.push_back(rectangle(
        footprint.bounds,
        absl::StrFormat("%d: $%03X", footprint.object_index,
                        footprint.object_id),
        "Measured proposed-object bounds, clipped to this room. Draw-list " +
            std::to_string(footprint.list_index) +
            "; not a collision or gameplay floor."));
  }
  for (size_t i = 0; i < options.clearance.size(); ++i) {
    const auto& clearance = options.clearance[i];
    reserved.push_back(rectangle(
        clearance.bounds, absl::StrFormat("Reserved %d", i),
        "Protected from new object writes; existing terrain and player "
        "approach are not qualified. Plane: " +
            std::string(!clearance.plane        ? "both"
                        : *clearance.plane == 0 ? "upper"
                                                : "lower")));
  }
  constexpr size_t kMaxPreviewConflicts = 64;
  size_t blocking_count = 0;
  for (const auto& conflict : plan.conflicts) {
    if (!conflict.blocking)
      continue;
    if (++blocking_count > kMaxPreviewConflicts)
      continue;
    std::string detail = conflict.message;
    if (conflict.existing_index)
      detail +=
          absl::StrFormat(" Existing index %d.", *conflict.existing_index);
    if (conflict.expected_region) {
      issues.push_back(rectangle(*conflict.expected_region,
                                 ConflictName(conflict.kind), detail));
    } else if (!conflict.cells.empty()) {
      int x0 = 63, y0 = 63, x1 = 0, y1 = 0;
      for (const auto& cell : conflict.cells) {
        x0 = std::min(x0, cell.x);
        y0 = std::min(y0, cell.y);
        x1 = std::max(x1, cell.x);
        y1 = std::max(y1, cell.y);
      }
      detail += absl::StrFormat(
          " %d physical-plane cells; rectangle encloses at most the first "
          "64 reported cells, not a filled collision region.",
          conflict.cell_count);
      issues.push_back(rectangle({x0, y0, x1 - x0 + 1, y1 - y0 + 1},
                                 ConflictName(conflict.kind), detail));
    } else {
      int x = 0, y = 0;
      if (conflict.template_index < plan.placements.size()) {
        x = plan.placements[conflict.template_index].x();
        y = plan.placements[conflict.template_index].y();
      }
      issues.push_back({{"type", "marker"},
                        {"room", room_id},
                        {"tile", {x, y}},
                        {"label", ConflictName(conflict.kind)},
                        {"detail", detail}});
    }
  }
  return {{"format", "yaze-dungeon-proposal-overlay"},
          {"version", 1},
          {"title", name},
          {"status", plan.valid ? "Geometry plan; gameplay unqualified"
                                : "Blocked geometry plan"},
          {"rooms", {{{"room", room_id}, {"label", name}}}},
          {"notes",
           {"Annotations show a proposed assembly over the loaded room. They "
            "do not render or apply its object changes.",
            absl::StrFormat(
                "%d blocking conflicts; first 64 shown. Complete findings "
                "remain in plan.conflicts.",
                blocking_count),
            "Reserved regions constrain new writes. They do not establish "
            "existing floor, Link reachability or water-state behavior."}},
          {"layers",
           {{{"id", "planned-objects"},
             {"label", "Proposed object bounds"},
             {"color", "#62B6E8"},
             {"shapes", placements}},
            {{"id", "reserved-regions"},
             {"label", "Reserved regions"},
             {"color", "#C79EF2"},
             {"shapes", reserved}},
            {{"id", "blocking-conflicts"},
             {"label", "Connections and placement needing repair"},
             {"color", "#F36E67"},
             {"shapes", issues}}}}};
}
}  // namespace

absl::Status WriteRoomSpatialReport(Rom* rom, const zelda3::Room& room,
                                    const resources::ArgumentParser& parser,
                                    resources::OutputFormatter& formatter) {
  zelda3::RoomSpatialOptions options;
  zelda3::RoomTilemaps tilemaps;
  if (parser.HasFlag("spatial-collision")) {
    // GameData embeds the palettes of every graphics sheet. Keep that storage
    // off the Windows CLI's default stack, including non-collision requests.
    auto game_data = std::make_unique<zelda3::GameData>();
    RETURN_IF_ERROR(zelda3::LoadGameData(
        *rom, *game_data, zelda3::LoadOptions{.expand_rom = false}));
    auto rendered = zelda3::LoadRoomFromRom(rom, room.id());
    rendered.SetGameData(game_data.get());
    rendered.LoadRoomGraphics();
    rendered.RenderRoomGraphics();
    tilemaps = zelda3::ComposeYazeRoomTilemaps(rendered);
    options.tilemaps = &tilemaps;
  }
  ASSIGN_OR_RETURN(const auto analysis,
                   zelda3::AnalyzeRoomSpatial(rom, room, options));
  const bool cells = parser.HasFlag("spatial-cells");
  Json report = {
      {"schema", "yaze.dungeon_spatial.v1"},
      {"coordinate_space", "room_tile8"},
      {"bounds_convention", "half_open"},
      {"room_size", {{"width", 64}, {"height", 64}, {"pixels_per_tile", 8}}},
      {"rom_content_sha256", RomContentHash(*rom)},
      {"room_id", analysis.room_id},
      {"layout_id", analysis.layout_id},
      {"layout_loaded", analysis.layout_loaded},
      {"floor1_graphics", analysis.floor1},
      {"floor2_graphics", analysis.floor2},
      {"layer2_mode", analysis.layer2_mode},
      {"collision_mode", analysis.collision_mode},
      {"effect", analysis.effect},
      {"tag1", analysis.tag1},
      {"tag2", analysis.tag2},
      {"state", "default object branch; new-file collision load state"},
      {"runtime_qualified", false},
      {"diagnostics", analysis.diagnostics},
      {"semantics",
       {{"layout",
         "Shared layout template selected by object-stream byte 1 bits 2..4; "
         "separate from room header and authored route graph"},
        {"size",
         "Encoded routine-specific field or custom subtype; not width or "
         "height"},
        {"draw_lists",
         "0 primary upper, 1 lower overlay, 2 final upper; routine routing can "
         "write both planes"},
        {"planes",
         "upper=TILEMAPA/hardware BG2; lower=TILEMAPB/hardware BG1; collision "
         "planes are separate from visibility"},
        {"floor_graphics",
         "Base floor tile-pattern selectors, not dungeon floor numbers or "
         "collision planes"},
        {"overlap",
         "Shared drawn cells are ordered repaint observations, not automatic "
         "movement blockers"}}},
      {"registers",
       {{"tm", analysis.registers.tm},
        {"ts", analysis.registers.ts},
        {"cgadsub", analysis.registers.cgadsub},
        {"lower_shown", analysis.registers.LowerTilemapShown()},
        {"tilemaps_share_main", analysis.registers.TilemapsShareMainScreen()},
        {"upper_blended", analysis.registers.UpperTilemapBlended()}}}};
  report["layout_objects"] = Json::array();
  report["objects"] = Json::array();
  for (const auto& object : analysis.layout)
    report["layout_objects"].push_back(Footprint(object, cells));
  for (const auto& object : analysis.objects)
    report["objects"].push_back(Footprint(object, cells));
  report["doors"] = Json::array();
  for (const auto& door : analysis.doors) {
    report["doors"].push_back(
        {{"index", door.index},
         {"position", door.position},
         {"direction", door.direction},
         {"type", door.type},
         {"anchor", {{"x", door.anchor_x}, {"y", door.anchor_y}}},
         {"bounds", Bounds(door.bounds)},
         {"valid_position", door.valid_position}});
  }
  report["overlaps"] = Json::array();
  for (const auto& overlap : analysis.overlaps) {
    report["overlaps"].push_back(
        {{"first_index", overlap.first_index},
         {"second_index", overlap.second_index},
         {"first_is_layout", overlap.first_is_layout},
         {"second_is_layout", overlap.second_is_layout},
         {"upper_cells", overlap.upper_cells},
         {"lower_cells", overlap.lower_cells},
         {"classification", overlap.classification}});
  }
  report["retaining_wall_joins"] =
      WallJoins(analysis.retaining_wall_join_issues,
                analysis.retaining_wall_join_coverage,
                analysis.retaining_wall_join_unassessed_objects);
  report["retaining_wall_joins"]["layout_loaded"] = analysis.layout_loaded;
  if (!analysis.layout_loaded)
    report["retaining_wall_joins"]["status"] = "incomplete_context";
  report["collision"] = {
      {"computed", analysis.collision.has_value()},
      {"model",
       "vanilla room-load replay; custom overrides and live hooks are "
       "separate"},
      {"custom_override_cells", analysis.custom_collision_overrides}};
  if (analysis.collision) {
    for (int plane = 0; plane < 2; ++plane) {
      Json counts = Json::object();
      Json sources = Json::object();
      Json rows = Json::array();
      std::map<int, int> values;
      for (int y = 0; y < 64; ++y) {
        Json row = Json::array();
        for (int x = 0; x < 64; ++x) {
          const auto index = plane * zelda3::kCollisionMapTiles + y * 64 + x;
          const auto value = analysis.collision->attributes[index];
          ++values[value];
          const auto name =
              zelda3::CollisionSourceName(analysis.collision->sources[index]);
          sources[name] = sources.value(name, 0) + 1;
          if (cells)
            row.push_back(value);
        }
        if (cells)
          rows.push_back(std::move(row));
      }
      for (const auto& [value, count] : values)
        counts[absl::StrCat(value)] = count;
      const char* name = plane == 0 ? "upper" : "lower";
      report["collision"][name] = {{"attribute_counts", counts},
                                   {"source_counts", sources}};
      if (cells)
        report["collision"][name]["rows"] = std::move(rows);
    }
  }
  formatter.AddRawJsonField("spatial", report.dump());
  formatter.AddRawText(absl::StrCat(
      "\nSpatial model (Tile8, half-open): ", analysis.objects.size(),
      " room objects; ", analysis.layout.size(), " layout objects; ",
      analysis.overlaps.size(), " ordered overlaps. Layout ",
      analysis.layout_id, "; BG mode ", analysis.layer2_mode,
      "; collision key ", analysis.collision_mode,
      "; retaining-wall endpoint issues ",
      analysis.retaining_wall_join_issues.size(),
      ". Upper/lower visibility and collision remain separate.\n"));
  if (parser.HasFlag("spatial-strict-joins")) {
    if (!analysis.layout_loaded) {
      formatter.AddField("status", "blocked");
      return absl::FailedPreconditionError(
          "Retaining-wall inspection requires a decoded room layout; "
          "inspect spatial.retaining_wall_joins");
    }
    if (!analysis.retaining_wall_join_issues.empty()) {
      formatter.AddField("status", "blocked");
      return absl::FailedPreconditionError(
          "Known retaining-wall endpoint is occluded; inspect "
          "spatial.retaining_wall_joins");
    }
    if (analysis.retaining_wall_join_unassessed_objects != 0) {
      formatter.AddField("status", "blocked");
      return absl::FailedPreconditionError(
          "Retaining-wall inspection is incomplete; inspect "
          "spatial.retaining_wall_joins.unassessed_objects");
    }
  }
  return absl::OkStatus();
}

absl::Status DungeonTemplatePlanCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"room", "template", "x", "y"}));
  RETURN_IF_ERROR(RoomId(parser).status());
  RETURN_IF_ERROR(Decimal(*parser.GetString("x")).status());
  RETURN_IF_ERROR(Decimal(*parser.GetString("y")).status());
  RETURN_IF_ERROR(PlanOptions(parser).status());
  if (parser.HasFlag("write"))
    return absl::InvalidArgumentError(
        "Template planning is read-only; --write is not supported");
  return absl::OkStatus();
}

absl::Status DungeonTemplatePlanCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const int room_id, RoomId(parser));
  ASSIGN_OR_RETURN(const int x, Decimal(*parser.GetString("x")));
  ASSIGN_OR_RETURN(const int y, Decimal(*parser.GetString("y")));
  ASSIGN_OR_RETURN(auto options, PlanOptions(parser));
  const auto path = *parser.GetString("template");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return absl::NotFoundError("Cannot open object template: " + path);
  constexpr size_t kMaximumTemplateBytes = 1024 * 1024;
  std::string payload;
  char byte;
  while (input.get(byte)) {
    if (payload.size() == kMaximumTemplateBytes)
      return absl::ResourceExhaustedError("Object template exceeds 1 MiB");
    payload.push_back(byte);
  }
  if (input.bad())
    return absl::DataLossError("Cannot read object template: " + path);
  ASSIGN_OR_RETURN(const auto object_template,
                   zelda3::ParseObjectTemplateJson(payload));
  auto room = zelda3::LoadRoomFromRom(rom, room_id);
  zelda3::RoomLayout layout(rom);
  RETURN_IF_ERROR(layout.LoadLayout(room.layout_id()));
  std::vector<zelda3::RoomObject> existing = layout.GetObjects();
  const size_t layout_count = existing.size();
  existing.insert(existing.end(), room.GetTileObjects().begin(),
                  room.GetTileObjects().end());
  const size_t requested_clearance_count = options.clearance.size();
  for (const auto& door : room.GetDoors()) {
    if (!zelda3::DoorPositionManager::IsValidPosition(door.position,
                                                      door.direction)) {
      return absl::FailedPreconditionError(
          "Cannot plan around a door with an invalid position");
    }
    const auto [px, py, width, height] =
        zelda3::DoorPositionManager::GetDoorEditorBounds(
            door.position, door.direction, door.type);
    options.clearance.push_back(
        {{px / 8, py / 8, width / 8, height / 8}, std::nullopt});
  }
  const auto plan = zelda3::PlanObjectTemplatePlacement(
      rom, room_id, object_template, {x, y}, existing, options);
  Json result = {
      {"schema", "yaze.dungeon_template_plan.v1"},
      {"status", plan.valid ? "valid_geometry_plan" : "blocked_geometry_plan"},
      {"valid", plan.valid},
      {"write_performed", false},
      {"runtime_qualified", false},
      {"coordinate_space", "room_tile8"},
      {"room_id", room_id},
      {"anchor", {{"x", x}, {"y", y}}},
      {"template_name", object_template.name},
      {"rom_content_sha256", RomContentHash(*rom)},
      {"template_sha256", core::ComputeSourceArtifactSha256(payload)},
      {"state", "default object branch; active project custom-object context"},
      {"existing_index_space",
       "plan.existing_objects[].index; layout prefix then complete room "
       "vector"},
      {"validation_scope",
       "isolated default-state geometry, baseline layout/object writes and "
       "door editor bounds, known native retaining-wall endpoint loss and "
       "explicitly declared native retaining-wall cycles"},
      {"unverified_contracts",
       {"engine state and ordered registrations",
        "player approach and reachability", "target graphics compatibility",
        "wall joins outside the inspected native endpoint relation",
        "sprite/chest/event dependencies", "allocation and write preflight"}},
      {"generation",
       "deterministic translation only; procedural expansion is not "
       "implemented"}};
  result["retaining_wall_joins"] = WallJoins(
      plan.retaining_wall_join_issues, plan.retaining_wall_join_coverage,
      plan.retaining_wall_join_unassessed_objects);
  result["retaining_wall_joins"]["index_space"] =
      "existing_objects prefix, then template indices; draw-list sorting "
      "does not renumber these identities";
  result["retaining_wall_joins"]["template_index_base"] = existing.size();
  Json loops = {
      {"coverage", plan.retaining_wall_loop_coverage},
      {"complete_room_qualification", false},
      {"status", object_template.retaining_wall_loops.empty() ? "not_declared"
                                                              : "not_assessed"},
      {"checks", Json::array()}};
  bool loops_blocked = false;
  for (const auto& check : plan.retaining_wall_loop_checks) {
    Json ports = Json::array();
    for (const auto& port : check.required_ports) {
      ports.push_back({{"template_index", port.template_index},
                       {"object_id", port.object_id},
                       {"straight_id", port.straight_id},
                       {"direction", port.direction},
                       {"x", port.x},
                       {"y", port.y}});
    }
    loops["checks"].push_back(
        {{"name", check.name},
         {"status", check.status},
         {"members", check.members},
         {"matched_connection_count", check.matched_connection_count},
         {"coordinate_space", "room_tile8_boundaries"},
         {"required_ports", ports},
         {"diagnostics", check.diagnostics}});
    loops_blocked |= check.status == "blocked";
  }
  if (!plan.retaining_wall_loop_checks.empty()) {
    loops["status"] = loops_blocked ? "blocked"
                      : plan.retaining_wall_loop_checks.size() !=
                              object_template.retaining_wall_loops.size()
                          ? "incomplete_assessment"
                          : "closed_ports_not_final_pixels";
  }
  result["retaining_wall_loops"] = std::move(loops);
  result["existing_objects"] = Json::array();
  for (size_t index = 0; index < existing.size(); ++index) {
    const auto& object = existing[index];
    result["existing_objects"].push_back(
        {{"index", index},
         {"source", index < layout_count ? "layout" : "room"},
         {"source_index", index < layout_count ? index : index - layout_count},
         {"id", object.id_},
         {"anchor", {{"x", object.x()}, {"y", object.y()}}}});
  }
  result["clearances"] = Json::array();
  for (size_t index = 0; index < options.clearance.size(); ++index) {
    const auto& clearance = options.clearance[index];
    result["clearances"].push_back(
        {{"index", index},
         {"source", index < requested_clearance_count ? "requested"
                                                      : "door_editor_bounds"},
         {"bounds", Bounds(clearance.bounds)},
         {"plane", !clearance.plane        ? "both"
                   : *clearance.plane == 0 ? "upper"
                                           : "lower"}});
  }
  result["placements"] = Json::array();
  for (size_t index = 0; index < plan.placements.size(); ++index) {
    const auto& object = plan.placements[index];
    const auto bytes = object.EncodeObjectToBytes();
    result["placements"].push_back(
        {{"template_index", index},
         {"id", object.id_},
         {"x", object.x()},
         {"y", object.y()},
         {"size", object.size_},
         {"draw_list", object.GetLayerValue()},
         {"encoded", {bytes.b1, bytes.b2, bytes.b3}}});
  }
  result["footprints"] = Json::array();
  for (const auto& footprint : plan.footprints)
    result["footprints"].push_back(
        Footprint(footprint, parser.HasFlag("cells")));
  result["conflicts"] = Json::array();
  for (const auto& conflict : plan.conflicts) {
    Json item = {{"kind", ConflictName(conflict.kind)},
                 {"blocking", conflict.blocking},
                 {"template_index", conflict.template_index},
                 {"message", conflict.message},
                 {"cell_count", conflict.cell_count}};
    if (conflict.existing_index)
      item["existing_index"] = *conflict.existing_index;
    if (conflict.other_template_index)
      item["other_template_index"] = *conflict.other_template_index;
    if (conflict.expected_region)
      item["expected_region"] = Bounds(*conflict.expected_region);
    item["cells"] = Json::array();
    for (const auto& cell : conflict.cells)
      item["cells"].push_back(Cell(cell));
    result["conflicts"].push_back(std::move(item));
  }
  result["proposal_overlay"] =
      TemplateProposal(plan, options, room_id, object_template.name);
  for (const auto& check : plan.retaining_wall_loop_checks) {
    result["proposal_overlay"]["notes"].push_back(
        check.name + ": " + check.status +
        ". Declared native wall "
        "connections only; final water/ceiling pixels and playable space "
        "remain separate requirements.");
  }
  formatter.AddRawJsonField("plan", result.dump());
  formatter.AddRawText(absl::StrCat(
      "\n", object_template.name, ": ",
      plan.valid ? "valid geometry plan" : "blocked geometry plan", "; ",
      plan.placements.size(), " ordered placements; ", plan.conflicts.size(),
      " observations/conflicts. No ROM write.\n"));
  return plan.valid
             ? absl::OkStatus()
             : absl::FailedPreconditionError(
                   "Template placement is blocked; inspect plan.conflicts");
}
}  // namespace yaze::cli::handlers
