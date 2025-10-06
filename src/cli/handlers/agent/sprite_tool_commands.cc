#include "cli/handlers/agent/commands.h"

#include <iostream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "app/rom.h"

namespace yaze {
namespace cli {
namespace agent {

absl::Status HandleSpriteListCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  std::string format = "json";
  std::string type = "all";  // all, enemy, npc, boss
  int limit = 50;
  
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--format") {
      if (i + 1 < arg_vec.size()) {
        format = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    } else if (token == "--type") {
      if (i + 1 < arg_vec.size()) {
        type = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--type=")) {
      type = token.substr(7);
    } else if (token == "--limit") {
      if (i + 1 < arg_vec.size()) {
        absl::SimpleAtoi(arg_vec[++i], &limit);
      }
    } else if (absl::StartsWith(token, "--limit=")) {
      absl::SimpleAtoi(token.substr(8), &limit);
    }
  }

  // Sample sprite data
  struct Sprite {
    int id;
    std::string name;
    std::string type;
    int hp;
  };

  std::vector<Sprite> sprites = {
    {0x00, "Raven", "Enemy", 1},
    {0x01, "Vulture", "Enemy", 2},
    {0x04, "Correct Pull Switch", "Object", 0},
    {0x08, "Octorok", "Enemy", 2},
    {0x09, "Moldorm (Boss)", "Boss", 6},
    {0x0A, "Octorok (Four Way)", "Enemy", 4},
    {0x13, "Mini Helmasaur", "Enemy", 2},
    {0x15, "Antifairy", "Enemy", 0},
    {0x1A, "Hoarder", "Enemy", 4},
    {0x22, "Bari", "Enemy", 1},
    {0x41, "Armos Knight (Boss)", "Boss", 12},
    {0x51, "Armos", "Enemy", 3},
    {0x53, "Lanmolas (Boss)", "Boss", 16},
    {0x6A, "Lynel", "Enemy", 8},
    {0x7C, "Green Eyegore", "Enemy", 8},
    {0x7D, "Red Eyegore", "Enemy", 12},
    {0x81, "Zora", "Enemy", 6},
    {0x83, "Catfish", "NPC", 0},
    {0x91, "Ganon", "Boss", 255},
    {0xAE, "Old Man", "NPC", 0},
  };

  // Filter by type if specified
  std::vector<Sprite> filtered;
  for (const auto& sprite : sprites) {
    if (type == "all" || 
        (type == "enemy" && sprite.type == "Enemy") ||
        (type == "boss" && sprite.type == "Boss") ||
        (type == "npc" && sprite.type == "NPC") ||
        (type == "object" && sprite.type == "Object")) {
      filtered.push_back(sprite);
      if (filtered.size() >= static_cast<size_t>(limit)) {
        break;
      }
    }
  }

  if (format == "json") {
    std::cout << "{\n";
    std::cout << "  \"sprites\": [\n";
    for (size_t i = 0; i < filtered.size(); ++i) {
      const auto& sprite = filtered[i];
      std::cout << "    {\n";
      std::cout << "      \"id\": \"0x" << std::hex << std::uppercase 
                << sprite.id << std::dec << "\",\n";
      std::cout << "      \"decimal_id\": " << sprite.id << ",\n";
      std::cout << "      \"name\": \"" << sprite.name << "\",\n";
      std::cout << "      \"type\": \"" << sprite.type << "\",\n";
      std::cout << "      \"hp\": " << sprite.hp << "\n";
      std::cout << "    }";
      if (i < filtered.size() - 1) {
        std::cout << ",";
      }
      std::cout << "\n";
    }
    std::cout << "  ],\n";
    std::cout << "  \"total\": " << filtered.size() << ",\n";
    std::cout << "  \"type_filter\": \"" << type << "\",\n";
    std::cout << "  \"rom\": \"" << rom_context->filename() << "\"\n";
    std::cout << "}\n";
  } else {
    std::cout << "Sprites (Type: " << type << "):\n";
    std::cout << "----------------------------------------\n";
    for (const auto& sprite : filtered) {
      std::cout << absl::StrFormat("0x%02X (%3d) | %-25s [%s] HP:%d\n", 
                                   sprite.id, sprite.id, sprite.name, 
                                   sprite.type, sprite.hp);
    }
    std::cout << "----------------------------------------\n";
    std::cout << "Total: " << filtered.size() << " sprites\n";
  }

