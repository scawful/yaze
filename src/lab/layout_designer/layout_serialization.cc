#include "lab/layout_designer/layout_serialization.h"

#include <fstream>
#include <sstream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "util/log.h"

// Simple JSON-like serialization (can be replaced with nlohmann/json later)
namespace yaze {
namespace editor {
namespace layout_designer {

namespace {

// Helper to escape JSON strings
std::string EscapeJson(const std::string& str) {
  std::string result;
  for (char chr : str) {
    switch (chr) {
      case '"': result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default: result += chr; break;
    }
  }
  return result;
}

std::string DirToString(ImGuiDir dir) {
  switch (dir) {
    case ImGuiDir_None: return "none";
    case ImGuiDir_Left: return "left";
    case ImGuiDir_Right: return "right";
    case ImGuiDir_Up: return "up";
    case ImGuiDir_Down: return "down";
    case ImGuiDir_COUNT: return "none";  // Unused, but handle for completeness
    default: return "none";
  }
}

ImGuiDir StringToDir(const std::string& str) {
  if (str == "left") return ImGuiDir_Left;
  if (str == "right") return ImGuiDir_Right;
  if (str == "up") return ImGuiDir_Up;
  if (str == "down") return ImGuiDir_Down;
  return ImGuiDir_None;
}

std::string NodeTypeToString(DockNodeType type) {
  switch (type) {
    case DockNodeType::Root: return "root";
    case DockNodeType::Split: return "split";
    case DockNodeType::Leaf: return "leaf";
    default: return "leaf";
  }
}

DockNodeType StringToNodeType(const std::string& str) {
  if (str == "root") return DockNodeType::Root;
  if (str == "split") return DockNodeType::Split;
  if (str == "leaf") return DockNodeType::Leaf;
  return DockNodeType::Leaf;
}

}  // namespace

std::string LayoutSerializer::ToJson(const LayoutDefinition& layout) {
  std::ostringstream json;
  
  json << "{\n";
  json << "  \"layout\": {\n";
  json << "    \"name\": \"" << EscapeJson(layout.name) << "\",\n";
  json << "    \"description\": \"" << EscapeJson(layout.description) << "\",\n";
  json << "    \"version\": \"" << EscapeJson(layout.version) << "\",\n";
  json << "    \"author\": \"" << EscapeJson(layout.author) << "\",\n";
  json << "    \"created_timestamp\": " << layout.created_timestamp << ",\n";
  json << "    \"modified_timestamp\": " << layout.modified_timestamp << ",\n";
  json << "    \"canvas_size\": [" << layout.canvas_size.x << ", " 
       << layout.canvas_size.y << "],\n";
  
  if (layout.root) {
    json << "    \"root_node\": " << SerializeDockNode(*layout.root) << "\n";
  } else {
    json << "    \"root_node\": null\n";
  }
  
  json << "  }\n";
  json << "}\n";
  
  return json.str();
}

std::string LayoutSerializer::SerializeDockNode(const DockNode& node) {
  std::ostringstream json;
  
  json << "{\n";
  json << "      \"type\": \"" << NodeTypeToString(node.type) << "\",\n";
  
  if (node.IsSplit()) {
    json << "      \"split_dir\": \"" << DirToString(node.split_dir) << "\",\n";
    json << "      \"split_ratio\": " << node.split_ratio << ",\n";
    
    json << "      \"left_child\": ";
    if (node.child_left) {
      json << SerializeDockNode(*node.child_left);
    } else {
      json << "null";
    }
    json << ",\n";
    
    json << "      \"right_child\": ";
    if (node.child_right) {
      json << SerializeDockNode(*node.child_right);
    } else {
      json << "null";
    }
    json << "\n";
    
  } else if (node.IsLeaf()) {
    json << "      \"panels\": [\n";
    for (size_t idx = 0; idx < node.panels.size(); ++idx) {
      json << "        " << SerializePanel(node.panels[idx]);
      if (idx < node.panels.size() - 1) {
        json << ",";
      }
      json << "\n";
    }
    json << "      ]\n";
  }
  
  json << "    }";
  
  return json.str();
}

std::string LayoutSerializer::SerializePanel(const LayoutPanel& panel) {
  return absl::StrFormat(
      "{\"id\":\"%s\",\"name\":\"%s\",\"icon\":\"%s\","
      "\"priority\":%d,\"visible\":%s,\"closable\":%s,\"pinnable\":%s}",
      EscapeJson(panel.panel_id),
      EscapeJson(panel.display_name),
      EscapeJson(panel.icon),
      panel.priority,
      panel.visible_by_default ? "true" : "false",
      panel.closable ? "true" : "false",
      panel.pinnable ? "true" : "false");
}

absl::StatusOr<LayoutDefinition> LayoutSerializer::FromJson(
    const std::string& json_str) {
  // TODO(scawful): Implement full JSON parsing
  // For now, this is a placeholder that returns an error
  return absl::UnimplementedError(
      "JSON deserialization not yet implemented. "
      "This requires integrating nlohmann/json library.");
}

absl::Status LayoutSerializer::SaveToFile(const LayoutDefinition& layout,
                                          const std::string& filepath) {
  std::ofstream file(filepath);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open file for writing: %s", filepath));
  }
  
  std::string json = ToJson(layout);
  file << json;
  file.close();
  
  if (file.fail()) {
    return absl::InternalError(
        absl::StrFormat("Failed to write to file: %s", filepath));
  }
  
  LOG_INFO("LayoutSerializer", "Saved layout to: %s", filepath.c_str());
  return absl::OkStatus();
}

absl::StatusOr<LayoutDefinition> LayoutSerializer::LoadFromFile(
    const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open file for reading: %s", filepath));
  }
  
  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();
  
  return FromJson(buffer.str());
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

