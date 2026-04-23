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

absl::StatusOr<std::unique_ptr<DockNode>> NodeFromJson(
    const nlohmann::json& j) {
  if (!j.is_object()) {
    return absl::InvalidArgumentError("dock node must be a JSON object");
  }
  const std::string type = j.value("type", "");
  if (type == "leaf") {
    auto node = std::make_unique<DockNode>();
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
  j["schema_version"] = tree.schema_version;
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
  DockTree tree;
  tree.schema_version = j.value("schema_version", 1ULL);
  tree.name = j.value("name", "");
  tree.description = j.value("description", "");
  if (j.contains("root")) {
    auto root = NodeFromJson(j.at("root"));
    if (!root.ok())
      return root.status();
    tree.root = std::move(*root);
  }
  // If "root" is missing, the default-constructed empty leaf stays.
  return tree;
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
