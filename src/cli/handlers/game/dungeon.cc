#include "cli/cli.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/room.h"
#include "absl/flags/flag.h"
#include "absl/flags/declare.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {

// Legacy DungeonExport class removed - using new CommandHandler system
// This implementation should be moved to DungeonExportCommandHandler
absl::Status HandleDungeonExportLegacy(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 1) {
    return absl::InvalidArgumentError("Usage: dungeon export <room_id>");
  }

  int room_id = std::stoi(arg_vec[0]);
  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  Rom rom;
  rom.LoadFromFile(rom_file);
  if (!rom.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::DungeonEditorSystem dungeon_editor(&rom);
  auto room_or = dungeon_editor.GetRoom(room_id);
  if (!room_or.ok()) {
      return room_or.status();
  }
  zelda3::Room room = room_or.value();

  std::cout << "Room ID: " << room_id << std::endl;
  std::cout << "Blockset: " << (int)room.blockset << std::endl;
  std::cout << "Spriteset: " << (int)room.spriteset << std::endl;
  std::cout << "Palette: " << (int)room.palette << std::endl;
  std::cout << "Layout: " << (int)room.layout << std::endl;

  return absl::OkStatus();
}

// Legacy DungeonListObjects class removed - using new CommandHandler system
// This implementation should be moved to DungeonListObjectsCommandHandler
absl::Status HandleDungeonListObjectsLegacy(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 1) {
    return absl::InvalidArgumentError("Usage: dungeon list-objects <room_id>");
  }

  int room_id = std::stoi(arg_vec[0]);
  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  Rom rom;
  rom.LoadFromFile(rom_file);
  if (!rom.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::DungeonEditorSystem dungeon_editor(&rom);
  auto room_or = dungeon_editor.GetRoom(room_id);
  if (!room_or.ok()) {
      return room_or.status();
  }
  zelda3::Room room = room_or.value();
  room.LoadObjects();

  std::cout << "Objects in Room " << room_id << ":" << std::endl;
  for (const auto& obj : room.GetTileObjects()) {
    std::cout << absl::StrFormat("  - ID: 0x%04X, Pos: (%d, %d), Size: 0x%02X, Layer: %d\n", 
                                 obj.id_, obj.x_, obj.y_, obj.size_, obj.layer_);
  }

  return absl::OkStatus();
}

} // namespace cli
} // namespace yaze
