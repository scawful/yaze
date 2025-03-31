#include "cli/z3ed.h"

namespace yaze {
namespace cli {

absl::Status Compress::Run(const std::vector<std::string>& arg_vec) {
  std::cout << "Compress selected with argument: " << arg_vec[0] << std::endl;
  return absl::OkStatus();
}

absl::Status Decompress::Run(const std::vector<std::string>& arg_vec) {
  std::cout << "Please specify the tilesheets you want to export\n";
  std::cout << "You can input an individual sheet, a range X-Y, or comma "
               "separate values.\n\n";
  std::cout  << "Tilesheets\n";
  std::cout << "0-112 -> compressed 3bpp bgr \n";
  std::cout << "113-114 -> compressed 2bpp\n";
  std::cout << "115-126 -> uncompressed 3bpp sprites\n";
  std::cout << "127-217 -> compressed 3bpp sprites\n";
  std::cout << "218-222 -> compressed 2bpp\n";

  std::cout << "Enter tilesheets: ";
  std::string sheet_input;
  std::cin >> sheet_input;

  std::cout << "Decompress selected with argument: " << arg_vec[0] << std::endl;
  return absl::UnimplementedError("Decompress not implemented");
}

}  // namespace cli
}  // namespace yaze