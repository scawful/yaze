#ifndef YAZE_CLI_Z3ED_H
#define YAZE_CLI_Z3ED_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "app/rom.h"
#include "app/snes.h"
#include "util/macro.h"

namespace yaze {
namespace cli {

class CommandHandler {
 public:
  CommandHandler() = default;
  virtual ~CommandHandler() = default;
  virtual absl::Status Run(const std::vector<std::string>& arg_vec) = 0;

  Rom rom_;
};

class ApplyPatch : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override;
};

class AsarPatch : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override;
};

class CreatePatch : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override;
};

class Tile16Transfer : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override;
};

class Open : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override {
    auto const& arg = arg_vec[0];
    RETURN_IF_ERROR(rom_.LoadFromFile(arg))
    std::cout << "Title: " << rom_.title() << std::endl;
    std::cout << "Size: 0x" << std::hex << rom_.size() << std::endl;
    return absl::OkStatus();
  }
};

class Backup : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override {
    RETURN_IF_ERROR(rom_.LoadFromFile(arg_vec[0]))
    Rom::SaveSettings settings;
    settings.backup = true;
    if (arg_vec.size() == 2) {
      // Optional filename added
      settings.filename = arg_vec[1];
    }
    RETURN_IF_ERROR(rom_.SaveToFile(settings))
    return absl::OkStatus();
  }
};

class Compress : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override;
};

class Decompress : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override;
};

/**
 * @brief Convert a SNES address to a PC address.

  * @param arg_vec `-s <address>`
  * @return absl::Status
*/
class SnesToPcCommand : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override {
    auto arg = arg_vec[0];
    std::stringstream ss(arg.data());
    uint32_t snes_address;
    ss >> std::hex >> snes_address;
    uint32_t pc_address = SnesToPc(snes_address);
    std::cout << std::hex << pc_address << std::endl;
    return absl::OkStatus();
  }
};

/**
 * @brief Convert a PC address to a SNES address.

  * @param arg_vec `-p <address>`
  * @return absl::Status
*/
class PcToSnesCommand : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override {
    auto arg = arg_vec[0];
    std::stringstream ss(arg.data());
    uint32_t pc_address;
    ss >> std::hex >> pc_address;
    uint32_t snes_address = PcToSnes(pc_address);
    std::cout << "SNES LoROM Address: ";
    std::cout << "$" << std::uppercase << std::hex << snes_address << "\n";
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
  absl::Status Run(const std::vector<std::string>& arg_vec) override {
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
 * @brief Expand a Rom file.

  * @param arg_vec `-x <rom_file> <file_size>`
  * @return absl::Status
*/
class Expand : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override {
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

}  // namespace cli
}  // namespace yaze

#endif
