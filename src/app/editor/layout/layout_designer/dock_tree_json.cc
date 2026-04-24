#include "app/editor/layout/layout_designer/dock_tree_json.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"

namespace yaze {
namespace editor {
namespace layout_designer {

namespace {

// The schema version this code emits and parses against. Bump in
// lockstep with format-changing edits; legacy JSON without a schema
// field is still accepted (treated as v1 → fresh ids on parse).
constexpr std::uint64_t kCurrentDockTreeSchemaVersion = 2;

const char* SplitDirectionToString(SplitDirection d) {
  switch (d) {
    case SplitDirection::kLeft:
      return "left";
    case SplitDirection::kRight:
      return "right";
    case SplitDirection::kUp:
      return "up";
    case SplitDirection::kDown:
      return "down";
  }
  return "left";
}

absl::StatusOr<SplitDirection> SplitDirectionFromString(const std::string& s) {
  if (s == "left")
    return SplitDirection::kLeft;
  if (s == "right")
    return SplitDirection::kRight;
  if (s == "up")
    return SplitDirection::kUp;
  if (s == "down")
    return SplitDirection::kDown;
  return absl::InvalidArgumentError(
      absl::StrCat("unknown split direction: '", s, "'"));
}

nlohmann::json PanelToJson(const PanelEntry& p) {
  nlohmann::json j;
  j["panel_id"] = p.panel_id;
  j["display_name"] = p.display_name;
  j["icon"] = p.icon;
  return j;
}

absl::StatusOr<PanelEntry> PanelFromJson(const nlohmann::json& j) {
  if (!j.is_object()) {
    return absl::InvalidArgumentError("panel entry must be a JSON object");
  }
  if (!j.contains("panel_id") || !j.at("panel_id").is_string()) {
    return absl::InvalidArgumentError("panel entry missing string panel_id");
  }
  PanelEntry p;
  p.panel_id = j.at("panel_id").get<std::string>();
  p.display_name = j.value("display_name", "");
  p.icon = j.value("icon", "");
  return p;
}

nlohmann::json NodeToJson(const DockNode& node) {
  nlohmann::json j;
  // Schema v2: every real node carries a `DockNodeId`. The default-
  // constructed sentinel (kInvalidDockNodeId) doesn't survive serialization
  // — that only happens for raw `DockNode{}` instances which the live tree
  // never holds. Emit the id unconditionally for non-zero values so v2
  // round-trips faithfully; v1-loaded trees that didn't carry ids will
  // have been re-allocated at parse time and emit cleanly.
  if (node.id != kInvalidDockNodeId) {
    j["id"] = node.id;
  }
  if (node.type == DockNode::Type::kLeaf) {
    j["type"] = "leaf";
    j["active_tab_index"] = node.active_tab_index;
    nlohmann::json panels = nlohmann::json::array();
    for (const auto& p : node.panels) {
      panels.push_back(PanelToJson(p));
    }
    j["panels"] = std::move(panels);
  } else {
    j["type"] = "split";
    j["direction"] = SplitDirectionToString(node.split_direction);
    j["ratio"] = node.split_ratio;
    j["child_a"] = node.child_a ? NodeToJson(*node.child_a) : nlohmann::json();
    j["child_b"] = node.child_b ? NodeToJson(*node.child_b) : nlohmann::json();
  }
  return j;
}

// Read the `id` field if present; otherwise allocate a fresh one. Either
// way, the result is non-zero so live trees never carry the sentinel.
DockNodeId ResolveNodeId(const nlohmann::json& j) {
  if (j.contains("id") && j.at("id").is_number_unsigned()) {
    const auto id = j.at("id").get<DockNodeId>();
    if (id != kInvalidDockNodeId) {
      // Bump the global counter so subsequent allocations don't collide
      // with a future load of the same persisted tree.
      internal::ObserveDockNodeId(id);
      return id;
    }
  }
  return internal::AllocateDockNodeId();
}

absl::StatusOr<std::unique_ptr<DockNode>> NodeFromJson(
    const nlohmann::json& j) {
  if (!j.is_object()) {
    return absl::InvalidArgumentError("dock node must be a JSON object");
  }
  const std::string type = j.value("type", "");
  if (type == "leaf") {
    auto node = std::make_unique<DockNode>();
    node->id = ResolveNodeId(j);
    node->type = DockNode::Type::kLeaf;
    node->active_tab_index = j.value("active_tab_index", 0);
    if (j.contains("panels")) {
      const auto& panels = j.at("panels");
      if (!panels.is_array()) {
        return absl::InvalidArgumentError("leaf 'panels' must be an array");
      }
      for (const auto& p : panels) {
        auto entry = PanelFromJson(p);
        if (!entry.ok())
          return entry.status();
        node->panels.push_back(std::move(*entry));
      }
    }
    return node;
  }
  if (type == "split") {
    auto node = std::make_unique<DockNode>();
    node->id = ResolveNodeId(j);
    node->type = DockNode::Type::kSplit;
    const std::string dir_str = j.value("direction", "left");
    auto dir = SplitDirectionFromString(dir_str);
    if (!dir.ok())
      return dir.status();
    node->split_direction = *dir;
    node->split_ratio = j.value("ratio", 0.5f);
    if (!j.contains("child_a") || !j.contains("child_b")) {
      return absl::InvalidArgumentError("split node missing child_a/child_b");
    }
    auto a = NodeFromJson(j.at("child_a"));
    if (!a.ok())
      return a.status();
    auto b = NodeFromJson(j.at("child_b"));
    if (!b.ok())
      return b.status();
    node->child_a = std::move(*a);
    node->child_b = std::move(*b);
    return node;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("unknown dock node type: '", type, "'"));
}

}  // namespace

nlohmann::json DockTreeToJson(const DockTree& tree) {
  nlohmann::json j;
  // The writer is the authority on schema version: every saved file
  // carries the current schema regardless of what `tree.schema_version`
  // happens to hold. This keeps legacy v1 trees from re-saving as a
  // self-contradictory document (v1 header, v2 body). The parser
  // normalizes the in-memory representation to the same constant so
  // round-trips are stable.
  j["schema_version"] = kCurrentDockTreeSchemaVersion;
  j["name"] = tree.name;
  j["description"] = tree.description;
  j["root"] = tree.root ? NodeToJson(*tree.root)
                        : NodeToJson(DockNode{});  // empty leaf
  return j;
}

absl::StatusOr<DockTree> DockTreeFromJson(const nlohmann::json& j) {
  if (!j.is_object()) {
    return absl::InvalidArgumentError("dock tree must be a JSON object");
  }
  // Read the on-disk version for diagnostics / future migration logic
  // hooks, but the in-memory tree always lands at the current schema
  // because every node ends up with a real id (the parser allocates
  // fresh ones for v1 input). Keeping `tree.schema_version` synced to
  // the in-memory shape avoids the v1-header / v2-body class of bug.
  (void)j.value("schema_version", 1ULL);

  DockTree tree;
  tree.schema_version = kCurrentDockTreeSchemaVersion;
  tree.name = j.value("name", "");
  tree.description = j.value("description", "");
  if (j.contains("root")) {
    auto root = NodeFromJson(j.at("root"));
    if (!root.ok())
      return root.status();
    tree.root = std::move(*root);
  }
  // If "root" is missing, the default-constructed empty leaf stays —
  // its id was already allocated by `DockTree::DockTree()`'s call to
  // `MakeLeaf({})`.
  return tree;
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
