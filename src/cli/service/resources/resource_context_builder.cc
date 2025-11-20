#include "cli/service/resources/resource_context_builder.h"

#include <sstream>

namespace yaze {
namespace cli {

std::string ResourceContextBuilder::GetCommonTile16Reference() {
  std::ostringstream oss;
  oss << "Common Tile16s:\n";
  oss << "  - 0x020: Grass\n";
  oss << "  - 0x022: Dirt\n";
  oss << "  - 0x02E: Tree\n";
  oss << "  - 0x003: Bush\n";
  oss << "  - 0x004: Rock\n";
  oss << "  - 0x021: Flower\n";
  oss << "  - 0x023: Sand\n";
  oss << "  - 0x14C: Water (top edge)\n";
  oss << "  - 0x14D: Water (middle)\n";
  oss << "  - 0x14E: Water (bottom edge)\n";
  return oss.str();
}

std::string ResourceContextBuilder::ExtractOverworldLabels() {
  if (!rom_ || !rom_->is_loaded()) {
    return "Overworld Maps: (ROM not loaded)\n";
  }

  auto* label_mgr = rom_->resource_label();
  if (!label_mgr || !label_mgr->labels_loaded_) {
    return "Overworld Maps: (No labels file loaded)\n";
  }

  std::ostringstream oss;
  oss << "Overworld Maps:\n";

  // Check if "overworld" labels exist
  auto it = label_mgr->labels_.find("overworld");
  if (it != label_mgr->labels_.end()) {
    for (const auto& [key, value] : it->second) {
      oss << "  - " << key << ": \"" << value << "\"\n";
    }
  } else {
    // Provide defaults
    oss << "  - 0: \"Light World\"\n";
    oss << "  - 1: \"Dark World\"\n";
    oss << "  - 3: \"Desert\"\n";
  }

  return oss.str();
}

std::string ResourceContextBuilder::ExtractDungeonLabels() {
  if (!rom_ || !rom_->is_loaded()) {
    return "Dungeons: (ROM not loaded)\n";
  }

  auto* label_mgr = rom_->resource_label();
  if (!label_mgr || !label_mgr->labels_loaded_) {
    return "Dungeons: (No labels file loaded)\n";
  }

  std::ostringstream oss;
  oss << "Dungeons:\n";

  // Check if "dungeon" labels exist
  auto it = label_mgr->labels_.find("dungeon");
  if (it != label_mgr->labels_.end()) {
    for (const auto& [key, value] : it->second) {
      oss << "  - " << key << ": \"" << value << "\"\n";
    }
  } else {
    // Provide vanilla defaults
    oss << "  - 0x00: \"Hyrule Castle\"\n";
    oss << "  - 0x02: \"Eastern Palace\"\n";
    oss << "  - 0x04: \"Desert Palace\"\n";
    oss << "  - 0x06: \"Tower of Hera\"\n";
  }

  return oss.str();
}

std::string ResourceContextBuilder::ExtractEntranceLabels() {
  if (!rom_ || !rom_->is_loaded()) {
    return "Entrances: (ROM not loaded)\n";
  }

  auto* label_mgr = rom_->resource_label();
  if (!label_mgr || !label_mgr->labels_loaded_) {
    return "Entrances: (No labels file loaded)\n";
  }

  std::ostringstream oss;
  oss << "Entrances:\n";

  // Check if "entrance" labels exist
  auto it = label_mgr->labels_.find("entrance");
  if (it != label_mgr->labels_.end()) {
    for (const auto& [key, value] : it->second) {
      oss << "  - " << key << ": \"" << value << "\"\n";
    }
  } else {
    // Provide vanilla defaults
    oss << "  - 0x00: \"Link's House\"\n";
    oss << "  - 0x01: \"Sanctuary\"\n";
  }

  return oss.str();
}

std::string ResourceContextBuilder::ExtractRoomLabels() {
  if (!rom_ || !rom_->is_loaded()) {
    return "Rooms: (ROM not loaded)\n";
  }

  auto* label_mgr = rom_->resource_label();
  if (!label_mgr || !label_mgr->labels_loaded_) {
    return "Rooms: (No labels file loaded)\n";
  }

  std::ostringstream oss;
  oss << "Rooms:\n";

  // Check if "room" labels exist
  auto it = label_mgr->labels_.find("room");
  if (it != label_mgr->labels_.end()) {
    for (const auto& [key, value] : it->second) {
      oss << "  - " << key << ": \"" << value << "\"\n";
    }
  } else {
    oss << "  (No room labels defined)\n";
  }

  return oss.str();
}

std::string ResourceContextBuilder::ExtractSpriteLabels() {
  if (!rom_ || !rom_->is_loaded()) {
    return "Sprites: (ROM not loaded)\n";
  }

  auto* label_mgr = rom_->resource_label();
  if (!label_mgr || !label_mgr->labels_loaded_) {
    return "Sprites: (No labels file loaded)\n";
  }

  std::ostringstream oss;
  oss << "Sprites:\n";

  // Check if "sprite" labels exist
  auto it = label_mgr->labels_.find("sprite");
  if (it != label_mgr->labels_.end()) {
    for (const auto& [key, value] : it->second) {
      oss << "  - " << key << ": \"" << value << "\"\n";
    }
  } else {
    // Provide vanilla defaults
    oss << "  - 0x00: \"Soldier\"\n";
    oss << "  - 0x01: \"Octorok\"\n";
  }

  return oss.str();
}

absl::StatusOr<std::string> ResourceContextBuilder::BuildResourceContext() {
  if (!rom_) {
    return absl::InvalidArgumentError("ROM pointer is null");
  }

  std::ostringstream context;

  context << "=== AVAILABLE RESOURCES ===\n\n";

  // Add overworld maps
  context << ExtractOverworldLabels() << "\n";

  // Add dungeons
  context << ExtractDungeonLabels() << "\n";

  // Add entrances
  context << ExtractEntranceLabels() << "\n";

  // Add rooms (if any)
  context << ExtractRoomLabels() << "\n";

  // Add sprites
  context << ExtractSpriteLabels() << "\n";

  // Add common tile16 reference
  context << GetCommonTile16Reference() << "\n";

  context << "=== INSTRUCTIONS ===\n";
  context << "1. Use the resource labels when they're available\n";
  context << "2. If a user refers to a custom name, check the labels above\n";
  context << "3. Always provide tile16 IDs as hex values (0x###)\n";
  context << "4. Explain which resources you're using in your reasoning\n";

  return context.str();
}

absl::StatusOr<std::map<std::string, std::string>>
ResourceContextBuilder::GetLabels(const std::string& category) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  auto* label_mgr = rom_->resource_label();
  if (!label_mgr) {
    return absl::FailedPreconditionError("No resource label manager");
  }

  if (!label_mgr->labels_loaded_) {
    return absl::FailedPreconditionError("No labels file loaded");
  }

  std::map<std::string, std::string> result;

  auto it = label_mgr->labels_.find(category);
  if (it != label_mgr->labels_.end()) {
    for (const auto& [key, value] : it->second) {
      result[key] = value;
    }
  }

  return result;
}

absl::StatusOr<std::string> ResourceContextBuilder::ExportToJson() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  auto* label_mgr = rom_->resource_label();
  if (!label_mgr || !label_mgr->labels_loaded_) {
    return absl::InvalidArgumentError("No labels file loaded");
  }

  std::ostringstream json;
  json << "{\n";

  bool first_category = true;
  for (const auto& [category, labels] : label_mgr->labels_) {
    if (!first_category) json << ",\n";
    first_category = false;

    json << "  \"" << category << "\": {\n";

    bool first_label = true;
    for (const auto& [key, value] : labels) {
      if (!first_label) json << ",\n";
      first_label = false;

      json << "    \"" << key << "\": \"" << value << "\"";
    }

    json << "\n  }";
  }

  json << "\n}\n";

  return json.str();
}

}  // namespace cli
}  // namespace yaze
