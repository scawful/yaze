#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/rom.h"
#include "cli/command_handler.h"
#include "cli/patch.h"

namespace yaze {
namespace cli {
namespace {

void HelpCommand() {
  Color::Modifier ylw(Color::FG_YELLOW);
  Color::Modifier mag(Color::FG_MAGENTA);
  Color::Modifier red(Color::FG_RED);
  Color::Modifier reset(Color::FG_RESET);
  Color::Modifier underline(Color::FG_UNDERLINE);
  std::cout << "\n";
  std::cout << ylw << " ▲  " << reset << "    z3ed\n";
  std::cout << ylw << "▲ ▲ " << reset << "    by " << mag << "scawful\n\n"
            << reset;
  std::cout << "The Legend of " << red << "Zelda" << reset
            << ": A Link to the Past Hacking Tool\n\n";
  std::cout << underline;
  std::cout << "Command" << reset << "                 " << underline << "Arg"
            << reset << "   " << underline << "Params\n"
            << reset;

  std::cout << "Apply BPS Patch         -a    <rom_file> <bps_file>\n";
  std::cout << "Create BPS Patch        -c    <bps_file> <src_file> "
               "<modified_file>\n\n";

  std::cout << "Open ROM                -o    <rom_file>\n";
  std::cout << "Backup ROM              -b    <rom_file> <optional:new_file>\n";
  std::cout << "Expand ROM              -x    <rom_file> <file_size>\n\n";

  std::cout << "Transfer Tile16         -t    <src_rom> <dest_rom> "
               "<tile32_id_list:csv>\n\n";

  std::cout << "Export Graphics         -e    <rom_file> <bin_file>\n";
  std::cout << "Import Graphics         -i    <bin_file> <rom_file>\n\n";

  std::cout << "SNES to PC Address      -s    <address>\n";
  std::cout << "PC to SNES Address      -p    <address>\n";
  std::cout << "\n";
}

int RunCommandHandler(int argc, char* argv[]) {
  if (argv[1] == "-h" || argc == 1) {
    HelpCommand();
    return EXIT_SUCCESS;
  }

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

int main(int argc, char* argv[]) {
  return yaze::cli::RunCommandHandler(argc, argv);
}