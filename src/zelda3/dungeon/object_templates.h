#ifndef YAZE_APP_ZELDA3_DUNGEON_OBJECT_TEMPLATES_H
#define YAZE_APP_ZELDA3_DUNGEON_OBJECT_TEMPLATES_H

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/room_spatial.h"

namespace yaze {
namespace zelda3 {

// Represents an object within a template, relative to the template's origin
struct TemplateObject {
  int id;
  int rel_x;  // Relative X offset
  int rel_y;  // Relative Y offset
  int size;
  int layer;
};

struct ObjectTemplateRetainingWallLoop {
  std::string name;
  // Stable indices in objects, never painter-sort positions. Changing this
  // declaration changes the authored contract; callers must pin the payload.
  std::vector<size_t> members;
};

// Represents a reusable group of objects
struct ObjectTemplate {
  std::string name;
  std::string description;
  std::string category;
  std::vector<TemplateObject> objects;
  std::vector<ObjectTemplateRetainingWallLoop> retaining_wall_loops = {};
};

// Strict readers, also used by the manager, reject unknown/missing/duplicate
// fields and never return a partial
// template. Relative coordinates are tile8 offsets in -63..63.
absl::StatusOr<ObjectTemplate> ParseObjectTemplateJson(
    std::string_view payload);
absl::StatusOr<ObjectTemplate> LoadObjectTemplateFile(const std::string& path);

struct ObjectTemplateTile8Anchor {
  int x = 0;
  int y = 0;
};

struct ObjectTemplateClearance {
  RoomSpatialBounds bounds;
  // Yaze buffer plane: 0=upper/BG1, 1=lower/BG2. Empty means both.
  std::optional<uint8_t> plane;
};

struct ObjectTemplatePlanOptions {
  std::vector<ObjectTemplateClearance> clearance;
  std::vector<size_t> allowed_existing_indices;
  // (template object index, existing object index).
  std::vector<std::pair<size_t, size_t>> allowed_existing_pairs;
};

enum class ObjectTemplatePlanConflictKind {
  kInvalidTemplate,
  kInvalidPlacement,
  kUnmeasuredFootprint,
  kOutsideRoom,
  kClearance,
  kExistingOverlap,
  kTemplateOverlap,
  kWallJoinLoss,
  kAssemblyConnection,
};

struct ObjectTemplatePlanConflict {
  ObjectTemplatePlanConflictKind kind =
      ObjectTemplatePlanConflictKind::kInvalidTemplate;
  size_t template_index = 0;
  std::optional<size_t> existing_index;
  std::optional<size_t> other_template_index;
  bool blocking = true;
  std::string message;
  size_t cell_count = 0;
  // Deterministically ordered physical-plane cells. Large intersections retain
  // the first 64 cells; cell_count always reports the complete intersection.
  std::vector<RoomSpatialCell> cells;
  // Expected adjacent connection zone, not an asserted rendered footprint.
  // It can extend outside the room so the failure location remains truthful.
  std::optional<RoomSpatialBounds> expected_region;
};

struct ObjectTemplateWallPort {
  size_t template_index = 0;
  int object_id = 0;
  int straight_id = 0;
  std::string direction;
  // Tile8 boundary origin, not the final written cell; the edge spans 4 tiles.
  int x = 0;
  int y = 0;
};

struct ObjectTemplateWallLoopCheck {
  std::string name;
  std::string status = "blocked";
  std::vector<size_t> members;
  size_t matched_connection_count = 0;
  std::vector<ObjectTemplateWallPort> required_ports;
  std::vector<std::string> diagnostics;
};

struct ObjectTemplatePlacementPlan {
  bool valid = false;
  // Complete original order, never an in-bounds subset. If integer fields
  // cannot safely construct RoomObjects, this is empty and conflicts explain
  // the failure. A caller must require valid before applying any placement.
  std::vector<RoomObject> placements;
  std::vector<RoomObjectFootprint> footprints;
  std::vector<ObjectTemplatePlanConflict> conflicts;
  std::string retaining_wall_join_coverage = kRetainingWallJoinCoverage;
  size_t retaining_wall_join_unassessed_objects = 0;
  // Newly affected endpoints only, with complete candidate issue counts.
  // Indices use existing[e], proposed[existing.size()+i], not sorted positions.
  std::vector<RoomWallJoinIssue> retaining_wall_join_issues;
  std::string retaining_wall_loop_coverage =
      "declared_native_retaining_wall_cycles_corner_straight_v1";
  std::vector<ObjectTemplateWallLoopCheck> retaining_wall_loop_checks;
};

// Read-only planning through the same renderer footprints as spatial analysis.
// Intra-template overlap is reported but does not invalidate ordered repaint.
// Existing-object overlap requires an explicit allowed index/pair. Clearance
// rectangles always block matching physical-plane writes.
ObjectTemplatePlacementPlan PlanObjectTemplatePlacement(
    Rom* rom, int room_id, const ObjectTemplate& object_template,
    ObjectTemplateTile8Anchor anchor,
    std::span<const RoomObject> existing_objects = {},
    const ObjectTemplatePlanOptions& options = {},
    const DungeonState* state = nullptr);

class ObjectTemplateManager {
 public:
  ObjectTemplateManager();
  ~ObjectTemplateManager() = default;

  // Load templates from the assets directory
  absl::Status LoadTemplates(const std::string& directory_path);

  // Save a new template
  absl::Status SaveTemplate(const ObjectTemplate& tmpl,
                            const std::string& directory_path);

  // Get all loaded templates
  const std::vector<ObjectTemplate>& GetTemplates() const { return templates_; }

  // Create a template from a selection of room objects
  // origin_x/y are usually the top-left most object's coordinates or a specific
  // anchor
  static ObjectTemplate CreateFromObjects(
      const std::string& name, const std::string& description,
      const std::vector<RoomObject>& objects, int origin_x, int origin_y);

  // Instantiate a template at a specific position
  std::vector<RoomObject> InstantiateTemplate(const ObjectTemplate& tmpl, int x,
                                              int y, Rom* rom);

 private:
  std::vector<ObjectTemplate> templates_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_OBJECT_TEMPLATES_H
