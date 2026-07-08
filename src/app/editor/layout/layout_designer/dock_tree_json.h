#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_JSON_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_JSON_H_

#include "absl/status/statusor.h"
#include "app/editor/layout/layout_designer/dock_tree.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace editor {
namespace layout_designer {

// Serialize a DockTree to its canonical JSON shape. Infallible — any
// in-memory tree round-trips. Does not validate, since callers have
// already done so via DockTree::Validate or are explicitly persisting
// a partially-constructed tree.
nlohmann::json DockTreeToJson(const DockTree& tree);

// Parse a JSON object into a DockTree. Returns an InvalidArgument status
// on malformed input. Unknown fields are silently ignored (forward-
// compat). The returned tree is NOT Validate()d — callers can do so if
// they want to reject otherwise-parseable-but-semantically-invalid
// shapes (e.g. ratios out of range).
//
// Optional fields and their defaults:
//   schema_version: 1
//   description:    ""
//   leaf panels[].display_name / .icon: ""
//   active_tab_index: 0
absl::StatusOr<DockTree> DockTreeFromJson(const nlohmann::json& j);

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_JSON_H_
