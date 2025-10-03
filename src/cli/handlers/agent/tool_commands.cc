#include "cli/handlers/agent/commands.h"

#include <iostream>
#include <string>
#include <utility>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room.h"
#include "cli/service/resources/resource_context_builder.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {
namespace agent {

namespace {

absl::StatusOr<Rom> LoadRomFromFlag() {
  std::string rom_path = absl::GetFlag(FLAGS_rom);
  if (rom_path.empty()) {
    return absl::FailedPreconditionError(
        "No ROM loaded. Use --rom=<path> to specify ROM file.");
  }

  Rom rom;
  auto status = rom.LoadFromFile(rom_path);
  if (!status.ok()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Failed to load ROM from '%s': %s", rom_path, status.message()));
  }

  return rom;
}

}  // namespace

absl::Status HandleResourceListCommand(
    const std::vector<std::string>& arg_vec) {
  std::string type;
  std::string format = "table";

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--type") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--type requires a value.");
      }
      type = arg_vec[++i];
    } else if (absl::StartsWith(token, "--type=")) {
      type = token.substr(7);
    } else if (token == "--format") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--format requires a value.");
      }
      format = arg_vec[++i];
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    }
  }

  if (type.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent resource-list --type <type> [--format <table|json>]");
  }

  auto rom_or = LoadRomFromFlag();
  if (!rom_or.ok()) {
    return rom_or.status();
  }
  Rom rom = std::move(rom_or.value());

  ResourceContextBuilder context_builder(&rom);
  auto labels_or = context_builder.GetLabels(type);
  if (!labels_or.ok()) {
    return labels_or.status();
  }
  auto labels = std::move(labels_or.value());

  if (format == "json") {
    std::cout << "{\n";
    bool first = true;
    for (const auto& [key, value] : labels) {
      if (!first) {
        std::cout << ",\n";
      }
      std::cout << "  \"" << key << "\": \"" << value << "\"";
      first = false;
    }
    std::cout << "\n}\n";
  } else {
    std::cout << "=== " << absl::AsciiStrToUpper(type) << " Labels ===\n";
    for (const auto& [key, value] : labels) {
      std::cout << absl::StrFormat("  %-10s : %s\n", key, value);
    }
  }

  return absl::OkStatus();
}

absl::Status HandleDungeonListSpritesCommand(
    const std::vector<std::string>& arg_vec) {
  std::string room_id_str;
  std::string format = "table";

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--room") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--room requires a value.");
      }
      room_id_str = arg_vec[++i];
    } else if (absl::StartsWith(token, "--room=")) {
      room_id_str = token.substr(7);
    } else if (token == "--format") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--format requires a value.");
      }
      format = arg_vec[++i];
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    }
  }

  if (room_id_str.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent dungeon-list-sprites --room <id> [--format <table|json>]");
  }

  int room_id;
  if (!absl::SimpleHexAtoi(room_id_str, &room_id)) {
    return absl::InvalidArgumentError(
        "Invalid room ID format. Must be hex.");
  }

  auto rom_or = LoadRomFromFlag();
  if (!rom_or.ok()) {
    return rom_or.status();
  }
  Rom rom = std::move(rom_or.value());

  auto room = zelda3::LoadRoomFromRom(&rom, room_id);
  const auto& sprites = room.GetSprites();

  if (format == "json") {
    std::cout << "[\n";
    for (size_t i = 0; i < sprites.size(); ++i) {
      const auto& sprite = sprites[i];
      std::cout << "  {\n";
      std::cout << "    \"id\": " << sprite.id() << ",\n";
      std::cout << "    \"x\": " << sprite.x() << ",\n";
      std::cout << "    \"y\": " << sprite.y() << "\n";
      std::cout << "  }" << (i + 1 == sprites.size() ? "" : ",") << "\n";
    }
    std::cout << "]\n";
  } else {
    std::cout << "=== Sprites in Room " << room_id_str << " ===\n";
    std::cout << absl::StrFormat("%-10s %-5s %-5s\n", "ID (Hex)", "X", "Y");
    std::cout << std::string(22, '-') << "\n";
    for (const auto& sprite : sprites) {
      std::cout << absl::StrFormat("0x%-8X %-5d %-5d\n", sprite.id(),
                                   sprite.x(), sprite.y());
    }
  }

  return absl::OkStatus();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
