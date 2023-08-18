#ifndef YAZE_CLI_COMMAND_HANDLER_H
#define YAZE_CLI_COMMAND_HANDLER_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/rom.h"
#include "cli/patch.h"

namespace yaze {
namespace cli {

namespace Color {
enum Code {
  FG_RED = 31,
  FG_GREEN = 32,
  FG_YELLOW = 33,
  FG_BLUE = 36,
  FG_MAGENTA = 35,
  FG_DEFAULT = 39,
  FG_RESET = 0,
  FG_UNDERLINE = 4,
  BG_RED = 41,
  BG_GREEN = 42,
  BG_BLUE = 44,
  BG_DEFAULT = 49
};
class Modifier {
  Code code;

 public:
  explicit Modifier(Code pCode) : code(pCode) {}
  friend std::ostream& operator<<(std::ostream& os, const Modifier& mod) {
    return os << "\033[" << mod.code << "m";
  }
};
}  // namespace Color

namespace {
std::vector<std::string> ParseArguments(const std::string_view args) {
  std::vector<std::string> arguments;
  std::stringstream ss(args.data());
  std::string arg;
  while (ss >> arg) {
    arguments.push_back(arg);
  }
  return arguments;
}
}  // namespace

class CommandHandler {
 public:
  CommandHandler() = default;
  virtual ~CommandHandler() = default;
  virtual absl::Status handle(std::string_view arg) = 0;

  app::ROM rom_;
};

class ApplyPatch : public CommandHandler {
 public:
  absl::Status handle(std::string_view arg) override {
    auto arg_vec = ParseArguments(arg);
    std::string rom_filename = arg_vec[1];
    std::string patch_filename = arg_vec[2];
    RETURN_IF_ERROR(rom_.LoadFromFile(rom_filename))
    auto source = rom_.vector();
    std::ifstream patch_file(patch_filename, std::ios::binary);
    std::vector<uint8_t> patch;
    patch_file.read(reinterpret_cast<char*>(patch.data()), patch.size());

    // Apply patch
    std::vector<uint8_t> patched;
    ApplyBpsPatch(source, patch, patched);

    // Save patched file
    std::ofstream patched_rom("patched.sfc", std::ios::binary);
    patched_rom.write(reinterpret_cast<const char*>(patched.data()),
                      patched.size());
    patched_rom.close();
    return absl::OkStatus();
  }
};

class CreatePatch : public CommandHandler {
 public:
  absl::Status handle(std::string_view arg) override {
    std::vector<uint8_t> source;
    std::vector<uint8_t> target;
    std::vector<uint8_t> patch;
    // Create patch
    CreateBpsPatch(source, target, patch);

    // Save patch to file
    // std::ofstream patchFile("patch.bps", ios::binary);
    // patchFile.write(reinterpret_cast<const char*>(patch.data()),
    // patch.size()); patchFile.close();
    return absl::OkStatus();
  }
};

class Open : public CommandHandler {
 public:
  absl::Status handle(std::string_view arg) override {
    Color::Modifier green(Color::FG_GREEN);
    Color::Modifier blue(Color::FG_BLUE);
    Color::Modifier reset(Color::FG_RESET);
    RETURN_IF_ERROR(rom_.LoadFromFile(arg))
    std::cout << "Title: " << green << rom_.title() << std::endl;
    std::cout << reset << "Size: " << blue << "0x" << std::hex << rom_.size()
              << reset << std::endl;
    return absl::OkStatus();
  }
};

class Backup : public CommandHandler {
 public:
  absl::Status handle(std::string_view arg) override {
    auto args_vec = ParseArguments(arg);
    RETURN_IF_ERROR(rom_.LoadFromFile(args_vec[0]))
    if (args_vec.size() == 2) {
      // Optional filename added
      RETURN_IF_ERROR(rom_.SaveToFile(/*backup=*/true, args_vec[1]))
    } else {
      RETURN_IF_ERROR(rom_.SaveToFile(/*backup=*/true))
    }
    return absl::OkStatus();
  }
};

// Compress Graphics
class Compress : public CommandHandler {
 public:
  absl::Status handle(std::string_view arg) override {
    std::cout << "Compress selected with argument: " << arg << std::endl;
    return absl::OkStatus();
  }
};

// Decompress (Export) Graphics
//
// -e <rom_file> <bin_file> --mode=<optional:mode>
//
// mode:
class Decompress : public CommandHandler {
 public:
  absl::Status handle(std::string_view arg) override {
    auto args_vec = ParseArguments(arg);
    Color::Modifier underline(Color::FG_UNDERLINE);
    Color::Modifier reset(Color::FG_RESET);
    std::cout << "Please specify the tilesheets you want to export\n";
    std::cout << "You can input an individual sheet, a range X-Y, or comma "
                 "separate values.\n\n";
    std::cout << underline << "Tilesheets\n" << reset;
    std::cout << "0-112 -> compressed 3bpp bgr \n";
    std::cout << "113-114 -> compressed 2bpp\n";
    std::cout << "115-126 -> uncompressed 3bpp sprites\n";
    std::cout << "127-217 -> compressed 3bpp sprites\n";
    std::cout << "218-222 -> compressed 2bpp\n";

    std::cout << "Enter tilesheets: ";
    std::string sheet_input;
    std::cin >> sheet_input;

    // Batch Mode
    // if (args_vec.size() == 1) {
    //   auto rom_filename = args_vec[1];
    //   RETURN_IF_ERROR(rom_.LoadFromFile(arg, true))
    //   RETURN_IF_ERROR(rom_.LoadAllGraphicsData())
    //   for (auto& graphic_sheet : rom_.GetGraphicsBin()) {
    //     const auto filename =
    //         absl::StrCat(rom_.filename(), graphic_sheet.first);
    //     graphic_sheet.second.SaveSurfaceToFile(filename);
    //   }
    // }

    std::cout << "Decompress selected with argument: " << arg << std::endl;
    return absl::OkStatus();
  }
};

// SnesToPc Conversion
class SnesToPc : public CommandHandler {
 public:
  absl::Status handle(std::string_view arg) override {
    std::stringstream ss(arg.data());
    uint32_t snes_address;
    ss >> std::hex >> snes_address;
    uint32_t pc_address = app::core::SnesToPc(snes_address);
    std::cout << std::hex << pc_address << std::endl;
    return absl::OkStatus();
  }
};

class PcToSnes : public CommandHandler {
 public:
  absl::Status handle(std::string_view arg) override {
    std::stringstream ss(arg.data());
    uint32_t pc_address;
    ss >> std::hex >> pc_address;
    uint32_t snes_address = app::core::PcToSnes(pc_address);
    Color::Modifier blue(Color::FG_BLUE);
    std::cout << "SNES LoROM Address: ";
    std::cout << blue << "$" << std::uppercase << std::hex << snes_address
              << "\n";
    return absl::OkStatus();
  }
};

struct Commands {
  std::unordered_map<std::string, std::shared_ptr<CommandHandler>> handlers = {
      {"-a", std::make_shared<ApplyPatch>()},
      {"-c", std::make_shared<CreatePatch>()},
      {"-o", std::make_shared<Open>()},
      {"-b", std::make_shared<Backup>()},
      {"-i", std::make_shared<Compress>()},    // Import
      {"-e", std::make_shared<Decompress>()},  // Export
      {"-s", std::make_shared<SnesToPc>()},
      {"-p", std::make_shared<PcToSnes>()}};
};

}  // namespace cli
}  // namespace yaze

#endif