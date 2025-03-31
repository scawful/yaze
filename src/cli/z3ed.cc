#include "cli/z3ed.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cli/tui.h"
#include "util/flag.h"
#include "util/macro.h"

DEFINE_FLAG(std::string, rom_file, "", "The ROM file to load.");
DEFINE_FLAG(std::string, bps_file, "", "The BPS file to apply.");

DEFINE_FLAG(std::string, src_file, "", "The source file.");
DEFINE_FLAG(std::string, modified_file, "", "The modified file.");

DEFINE_FLAG(std::string, bin_file, "", "The binary file to export to.");
DEFINE_FLAG(std::string, address, "", "The address to convert.");
DEFINE_FLAG(std::string, length, "", "The length of the data to read.");

DEFINE_FLAG(std::string, file_size, "", "The size of the file to expand to.");
DEFINE_FLAG(std::string, dest_rom, "", "The destination ROM file.");
DEFINE_FLAG(std::string, tile32_id_list, "",
            "The list of tile32 IDs to transfer.");

namespace yaze {
/**
 * @namespace yaze::cli
 * @brief Namespace for the command line interface.
 */
namespace cli {
namespace {

int RunCommandHandler(int argc, char *argv[]) {
  std::vector<std::string> arguments;
  for (int i = 2; i < argc; i++) {  // Skip the arg mode (argv[1])
    std::cout << "argv[" << i << "] = " << argv[i] << std::endl;
    arguments.emplace_back(argv[i]);
  }

  Commands commands;
  std::string mode = argv[1];
  if (commands.handlers.find(mode) != commands.handlers.end()) {
    PRINT_IF_ERROR(commands.handlers[mode]->handle(arguments))
  } else {
    std::cerr << "Invalid mode specified: " << mode << std::endl;
  }
  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace cli
}  // namespace yaze

int main(int argc, char *argv[]) {
  yaze::util::FlagParser flag_parser(yaze::util::global_flag_registry());
  RETURN_IF_EXCEPTION(flag_parser.Parse(argc, argv));

  for (const auto &flag : yaze::util::global_flag_registry()->AllFlags()) {
    // Cast the IFlag to a Flag and use Get
    auto flags = dynamic_cast<yaze::util::Flag<std::string> *>(flag);
    std::cout << "Flag: " << flag->name() << " = " << flags->Get() << std::endl;
  }

  yaze::cli::ShowMain();
  return EXIT_SUCCESS;
}
