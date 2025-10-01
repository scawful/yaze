#include "cli/z3ed.h"
#include "app/zelda3/overworld/overworld.h"
#include "absl/flags/flag.h"
#include "absl/flags/declare.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {

absl::Status OverworldGetTile::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 3) {
    return absl::InvalidArgumentError("Usage: overworld get-tile --map <map_id> --x <x> --y <y>");
  }

  // TODO: Implement proper argument parsing
  int map_id = std::stoi(arg_vec[0]);
  int x = std::stoi(arg_vec[1]);
  int y = std::stoi(arg_vec[2]);

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  rom_.LoadFromFile(rom_file);
  if (!rom_.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld(&rom_);
  overworld.Load(&rom_);

  uint16_t tile = overworld.GetTile(x, y);

  std::cout << "Tile at (" << x << ", " << y << ") on map " << map_id << " is: 0x" << std::hex << tile << std::endl;

  return absl::OkStatus();
}

absl::Status OverworldSetTile::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 4) {
    return absl::InvalidArgumentError("Usage: overworld set-tile --map <map_id> --x <x> --y <y> --tile <tile_id>");
  }

  // TODO: Implement proper argument parsing
  int map_id = std::stoi(arg_vec[0]);
  int x = std::stoi(arg_vec[1]);
  int y = std::stoi(arg_vec[2]);
  int tile_id = std::stoi(arg_vec[3], nullptr, 16);

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  rom_.LoadFromFile(rom_file);
  if (!rom_.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld(&rom_);
  overworld.Load(&rom_);

  // TODO: Implement the actual set_tile method in Overworld class
  // overworld.SetTile(x, y, tile_id);

  // rom_.SaveToFile({.filename = rom_file});

  std::cout << "Set tile at (" << x << ", " << y << ") on map " << map_id << " to: 0x" << std::hex << tile_id << std::endl;
  std::cout << "(Not actually implemented yet)" << std::endl;

  return absl::OkStatus();
}

} // namespace cli
} // namespace yaze
