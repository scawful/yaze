#include "app/editor/layout/layout_designer/dock_tree.h"

#include <atomic>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace yaze {
namespace editor {
namespace layout_designer {

namespace {

// Process-wide monotonic counter for DockNodeId. Starts at 1 because 0 is
// reserved as the kInvalidDockNodeId sentinel.
std::atomic<DockNodeId> g_next_dock_node_id{1};

}  // namespace

namespace internal {

DockNodeId AllocateDockNodeId() {
  return g_next_dock_node_id.fetch_add(1, std::memory_order_relaxed);
}

void ObserveDockNodeId(DockNodeId id) {
  if (id == kInvalidDockNodeId)
    return;
  // Bump the counter to id + 1 if we'd otherwise hand out a value <= id.
  // Use compare-exchange-weak so concurrent observers race cleanly.
  DockNodeId expected = g_next_dock_node_id.load(std::memory_order_relaxed);
  while (expected <= id) {
    if (g_next_dock_node_id.compare_exchange_weak(expected, id + 1,
                                                  std::memory_order_relaxed)) {
      return;
    }
    // compare_exchange_weak refreshes `expected` on failure; loop and retry.
  }
}

}  // namespace internal

std::unique_ptr<DockNode> DockNode::MakeLeaf(std::vector<PanelEntry> panels) {
  auto node = std::make_unique<DockNode>();
  node->id = internal::AllocateDockNodeId();
  node->type = Type::kLeaf;
  node->panels = std::move(panels);
  node->active_tab_index = 0;
  return node;
}

std::unique_ptr<DockNode> DockNode::MakeSplit(SplitDirection dir, float ratio,
                                              std::unique_ptr<DockNode> a,
                                              std::unique_ptr<DockNode> b) {
  auto node = std::make_unique<DockNode>();
  node->id = internal::AllocateDockNodeId();
  node->type = Type::kSplit;
  node->split_direction = dir;
  node->split_ratio = ratio;
  node->child_a = std::move(a);
  node->child_b = std::move(b);
  return node;
}

std::unique_ptr<DockNode> DockNode::Clone() const {
  auto copy = std::make_unique<DockNode>();
  // Preserve id verbatim — Clone is the undo/redo and snapshot path, and
  // selection-by-id only survives those operations if the cloned tree's
  // node ids match the originals'.
  copy->id = id;
  copy->type = type;
  copy->panels = panels;
  copy->active_tab_index = active_tab_index;
  copy->split_direction = split_direction;
  copy->split_ratio = split_ratio;
  if (child_a)
    copy->child_a = child_a->Clone();
  if (child_b)
    copy->child_b = child_b->Clone();
  return copy;
}

void DockNode::SplitInPlace(SplitDirection dir, float ratio,
                            std::unique_ptr<DockNode> new_child,
                            bool new_child_first) {
  auto saved_panels = std::move(panels);
  const int saved_tab = active_tab_index;

  type = Type::kSplit;
  split_direction = dir;
  split_ratio = ratio;
  panels.clear();
  active_tab_index = 0;

  auto existing_leaf = MakeLeaf(std::move(saved_panels));
  existing_leaf->active_tab_index = saved_tab;

  if (new_child_first) {
    child_a = std::move(new_child);
    child_b = std::move(existing_leaf);
  } else {
    child_a = std::move(existing_leaf);
    child_b = std::move(new_child);
  }
}

bool DockNode::PromoteSingleChild() {
  if (type != Type::kSplit)
    return false;
  if (child_a && child_b)
    return false;
  auto surviving = child_a ? std::move(child_a) : std::move(child_b);
  if (!surviving)
    return false;
  *this = std::move(*surviving);
  return true;
}

const PanelEntry* DockNode::FindPanel(const std::string& panel_id) const {
  if (type == Type::kLeaf) {
    for (const auto& p : panels) {
      if (p.panel_id == panel_id)
        return &p;
    }
    return nullptr;
  }
  if (child_a) {
    if (const auto* hit = child_a->FindPanel(panel_id))
      return hit;
  }
  if (child_b) {
    if (const auto* hit = child_b->FindPanel(panel_id))
      return hit;
  }
  return nullptr;
}

DockTree::DockTree() : root(DockNode::MakeLeaf({})) {}

DockTree::DockTree(std::string n)
    : name(std::move(n)), root(DockNode::MakeLeaf({})) {}

DockTree DockTree::Clone() const {
  DockTree copy;
  copy.name = name;
  copy.description = description;
  copy.schema_version = schema_version;
  if (root)
    copy.root = root->Clone();
  return copy;
}

namespace {

bool ValidateNode(const DockNode& node,
                  std::unordered_set<std::string>* seen_ids,
                  std::string* error) {
  if (node.type == DockNode::Type::kLeaf) {
    const int n = static_cast<int>(node.panels.size());
    if (n == 0) {
      if (node.active_tab_index != 0) {
        if (error)
          *error = "empty leaf has non-zero active_tab_index";
        return false;
      }
    } else if (node.active_tab_index < 0 || node.active_tab_index >= n) {
      if (error)
        *error = "active_tab_index out of range";
      return false;
    }
    for (const auto& p : node.panels) {
      if (p.panel_id.empty()) {
        if (error)
          *error = "panel has empty panel_id";
        return false;
      }
      auto inserted = seen_ids->insert(p.panel_id);
      if (!inserted.second) {
        if (error)
          *error = "panel id '" + p.panel_id + "' appears twice";
        return false;
      }
    }
    return true;
  }

  // kSplit
  if (!node.child_a || !node.child_b) {
    if (error)
      *error = "split node has null child";
    return false;
  }
  if (!(node.split_ratio >= 0.05f && node.split_ratio <= 0.95f)) {
    if (error)
      *error = "split_ratio out of range [0.05, 0.95]";
    return false;
  }
  return ValidateNode(*node.child_a, seen_ids, error) &&
         ValidateNode(*node.child_b, seen_ids, error);
}

}  // namespace

bool DockTree::Validate(std::string* error) const {
  if (!root) {
    if (error)
      *error = "tree has null root";
    return false;
  }
  std::unordered_set<std::string> seen_ids;
  return ValidateNode(*root, &seen_ids, error);
}

namespace {

const DockNode* FindNodeInSubtree(const DockNode* node, DockNodeId id) {
  if (node == nullptr)
    return nullptr;
  if (node->id == id)
    return node;
  if (node->type == DockNode::Type::kSplit) {
    if (const auto* hit = FindNodeInSubtree(node->child_a.get(), id))
      return hit;
    if (const auto* hit = FindNodeInSubtree(node->child_b.get(), id))
      return hit;
  }
  return nullptr;
}

}  // namespace

const DockNode* DockTree::FindNode(DockNodeId id) const {
  if (id == kInvalidDockNodeId)
    return nullptr;
  return FindNodeInSubtree(root.get(), id);
}

DockNode* DockTree::FindNode(DockNodeId id) {
  return const_cast<DockNode*>(
      static_cast<const DockTree*>(this)->FindNode(id));
}

DockTree MakeEmptyTree(std::string name) {
  return DockTree{std::move(name)};
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