  return absl::OkStatus();
}

absl::Status HandleSpritePropertiesCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  int sprite_id = -1;
  std::string format = "json";
  
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--id" || token == "--sprite") {
      if (i + 1 < arg_vec.size()) {
        std::string id_str = arg_vec[++i];
        if (absl::StartsWith(id_str, "0x") || absl::StartsWith(id_str, "0X")) {
          sprite_id = std::stoi(id_str, nullptr, 16);
        } else {
          absl::SimpleAtoi(id_str, &sprite_id);
        }
      }
    } else if (absl::StartsWith(token, "--id=") || absl::StartsWith(token, "--sprite=")) {
      std::string id_str = token.substr(token.find('=') + 1);
      if (absl::StartsWith(id_str, "0x") || absl::StartsWith(id_str, "0X")) {
        sprite_id = std::stoi(id_str, nullptr, 16);
      } else {
        absl::SimpleAtoi(id_str, &sprite_id);
      }
    } else if (token == "--format") {
      if (i + 1 < arg_vec.size()) {
        format = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    }
  }

  if (sprite_id < 0) {
    return absl::InvalidArgumentError(
        "Usage: sprite-properties --id <sprite_id> [--format json|text]");
  }

  // Simplified sprite properties
  std::string name = absl::StrFormat("Sprite %d", sprite_id);
  std::string type = "Enemy";
  int hp = 4;
  int damage = 2;
  bool boss = false;
  std::string palette = "enemyGreenPalette";

  // Override for known sprites
  if (sprite_id == 0x08) {
    name = "Octorok";
    hp = 2;
    damage = 1;
  } else if (sprite_id == 0x91) {
    name = "Ganon";
    type = "Boss";
    hp = 255;
    damage = 8;
    boss = true;
    palette = "bossPalette";
  }

  if (format == "json") {
    std::cout << "{\n";
    std::cout << "  \"sprite_id\": \"0x" << std::hex << std::uppercase 
              << sprite_id << std::dec << "\",\n";
    std::cout << "  \"decimal_id\": " << sprite_id << ",\n";
    std::cout << "  \"name\": \"" << name << "\",\n";
    std::cout << "  \"type\": \"" << type << "\",\n";
    std::cout << "  \"hp\": " << hp << ",\n";
    std::cout << "  \"damage\": " << damage << ",\n";
    std::cout << "  \"is_boss\": " << (boss ? "true" : "false") << ",\n";
    std::cout << "  \"palette\": \"" << palette << "\",\n";
    std::cout << "  \"rom\": \"" << rom_context->filename() << "\"\n";
    std::cout << "}\n";
  } else {
    std::cout << "Sprite ID: 0x" << std::hex << std::uppercase 
              << sprite_id << std::dec << " (" << sprite_id << ")\n";
    std::cout << "Name: " << name << "\n";
    std::cout << "Type: " << type << "\n";
    std::cout << "HP: " << hp << "\n";
    std::cout << "Damage: " << damage << "\n";
    std::cout << "Boss: " << (boss ? "Yes" : "No") << "\n";
    std::cout << "Palette: " << palette << "\n";
  }

  return absl::OkStatus();
}

absl::Status HandleSpritePaletteCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  int sprite_id = -1;
  std::string format = "json";
  
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--id" || token == "--sprite") {
      if (i + 1 < arg_vec.size()) {
        std::string id_str = arg_vec[++i];
        if (absl::StartsWith(id_str, "0x") || absl::StartsWith(id_str, "0X")) {
          sprite_id = std::stoi(id_str, nullptr, 16);
        } else {
          absl::SimpleAtoi(id_str, &sprite_id);
        }
      }
    } else if (absl::StartsWith(token, "--id=") || absl::StartsWith(token, "--sprite=")) {
      std::string id_str = token.substr(token.find('=') + 1);
      if (absl::StartsWith(id_str, "0x") || absl::StartsWith(id_str, "0X")) {
        sprite_id = std::stoi(id_str, nullptr, 16);
      } else {
        absl::SimpleAtoi(id_str, &sprite_id);
      }
    } else if (token == "--format") {
      if (i + 1 < arg_vec.size()) {
        format = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    }
  }

  if (sprite_id < 0) {
    return absl::InvalidArgumentError(
        "Usage: sprite-palette --id <sprite_id> [--format json|text]");
  }

  // Simplified palette data
  std::vector<std::string> colors = {
    "#FF0000", "#00FF00", "#0000FF", "#FFFF00",
    "#FF00FF", "#00FFFF", "#FFFFFF", "#000000"
  };

  std::cout << "{\n";
  std::cout << "  \"sprite_id\": \"0x" << std::hex << std::uppercase 
            << sprite_id << std::dec << "\",\n";
  std::cout << "  \"decimal_id\": " << sprite_id << ",\n";
  std::cout << "  \"palette\": [\n";
  for (size_t i = 0; i < colors.size(); ++i) {
    std::cout << "    \"" << colors[i] << "\"";
    if (i < colors.size() - 1) {
      std::cout << ",";
    }
    std::cout << "\n";
  }
  std::cout << "  ],\n";
  std::cout << "  \"rom\": \"" << rom_context->filename() << "\"\n";
  std::cout << "}\n";

  return absl::OkStatus();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze

