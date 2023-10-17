#include "cli/command_handler.h"

#include <string>  // for basic_string, char_traits, stoi
#include <vector>  // for vector, vector<>::value_type

#include "absl/status/status.h"  // for OkStatus, Status
#include "app/core/common.h"     // for app
#include "app/core/constants.h"  // for RETURN_IF_ERROR
#include "app/rom.h"             // for ROM

namespace yaze {
namespace cli {

using namespace app;

absl::Status Tile16Transfer::handle(const std::vector<std::string>& arg_vec) {
  // Load the source rom
  RETURN_IF_ERROR(rom_.LoadFromFile(arg_vec[0]))

  // Load the destination rom
  ROM dest_rom;
  RETURN_IF_ERROR(dest_rom.LoadFromFile(arg_vec[1]))

  // Parse the CSV list of tile16 IDs.
  std::stringstream ss(arg_vec[2].data());
  for (std::string tile16_id; std::getline(ss, tile16_id, ',');) {
    std::cout << "Writing tile16 ID " << tile16_id << " to dest rom."
              << std::endl;

    // Convert the string to a base16 integer.
    uint32_t tile16_id_int = std::stoi(tile16_id, nullptr, /*base=*/16);

    // Read the tile16 definition from the source ROM.
    auto tile16_data = rom_.ReadTile16(tile16_id_int);

    // Write the tile16 definition to the destination ROM.
    dest_rom.WriteTile16(tile16_id_int, tile16_data);
  }

  RETURN_IF_ERROR(dest_rom.SaveToFile(/*backup=*/true, arg_vec[1]))

  std::cout << "Successfully transferred tile16" << std::endl;

  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze