#include "app/editor/layout_designer/layout_definition.h"

#include <chrono>

namespace yaze {
namespace editor {
namespace layout_designer {

// ============================================================================
// DockNode Implementation
// ============================================================================

void DockNode::AddPanel(const LayoutPanel& panel) {
  if (type == DockNodeType::Split) {
    // Can only add panels to leaf/root nodes
    return;
  }
  panels.push_back(panel);
}

void DockNode::Split(ImGuiDir direction, float ratio) {
  if (type == DockNodeType::Split) {
    // Already split
    return;
  }
  
  type = DockNodeType::Split;
  split_dir = direction;
  split_ratio = ratio;
  
  // Move existing panels to left child
  child_left = std::make_unique<DockNode>();
  child_left->type = DockNodeType::Leaf;
  child_left->panels = std::move(panels);
  panels.clear();
  
  // Create empty right child
  child_right = std::make_unique<DockNode>();
  child_right->type = DockNodeType::Leaf;
}

LayoutPanel* DockNode::FindPanel(const std::string& panel_id) {
  if (type == DockNodeType::Leaf) {
    for (auto& panel : panels) {
      if (panel.panel_id == panel_id) {
        return &panel;
      }
    }
    return nullptr;
  }
  
  // Search children
  if (child_left) {
    if (auto* found = child_left->FindPanel(panel_id)) {
      return found;
    }
  }
  if (child_right) {
    if (auto* found = child_right->FindPanel(panel_id)) {
      return found;
    }
  }
  
  return nullptr;
}

size_t DockNode::CountPanels() const {
  if (type == DockNodeType::Leaf || type == DockNodeType::Root) {
    return panels.size();
  }
  
  size_t count = 0;
  if (child_left) {
    count += child_left->CountPanels();
  }
  if (child_right) {
    count += child_right->CountPanels();
  }
  return count;
}

std::unique_ptr<DockNode> DockNode::Clone() const {
  auto clone = std::make_unique<DockNode>();
  clone->type = type;
  clone->node_id = node_id;
  clone->split_dir = split_dir;
  clone->split_ratio = split_ratio;
  clone->flags = flags;
  clone->panels = panels;
  
  if (child_left) {
    clone->child_left = child_left->Clone();
  }
  if (child_right) {
    clone->child_right = child_right->Clone();
  }
  
  return clone;
}

// ============================================================================
// LayoutDefinition Implementation
// ============================================================================

LayoutDefinition LayoutDefinition::CreateEmpty(const std::string& name) {
  LayoutDefinition layout;
  layout.name = name;
  layout.description = "Empty layout";
  layout.root = std::make_unique<DockNode>();
  layout.root->type = DockNodeType::Root;
  
  auto now = std::chrono::system_clock::now();
  layout.created_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
      now.time_since_epoch()).count();
  layout.modified_timestamp = layout.created_timestamp;
  
  return layout;
}

std::unique_ptr<LayoutDefinition> LayoutDefinition::Clone() const {
  auto clone = std::make_unique<LayoutDefinition>();
  clone->name = name;
  clone->description = description;
  clone->editor_type = editor_type;
  clone->canvas_size = canvas_size;
  clone->author = author;
  clone->version = version;
  clone->created_timestamp = created_timestamp;
  clone->modified_timestamp = modified_timestamp;
  
  if (root) {
    clone->root = root->Clone();
  }
  
  return clone;
}

LayoutPanel* LayoutDefinition::FindPanel(const std::string& panel_id) const {
  if (!root) {
    return nullptr;
  }
  return root->FindPanel(panel_id);
}

std::vector<LayoutPanel*> LayoutDefinition::GetAllPanels() const {
  std::vector<LayoutPanel*> result;
  
  if (!root) {
    return result;
  }
  
  // Recursive helper to collect panels
  std::function<void(DockNode*)> collect = [&](DockNode* node) {
    if (!node) return;
    
    if (node->type == DockNodeType::Leaf) {
      for (auto& panel : node->panels) {
        result.push_back(&panel);
      }
    } else {
      collect(node->child_left.get());
      collect(node->child_right.get());
    }
  };
  
  collect(root.get());
  return result;
}

bool LayoutDefinition::Validate(std::string* error_message) const {
  if (name.empty()) {
    if (error_message) {
      *error_message = "Layout name cannot be empty";
    }
    return false;
  }
  
  if (!root) {
    if (error_message) {
      *error_message = "Layout must have a root node";
    }
    return false;
  }
  
  // Validate that split nodes have both children
  std::function<bool(const DockNode*)> validate_node = 
      [&](const DockNode* node) -> bool {
    if (!node) {
      if (error_message) {
        *error_message = "Null node found in tree";
      }
      return false;
    }
    
    if (node->type == DockNodeType::Split) {
      if (!node->child_left || !node->child_right) {
        if (error_message) {
          *error_message = "Split node must have both children";
        }
        return false;
      }
      
      if (node->split_ratio <= 0.0f || node->split_ratio >= 1.0f) {
        if (error_message) {
          *error_message = "Split ratio must be between 0.0 and 1.0";
        }
        return false;
      }
      
      if (!validate_node(node->child_left.get())) {
        return false;
      }
      if (!validate_node(node->child_right.get())) {
        return false;
      }
    }
    
    return true;
  };
  
  return validate_node(root.get());
}

void LayoutDefinition::Touch() {
  auto now = std::chrono::system_clock::now();
  modified_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
      now.time_since_epoch()).count();
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
