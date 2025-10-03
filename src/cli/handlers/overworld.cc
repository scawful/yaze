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

  auto load_status = rom_.LoadFromFile(rom_file);
  if (!load_status.ok()) {
    return load_status;
  }
  if (!rom_.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld(&rom_);
  auto ow_status = overworld.Load(&rom_);
  if (!ow_status.ok()) {
    return ow_status;
  }

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

  auto load_status = rom_.LoadFromFile(rom_file);
  if (!load_status.ok()) {
    return load_status;
  }
  if (!rom_.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld(&rom_);
  auto status = overworld.Load(&rom_);
  if (!status.ok()) {
    return status;
  }

  // Set the world based on map_id
  if (map_id < 0x40) {
    overworld.set_current_world(0);  // Light World
  } else if (map_id < 0x80) {
    overworld.set_current_world(1);  // Dark World
  } else {
    overworld.set_current_world(2);  // Special World
  }

  // Set the tile
  overworld.SetTile(x, y, static_cast<uint16_t>(tile_id));

  // Save the ROM
  auto save_status = rom_.SaveToFile({.filename = rom_file});
  if (!save_status.ok()) {
    return save_status;
  }

  std::cout << "✅ Set tile at (" << x << ", " << y << ") on map " << map_id 
            << " to: 0x" << std::hex << tile_id << std::dec << std::endl;

  return absl::OkStatus();
}

} // namespace cli
} // namespace yaze
