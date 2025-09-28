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

#ifdef _WIN32
extern "C" int SDL_main(int argc, char *argv[]) {
#else
int main(int argc, char *argv[]) {
#endif
  yaze::util::FlagParser flag_parser(yaze::util::global_flag_registry());
  RETURN_IF_EXCEPTION(flag_parser.Parse(argc, argv));
  yaze::cli::ShowMain();
  return EXIT_SUCCESS;
}
