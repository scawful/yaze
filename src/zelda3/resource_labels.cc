#include "zelda3/resource_labels.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "zelda3/sprite/sprite.h"
#include "zelda3/zelda3_labels.h"

// External declarations for Hyrule Magic sprite names (defined in sprite.cc)
namespace yaze::zelda3 {
extern const char* const kSpriteNames[];
extern const size_t kSpriteNameCount;
}  // namespace yaze::zelda3

namespace yaze {
namespace zelda3 {

// ============================================================================
// Resource Type String Conversion
// ============================================================================

std::string ResourceTypeToString(ResourceType type) {
  switch (type) {
    case ResourceType::kSprite:
      return "sprite";
    case ResourceType::kRoom:
      return "room";
    case ResourceType::kEntrance:
      return "entrance";
    case ResourceType::kItem:
      return "item";
    case ResourceType::kOverlord:
      return "overlord";
    case ResourceType::kOverworldMap:
      return "overworld_map";
    case ResourceType::kMusic:
      return "music";
    case ResourceType::kGraphics:
      return "graphics";
    case ResourceType::kRoomEffect:
      return "room_effect";
    case ResourceType::kRoomTag:
      return "room_tag";
    case ResourceType::kTileType:
      return "tile_type";
  }
  return "unknown";
}

ResourceType StringToResourceType(const std::string& type_str) {
  if (type_str == "sprite")
    return ResourceType::kSprite;
  if (type_str == "room")
    return ResourceType::kRoom;
  if (type_str == "entrance")
    return ResourceType::kEntrance;
  if (type_str == "item")
    return ResourceType::kItem;
  if (type_str == "overlord")
    return ResourceType::kOverlord;
  if (type_str == "overworld_map")
    return ResourceType::kOverworldMap;
  if (type_str == "music")
    return ResourceType::kMusic;
  if (type_str == "graphics")
    return ResourceType::kGraphics;
  if (type_str == "room_effect")
    return ResourceType::kRoomEffect;
  if (type_str == "room_tag")
    return ResourceType::kRoomTag;
  if (type_str == "tile_type")
    return ResourceType::kTileType;
  // Default fallback
  return ResourceType::kSprite;
}

// ============================================================================
// Global Provider Instance
// ============================================================================

ResourceLabelProvider& GetResourceLabels() {
  static ResourceLabelProvider instance;
  return instance;
}

// ============================================================================
// ResourceLabelProvider Implementation
// ============================================================================

std::string ResourceLabelProvider::GetLabel(ResourceType type, int id) const {
  std::string type_str = ResourceTypeToString(type);

  // 1. Check project-specific labels first
  if (project_labels_) {
    auto type_it = project_labels_->find(type_str);
    if (type_it != project_labels_->end()) {
      auto label_it = type_it->second.find(std::to_string(id));
      if (label_it != type_it->second.end() && !label_it->second.empty()) {
        return label_it->second;
      }
    }
  }

  // 2. For sprites, check Hyrule Magic names if preferred
  if (type == ResourceType::kSprite && prefer_hmagic_) {
    std::string hmagic = GetHMagicLabel(type, id);
    if (!hmagic.empty()) {
      return hmagic;
    }
  }

  // 3. Fall back to vanilla labels
  return GetVanillaLabel(type, id);
}

std::string ResourceLabelProvider::GetLabel(const std::string& type_str,
                                            int id) const {
  ResourceType type = StringToResourceType(type_str);
  return GetLabel(type, id);
}

void ResourceLabelProvider::SetProjectLabel(ResourceType type, int id,
                                            const std::string& label) {
  if (!project_labels_) {
    return;
  }
  std::string type_str = ResourceTypeToString(type);
  (*project_labels_)[type_str][std::to_string(id)] = label;
}

void ResourceLabelProvider::ClearProjectLabel(ResourceType type, int id) {
  if (!project_labels_) {
    return;
  }
  std::string type_str = ResourceTypeToString(type);
  auto type_it = project_labels_->find(type_str);
  if (type_it != project_labels_->end()) {
    type_it->second.erase(std::to_string(id));
  }
}

bool ResourceLabelProvider::HasProjectLabel(ResourceType type, int id) const {
  if (!project_labels_) {
    return false;
  }
  std::string type_str = ResourceTypeToString(type);
  auto type_it = project_labels_->find(type_str);
  if (type_it == project_labels_->end()) {
    return false;
  }
  return type_it->second.find(std::to_string(id)) != type_it->second.end();
}

std::string ResourceLabelProvider::GetVanillaLabel(ResourceType type,
                                                   int id) const {
  switch (type) {
    case ResourceType::kSprite: {
      const auto& names = Zelda3Labels::GetSpriteNames();
      if (id >= 0 && static_cast<size_t>(id) < names.size()) {
        return names[id];
      }
      return absl::StrFormat("Sprite %02X", id);
    }
    case ResourceType::kRoom: {
      const auto& names = Zelda3Labels::GetRoomNames();
      if (id >= 0 && static_cast<size_t>(id) < names.size()) {
        return names[id];
      }
      return absl::StrFormat("Room %03X", id);
    }
    case ResourceType::kEntrance: {
      const auto& names = Zelda3Labels::GetEntranceNames();
      if (id >= 0 && static_cast<size_t>(id) < names.size()) {
        return names[id];
      }
      return absl::StrFormat("Entrance %02X", id);
    }
    case ResourceType::kItem: {
      const auto& names = Zelda3Labels::GetItemNames();
      if (id >= 0 && static_cast<size_t>(id) < names.size()) {
        return names[id];
      }
      return absl::StrFormat("Item %02X", id);
    }
    case ResourceType::kOverlord: {
      const auto& names = Zelda3Labels::GetOverlordNames();
      if (id >= 0 && static_cast<size_t>(id) < names.size()) {
        return names[id];
      }
      return absl::StrFormat("Overlord %02X", id);
    }
    case ResourceType::kOverworldMap: {
      const auto& names = Zelda3Labels::GetOverworldMapNames();
      if (id >= 0 && static_cast<size_t>(id) < names.size()) {
        return names[id];
      }
      return absl::StrFormat("Map %02X", id);
    }
    case ResourceType::kMusic: {
      const auto& names = Zelda3Labels::GetMusicTrackNames();
      if (id >= 0 && static_cast<size_t>(id) < names.size()) {
        return names[id];
      }
      return absl::StrFormat("Music %02X", id);
    }
    case ResourceType::kGraphics: {
      const auto& names = Zelda3Labels::GetGraphicsSheetNames();
      if (id >= 0 && static_cast<size_t>(id) < names.size()) {
        return names[id];
      }
      return absl::StrFormat("GFX %02X", id);
    }
    case ResourceType::kRoomEffect: {
      const auto& names = Zelda3Labels::GetRoomEffectNames();
      if (id >= 0 && static_cast<size_t>(id) < names.size()) {
        return names[id];
      }
      return absl::StrFormat("Effect %02X", id);
    }
    case ResourceType::kRoomTag: {
      const auto& names = Zelda3Labels::GetRoomTagNames();
      if (id >= 0 && static_cast<size_t>(id) < names.size()) {
        return names[id];
      }
      return absl::StrFormat("Tag %02X", id);
    }
    case ResourceType::kTileType: {
      const auto& names = Zelda3Labels::GetTileTypeNames();
      if (id >= 0 && static_cast<size_t>(id) < names.size()) {
        return names[id];
      }
      return absl::StrFormat("Tile %02X", id);
    }
  }
  return absl::StrFormat("Unknown %02X", id);
}

std::string ResourceLabelProvider::GetHMagicLabel(ResourceType type,
                                                  int id) const {
  // Hyrule Magic names are only available for sprites
  if (type != ResourceType::kSprite) {
    return "";
  }

  // Use the kSpriteNames array from sprite_names.h
  if (id >= 0 && static_cast<size_t>(id) < kSpriteNameCount) {
    return kSpriteNames[id];
  }
  return "";
}

int ResourceLabelProvider::GetResourceCount(ResourceType type) const {
  switch (type) {
    case ResourceType::kSprite:
      return 256;
    case ResourceType::kRoom:
      return 297;
    case ResourceType::kEntrance:
      return 133;
    case ResourceType::kItem:
      return static_cast<int>(Zelda3Labels::GetItemNames().size());
    case ResourceType::kOverlord:
      return static_cast<int>(Zelda3Labels::GetOverlordNames().size());
    case ResourceType::kOverworldMap:
      return 160;
    case ResourceType::kMusic:
      return static_cast<int>(Zelda3Labels::GetMusicTrackNames().size());
    case ResourceType::kGraphics:
      return static_cast<int>(Zelda3Labels::GetGraphicsSheetNames().size());
    case ResourceType::kRoomEffect:
      return static_cast<int>(Zelda3Labels::GetRoomEffectNames().size());
    case ResourceType::kRoomTag:
      return static_cast<int>(Zelda3Labels::GetRoomTagNames().size());
    case ResourceType::kTileType:
      return static_cast<int>(Zelda3Labels::GetTileTypeNames().size());
  }
  return 0;
}

const ResourceLabelProvider::LabelMap*
ResourceLabelProvider::GetProjectLabelsForType(ResourceType type) const {
  if (!project_labels_) {
    return nullptr;
  }
  std::string type_str = ResourceTypeToString(type);
  auto it = project_labels_->find(type_str);
  if (it == project_labels_->end()) {
    return nullptr;
  }
  return &it->second;
}

void ResourceLabelProvider::ClearAllProjectLabels() {
  if (project_labels_) {
    project_labels_->clear();
  }
}

// ============================================================================
// ZScream DefaultNames.txt Import/Export
// ============================================================================

absl::Status ResourceLabelProvider::ImportFromZScreamFormat(
    const std::string& content) {
  if (!project_labels_) {
    return absl::FailedPreconditionError(
        "Project labels not initialized. Open a project first.");
  }

  std::istringstream stream(content);
  std::string line;
  std::string current_section;
  int line_index = 0;
  int line_number = 0;

  while (std::getline(stream, line)) {
    line_number++;

    // Trim whitespace
    std::string trimmed = std::string(absl::StripAsciiWhitespace(line));

    // Skip empty lines and comments
    if (trimmed.empty() || trimmed.substr(0, 2) == "//") {
      continue;
    }

    // Check for section headers
    if (trimmed.front() == '[' && trimmed.back() == ']') {
      current_section = trimmed.substr(1, trimmed.length() - 2);
      line_index = 0;  // Reset line index for new section
      continue;
    }

    // Parse the line based on current section
    if (!current_section.empty()) {
      ParseZScreamLine(trimmed, current_section, line_index);
    }
  }

  return absl::OkStatus();
}

bool ResourceLabelProvider::ParseZScreamLine(const std::string& line,
                                             const std::string& section,
                                             int& line_index) {
  if (!project_labels_) {
    return false;
  }

  // Determine resource type and format based on section
  std::string resource_type;
  bool has_hex_prefix = false;

  if (section == "Sprites Names") {
    resource_type = "sprite";
    has_hex_prefix = true;  // Format: "00 Raven"
  } else if (section == "Rooms Names") {
    resource_type = "room";
    has_hex_prefix = false;  // Format: just "Room Name" by line order
  } else if (section == "Chests Items") {
    resource_type = "item";
    has_hex_prefix = true;  // Format: "00 Fighter sword and shield"
  } else if (section == "Tags Names") {
    resource_type = "room_tag";
    has_hex_prefix = false;  // Format: just "Tag Name" by line order
  } else {
    // Unknown section, skip
    return false;
  }

  std::string id_str;
  std::string label;

  if (has_hex_prefix) {
    // Format: "XX Label Name" where XX is hex
    size_t space_pos = line.find(' ');
    if (space_pos == std::string::npos || space_pos < 2) {
      // No space found or too short for hex prefix
      return false;
    }

    std::string hex_str = line.substr(0, space_pos);
    label = line.substr(space_pos + 1);

    // Parse hex to int
    try {
      int id = std::stoi(hex_str, nullptr, 16);
      id_str = std::to_string(id);
    } catch (...) {
      return false;
    }
  } else {
    // Line index determines the ID
    id_str = std::to_string(line_index);
    label = line;
    line_index++;
  }

  // Trim the label
  label = std::string(absl::StripAsciiWhitespace(label));

  // Store the label
  if (!label.empty()) {
    (*project_labels_)[resource_type][id_str] = label;
  }

  return true;
}

std::string ResourceLabelProvider::ExportToZScreamFormat() const {
  std::ostringstream output;

  output << "//Do not use brackets [] in naming\n";

  // Export sprites
  output << "[Sprites Names]\n";
  for (int i = 0; i < 256; ++i) {
    std::string label = GetLabel(ResourceType::kSprite, i);
    output << absl::StrFormat("%02X %s\n", i, label);
  }

  // Export rooms
  output << "\n[Rooms Names]\n";
  for (int i = 0; i < 297; ++i) {
    std::string label = GetLabel(ResourceType::kRoom, i);
    output << label << "\n";
  }

  // Export items
  output << "\n[Chests Items]\n";
  int item_count = GetResourceCount(ResourceType::kItem);
  for (int i = 0; i < item_count; ++i) {
    std::string label = GetLabel(ResourceType::kItem, i);
    output << absl::StrFormat("%02X %s\n", i, label);
  }

  // Export room tags
  output << "\n[Tags Names]\n";
  int tag_count = GetResourceCount(ResourceType::kRoomTag);
  for (int i = 0; i < tag_count; ++i) {
    std::string label = GetLabel(ResourceType::kRoomTag, i);
    output << label << "\n";
  }

  return output.str();
}

// ============================================================================
// Oracle of Secrets Sprite Registry Import
// ============================================================================

absl::Status ResourceLabelProvider::ImportOracleSpriteRegistry(
    const std::string& csv_content) {
  // Create local storage if project_labels_ not set
  static ProjectLabels local_labels;
  if (!project_labels_) {
    project_labels_ = &local_labels;
  }

  std::istringstream stream(csv_content);
  std::string line;
  int line_number = 0;
  int imported_count = 0;

  while (std::getline(stream, line)) {
    line_number++;

    // Trim whitespace
    std::string trimmed = std::string(absl::StripAsciiWhitespace(line));

    // Skip empty lines and header row
    if (trimmed.empty() || line_number == 1) {
      continue;
    }

    // Parse CSV: name,id,paths,group,notes,allow_dupe
    std::vector<std::string> fields = absl::StrSplit(trimmed, ',');
    if (fields.size() < 2) {
      continue;  // Need at least name and id
    }

    std::string name = std::string(absl::StripAsciiWhitespace(fields[0]));
    std::string id_str = std::string(absl::StripAsciiWhitespace(fields[1]));

    // Parse sprite ID (format: $XX or 0xXX or just XX)
    int sprite_id = -1;
    if (id_str.empty()) {
      continue;
    }

    // Handle $XX format
    if (id_str[0] == '$') {
      id_str = id_str.substr(1);
    }
    // Handle 0xXX format
    if (id_str.size() >= 2 && id_str[0] == '0' &&
        (id_str[1] == 'x' || id_str[1] == 'X')) {
      id_str = id_str.substr(2);
    }

    try {
      sprite_id = std::stoi(id_str, nullptr, 16);
    } catch (...) {
      continue;  // Skip invalid IDs
    }

    if (sprite_id < 0 || sprite_id > 255) {
      continue;  // Invalid sprite ID range
    }

    // Store the sprite name
    (*project_labels_)["sprite"][std::to_string(sprite_id)] = name;
    imported_count++;
  }

  if (imported_count == 0) {
    return absl::InvalidArgumentError(
        "No valid sprite entries found in registry CSV");
  }

  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
