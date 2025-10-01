#include "cli/z3ed.h"
#include "app/zelda3/dungeon/dungeon_editor_system.h"

namespace yaze {
namespace cli {

absl::Status DungeonExport::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 1) {
    return absl::InvalidArgumentError("Usage: dungeon export <room_id>");
  }

  int room_id = std::stoi(arg_vec[0]);

  // TODO: Implement dungeon export logic
  std::cout << "Dungeon export for room " << room_id << " not yet implemented." << std::endl;

  return absl::OkStatus();
}

} // namespace cli
} // namespace yaze
