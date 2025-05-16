#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/rom.h"
#include "cli/z3ed.h"
#include "util/macro.h"

namespace yaze {
namespace cli {

absl::Status Tile16Transfer::Run(const std::vector<std::string>& arg_vec) {
  // Load the source rom
  RETURN_IF_ERROR(rom_.LoadFromFile(arg_vec[0]))

  // Load the destination rom
  Rom dest_rom;
  RETURN_IF_ERROR(dest_rom.LoadFromFile(arg_vec[1]))

  std::vector<uint32_t> tileIDs;

  // Parse the CSV list of tile16 IDs.
  std::stringstream ss(arg_vec[2].data());
  for (std::string tileID; std::getline(ss, tileID, ',');) {
    if (tileID == "*") {
      // for (uint32_t i = 0; i <= rom_.GetMaxTileID(); ++i) {
      //   tileIDs.push_back(i);
      // }
      break;  // No need to continue parsing if * is used
    } else if (tileID.find('-') != std::string::npos) {
      // Handle range: split by hyphen and add all tile IDs in the range.
      std::stringstream rangeSS(tileID);
      std::string start;
      std::string end;
      std::getline(rangeSS, start, '-');
      std::getline(rangeSS, end);
      uint32_t startID = std::stoi(start, nullptr, 16);
      uint32_t endID = std::stoi(end, nullptr, 16);
      for (uint32_t i = startID; i <= endID; ++i) {
        tileIDs.push_back(i);
      }
    } else {
      // Handle single tile ID
      uint32_t tileID_int = std::stoi(tileID, nullptr, 16);
      tileIDs.push_back(tileID_int);
    }
  }

  for (const auto& tile16_id_int : tileIDs) {
    // Compare the tile16 data between source and destination ROMs.
    // auto source_tile16_data = rom_.ReadTile16(tile16_id_int);
    // auto dest_tile16_data = dest_rom.ReadTile16(tile16_id_int);
    ASSIGN_OR_RETURN(auto source_tile16_data, rom_.ReadTile16(tile16_id_int))
    ASSIGN_OR_RETURN(auto dest_tile16_data, dest_rom.ReadTile16(tile16_id_int))
    if (source_tile16_data != dest_tile16_data) {
      // Notify user of difference
      std::cout << "Difference detected in tile16 ID " << tile16_id_int
                << ". Do you want to transfer it to dest rom? (y/n): ";
      char userChoice;
      std::cin >> userChoice;

      // Transfer if user confirms
      if (userChoice == 'y' || userChoice == 'Y') {
        RETURN_IF_ERROR(
            dest_rom.WriteTile16(tile16_id_int, source_tile16_data));
        std::cout << "Transferred tile16 ID " << tile16_id_int
                  << " to dest rom." << std::endl;
      } else {
        std::cout << "Skipped transferring tile16 ID " << tile16_id_int << "."
                  << std::endl;
      }
    }
  }

  RETURN_IF_ERROR(dest_rom.SaveToFile(yaze::Rom::SaveSettings{
      .backup = true, .save_new = false, .filename = arg_vec[1]}))

  std::cout << "Successfully transferred tile16" << std::endl;

  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
