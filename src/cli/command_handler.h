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
#include "app/emu/snes.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/compression.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "asar-dll-bindings/c/asar.h"

namespace yaze {
namespace cli {

enum ColorCode {
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
class ColorModifier {
  ColorCode code;

 public:
  explicit ColorModifier(ColorCode pCode) : code(pCode) {}
  friend std::ostream& operator<<(std::ostream& os, const ColorModifier& mod) {
    return os << "\033[" << mod.code << "m";
  }
};

class CommandHandler {
 public:
  CommandHandler() = default;
  virtual ~CommandHandler() = default;
  virtual absl::Status handle(const std::vector<std::string>& arg_vec) = 0;

  app::Rom rom_;
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
    patch.resize(rom_.size());
    patch_file.read((char*)patch.data(), patch.size());

    // Apply patch
    std::vector<uint8_t> patched;
    app::core::ApplyBpsPatch(source, patch, patched);

    // Save patched file
    std::ofstream patched_rom("patched.sfc", std::ios::binary);
    patched_rom.write((char*)patched.data(), patched.size());
    patched_rom.close();
    return absl::OkStatus();
  }
};

class AsarPatch : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    std::string patch_filename = arg_vec[1];
    std::string rom_filename = arg_vec[2];
    RETURN_IF_ERROR(rom_.LoadFromFile(rom_filename))
    int buflen = rom_.vector().size();
    int romlen = rom_.vector().size();
    if (!asar_patch(patch_filename.c_str(), rom_filename.data(), buflen,
                    &romlen)) {
      std::string error_message = "Failed to apply patch: ";
      int num_errors = 0;
      const errordata* errors = asar_geterrors(&num_errors);
      for (int i = 0; i < num_errors; i++) {
        error_message += absl::StrFormat("%s", errors[i].fullerrdata);
      }
      return absl::InternalError(error_message);
    }
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
    app::core::CreateBpsPatch(source, target, patch);

    // Save patch to file
    // std::ofstream patchFile("patch.bps", ios::binary);
    // patchFile.write(reinterpret_cast<const char*>(patch.data()),
    // patch.size()); patchFile.close();
    return absl::OkStatus();
  }
};

/**
 * @brief Open a Rom file and display information about it.
 */
class Open : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    ColorModifier green(ColorCode::FG_GREEN);
    ColorModifier blue(ColorCode::FG_BLUE);
    ColorModifier reset(ColorCode::FG_RESET);
    auto const& arg = arg_vec[0];
    RETURN_IF_ERROR(rom_.LoadFromFile(arg))
    std::cout << "Title: " << green << rom_.title() << std::endl;
    std::cout << reset << "Size: " << blue << "0x" << std::hex << rom_.size()
              << reset << std::endl;
    return absl::OkStatus();
  }
};

/**
 * @brief Backup a Rom file.
 */
class Backup : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    RETURN_IF_ERROR(rom_.LoadFromFile(arg_vec[0]))
    if (arg_vec.size() == 2) {
      // Optional filename added
      RETURN_IF_ERROR(rom_.SaveToFile(/*backup=*/true, false, arg_vec[1]))
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
    ColorModifier underline(ColorCode::FG_UNDERLINE);
    ColorModifier reset(ColorCode::FG_RESET);
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

    std::cout << "Decompress selected with argument: " << arg_vec[0]
              << std::endl;
    return absl::UnimplementedError("Decompress not implemented");
  }
};

/**
 * @brief Convert a SNES address to a PC address.

  * @param arg_vec `-s <address>`
  * @return absl::Status
*/
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

/**
 * @brief Convert a PC address to a SNES address.

  * @param arg_vec `-p <address>`
  * @return absl::Status
*/
class PcToSnes : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    auto arg = arg_vec[0];
    std::stringstream ss(arg.data());
    uint32_t pc_address;
    ss >> std::hex >> pc_address;
    uint32_t snes_address = app::core::PcToSnes(pc_address);
    ColorModifier blue(ColorCode::FG_BLUE);
    std::cout << "SNES LoROM Address: ";
    std::cout << blue << "$" << std::uppercase << std::hex << snes_address
              << "\n";
    return absl::OkStatus();
  }
};

/**
 * @brief Read from a Rom file.

  * @param arg_vec `-r <rom_file> <address> <optional:length, default: 0x01>`
  * @return absl::Status
*/
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
      auto returned_bytes_status = rom_.ReadByteVector(offset, length);
      if (!returned_bytes_status.ok()) {
        return returned_bytes_status.status();
      }
      auto returned_bytes = returned_bytes_status.value();
      for (const auto& each : returned_bytes) {
        std::cout << each;
      }
      std::cout << std::endl;
    } else {
      auto byte = rom_.ReadByte(offset);
      std::cout << std::hex << byte.value() << std::endl;
    }

    return absl::OkStatus();
  }
};

/**
 * @brief Transfer tile 16 data from one Rom to another.

  * @param arg_vec `-t <src_rom> <dest_rom> "<tile32_id_list:csv>"`
  * @return absl::Status
*/
class Tile16Transfer : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override;
};

/**
 * @brief Expand a Rom file.

  * @param arg_vec `-x <rom_file> <file_size>`
  * @return absl::Status
*/
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

/**
 * @brief Start the emulator on a SNES Rom file.

  * @param arg_vec `-emu <rom_file> <optional:num_cpu_cycles>`
  * @return absl::Status
*/
class Emulator : public CommandHandler {
 public:
  absl::Status handle(const std::vector<std::string>& arg_vec) override {
    std::string filename = arg_vec[0];
    RETURN_IF_ERROR(rom_.LoadFromFile(filename))

    return absl::OkStatus();
  }

  app::emu::SNES snes;
};

/**
 * @brief Command handler for the CLI.
 */
struct Commands {
  std::unordered_map<std::string, std::shared_ptr<CommandHandler>> handlers = {
      {"-emu", std::make_shared<Emulator>()},
      {"-a", std::make_shared<ApplyPatch>()},
      {"-asar", std::make_shared<AsarPatch>()},
      {"-c", std::make_shared<CreatePatch>()},
      {"-o", std::make_shared<Open>()},
      {"-b", std::make_shared<Backup>()},
      {"-x", std::make_shared<Expand>()},
      {"-i", std::make_shared<Compress>()},    // Import
      {"-e", std::make_shared<Decompress>()},  // Export
      {"-s", std::make_shared<SnesToPc>()},
      {"-p", std::make_shared<PcToSnes>()},
      {"-t", std::make_shared<Tile16Transfer>()},
      {"-r", std::make_shared<ReadFromRom>()}  // Read from Rom
  };
};

}  // namespace cli
}  // namespace yaze

#endif