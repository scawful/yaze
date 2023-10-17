#ifndef YAZE_CLI_COMMAND_HANDLER_H
#define YAZE_CLI_COMMAND_HANDLER_H

#include <cstdint>   // for uint8_t, uint32_t
#include <iostream>  // for operator<<, string, ostream, basic_...
#include <memory>    // for make_shared, shared_ptr
#include <sstream>
#include <string>  // for char_traits, basic_string, hash
#include <string_view>
#include <unordered_map>  // for unordered_map
#include <vector>         // for vector, vector<>::value_type

#include "absl/status/status.h"  // for OkStatus, Status
#include "absl/strings/str_cat.h"
#include "app/core/common.h"     // for PcToSnes, SnesToPc
#include "app/core/constants.h"  // for RETURN_IF_ERROR
#include "app/core/pipeline.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/compression.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/rom.h"  // for ROM
#include "app/zelda3/overworld.h"
#include "cli/patch.h"  // for ApplyBpsPatch, CreateBpsPatch

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

class CommandHandler {
 public:
  CommandHandler() = default;
  virtual ~CommandHandler() = default;
  virtual absl::Status handle(const std::vector<std::string>& arg_vec) = 0;

  app::ROM rom_;
};

class ApplyPatch : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
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
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
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
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    Color::Modifier green(Color::FG_GREEN);
    Color::Modifier blue(Color::FG_BLUE);
    Color::Modifier reset(Color::FG_RESET);
    auto const& arg = arg_vec[0];
    RETURN_IF_ERROR(rom_.LoadFromFile(arg))
    std::cout << "Title: " << green << rom_.title() << std::endl;
    std::cout << reset << "Size: " << blue << "0x" << std::hex << rom_.size()
              << reset << std::endl;
    return absl::OkStatus();
  }
};

// Backup ROM
class Backup : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    RETURN_IF_ERROR(rom_.LoadFromFile(arg_vec[0]))
    if (arg_vec.size() == 2) {
      // Optional filename added
      RETURN_IF_ERROR(rom_.SaveToFile(/*backup=*/true, arg_vec[1]))
    } else {
      RETURN_IF_ERROR(rom_.SaveToFile(/*backup=*/true))
    }
    return absl::OkStatus();
  }
};

// Compress Graphics
class Compress : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    std::cout << "Compress selected with argument: " << arg_vec[0] << std::endl;
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
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
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
    // if (arg_vec.size() == 1) {
    //   auto rom_filename = arg_vec[1];
    //   RETURN_IF_ERROR(rom_.LoadFromFile(arg, true))
    //   RETURN_IF_ERROR(rom_.LoadAllGraphicsData())
    //   for (auto& graphic_sheet : rom_.GetGraphicsBin()) {
    //     const auto filename =
    //         absl::StrCat(rom_.filename(), graphic_sheet.first);
    //     graphic_sheet.second.SaveSurfaceToFile(filename);
    //   }
    // }

    std::cout << "Decompress selected with argument: " << arg_vec[0]
              << std::endl;
    return absl::OkStatus();
  }
};

// SnesToPc Conversion
// -s <address>
class SnesToPc : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    auto arg = arg_vec[0];
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
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    auto arg = arg_vec[0];
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

// -r <rom_file> <address> <optional:length, default: 0x01>
class ReadFromRom : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    RETURN_IF_ERROR(rom_.LoadFromFile(arg_vec[0]))

    std::stringstream ss(arg_vec[1].data());
    uint32_t offset;
    ss >> std::hex >> offset;
    uint32_t length = 0x01;
    if (!arg_vec[2].empty()) {
      length = std::stoi(arg_vec[2]);
    }

    if (length > 1) {
      auto returned_bytes = rom_.ReadByteVector(offset, length);
      for (const auto& each : returned_bytes) {
        std::cout << each;
      }
      std::cout << std::endl;
    } else {
      auto byte = rom_.ReadByte(offset);
      std::cout << std::hex << byte << std::endl;
    }

    return absl::OkStatus();
  }
};

// Transfer tile 16 data from one rom to another
// -t <src_rom> <dest_rom> "<tile32_id_list:csv>"
class Tile16Transfer : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override;
};

class Expand : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    RETURN_IF_ERROR(rom_.LoadFromFile(arg_vec[0]))

    std::stringstream ss(arg_vec[1].data());
    uint32_t size;
    ss >> std::hex >> size;

    rom_.Expand(size);

    std::cout << "Successfully expanded ROM to " << std::hex << size
              << std::endl;

    return absl::OkStatus();
  }
};

struct Commands {
  std::unordered_map<std::string, std::shared_ptr<CommandHandler>> handlers = {
      {"-a", std::make_shared<ApplyPatch>()},
      {"-c", std::make_shared<CreatePatch>()},
      {"-o", std::make_shared<Open>()},
      {"-b", std::make_shared<Backup>()},
      {"-x", std::make_shared<Expand>()},
      {"-i", std::make_shared<Compress>()},    // Import
      {"-e", std::make_shared<Decompress>()},  // Export
      {"-s", std::make_shared<SnesToPc>()},
      {"-p", std::make_shared<PcToSnes>()},
      {"-t", std::make_shared<Tile16Transfer>()},
      {"-r", std::make_shared<ReadFromRom>()}  // Read from ROM
  };
};

}  // namespace cli
}  // namespace yaze

#endif