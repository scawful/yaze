#include "cli/handlers/graphics/sprite_commands.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace cli {
namespace handlers {

absl::Status SpriteListCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  
  auto limit = parser.GetInt("limit").value_or(256);
  auto type = parser.GetString("type").value_or("all");
  
  formatter.BeginObject("Sprite List");
  formatter.AddField("total_sprites", 256);
  formatter.AddField("display_limit", limit);
  formatter.AddField("filter_type", type);
  
  formatter.BeginArray("sprites");
  
  // Use the sprite names from the sprite system
  for (int i = 0; i < std::min(limit, 256); i++) {
    std::string sprite_name = zelda3::kSpriteDefaultNames[i];
    std::string sprite_entry = absl::StrFormat("0x%02X: %s", i, sprite_name);
    formatter.AddArrayItem(sprite_entry);
  }
  
  formatter.EndArray();
  formatter.EndObject();
  
  return absl::OkStatus();
}

absl::Status SpritePropertiesCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  
  auto id_str = parser.GetString("id").value();
  
  int sprite_id;
  if (!absl::SimpleHexAtoi(id_str, &sprite_id) && 
      !absl::SimpleAtoi(id_str, &sprite_id)) {
    return absl::InvalidArgumentError(
        "Invalid sprite ID format. Must be hex (0xNN) or decimal.");
  }
  
  if (sprite_id < 0 || sprite_id > 255) {
    return absl::InvalidArgumentError(
        "Sprite ID must be between 0 and 255.");
  }
  
  formatter.BeginObject("Sprite Properties");
  formatter.AddHexField("sprite_id", sprite_id, 2);
  
  // Get sprite name
  std::string sprite_name = zelda3::kSpriteDefaultNames[sprite_id];
  formatter.AddField("name", sprite_name);
  
  // Add basic sprite properties
  // Note: Full sprite properties would require loading sprite data from ROM
  formatter.BeginObject("properties");
  formatter.AddField("type", "standard");
  formatter.AddField("is_boss", sprite_id == 0x09 || sprite_id == 0x1A || 
                               sprite_id == 0x1E || sprite_id == 0x1F || 
                               sprite_id == 0xCE || sprite_id == 0xD6);
  formatter.AddField("is_overlord", sprite_id <= 0x1A);
  formatter.AddField("description", "Sprite properties would be loaded from ROM data");
  formatter.EndObject();
  
  formatter.EndObject();
  
  return absl::OkStatus();
}

absl::Status SpritePaletteCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  
  auto id_str = parser.GetString("id").value();
  
  int sprite_id;
  if (!absl::SimpleHexAtoi(id_str, &sprite_id) && 
      !absl::SimpleAtoi(id_str, &sprite_id)) {
    return absl::InvalidArgumentError(
        "Invalid sprite ID format. Must be hex (0xNN) or decimal.");
  }
  
  if (sprite_id < 0 || sprite_id > 255) {
    return absl::InvalidArgumentError(
        "Sprite ID must be between 0 and 255.");
  }
  
  formatter.BeginObject("Sprite Palette");
  formatter.AddHexField("sprite_id", sprite_id, 2);
  
  std::string sprite_name = zelda3::kSpriteDefaultNames[sprite_id];
  formatter.AddField("name", sprite_name);
  
  // Note: Actual palette data would need to be loaded from ROM
  formatter.BeginObject("palette_info");
  formatter.AddField("palette_group", "Unknown - requires ROM analysis");
  formatter.AddField("palette_index", "Unknown - requires ROM analysis");
  formatter.AddField("color_count", 16);
  formatter.EndObject();
  
  formatter.BeginArray("colors");
  formatter.AddArrayItem("Palette colors would be loaded from ROM data");
  formatter.EndArray();
  
  formatter.EndObject();
  
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

