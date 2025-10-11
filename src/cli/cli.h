#ifndef YAZE_CLI_CLI_H
#define YAZE_CLI_CLI_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/rom.h"
#include "app/snes.h"
#include "util/macro.h"

// Forward declarations
namespace ftxui {
class ScreenInteractive;
}

namespace yaze {
namespace cli {

// Forward declaration
class TuiComponent;

class CommandHandler {
 public:
  CommandHandler() = default;
  virtual ~CommandHandler() = default;
  virtual absl::Status Run(const std::vector<std::string>& arg_vec) = 0;
  virtual void RunTUI(ftxui::ScreenInteractive& screen) {
    // Default implementation does nothing
  }

  Rom rom_;
};

struct CommandInfo {
  std::string name;
  std::string description;
  std::string usage;
  std::function<absl::Status(const std::vector<std::string>&)> handler;
};

class ModernCLI {
 public:
  ModernCLI();
  absl::Status Run(int argc, char* argv[]);
  CommandHandler* GetCommandHandler(const std::string& name);
  void PrintTopLevelHelp() const;
  void PrintCategoryHelp(const std::string& category) const;
  void PrintCommandSummary() const;

  std::map<std::string, CommandInfo> commands_;

 private:
  void SetupCommands();
  void ShowHelp();
  void ShowCategoryHelp(const std::string& category);
  void ShowCommandSummary() const;

  // Command Handlers
  absl::Status HandleAsarPatchCommand(const std::vector<std::string>& args);
  absl::Status HandleBpsPatchCommand(const std::vector<std::string>& args);
  absl::Status HandleExtractSymbolsCommand(const std::vector<std::string>& args);
  absl::Status HandleAgentCommand(const std::vector<std::string>& args);
  absl::Status HandleCollabCommand(const std::vector<std::string>& args);
  absl::Status HandleProjectBuildCommand(const std::vector<std::string>& args);
  absl::Status HandleProjectInitCommand(const std::vector<std::string>& args);
  absl::Status HandleRomInfoCommand(const std::vector<std::string>& args);
  absl::Status HandleRomGenerateGoldenCommand(const std::vector<std::string>& args);
  absl::Status HandleRomDiffCommand(const std::vector<std::string>& args);
  absl::Status HandleDungeonExportCommand(const std::vector<std::string>& args);
  absl::Status HandleDungeonListObjectsCommand(const std::vector<std::string>& args);
  absl::Status HandleGfxExportCommand(const std::vector<std::string>& args);
  absl::Status HandleGfxImportCommand(const std::vector<std::string>& args);
  absl::Status HandleCommandPaletteCommand(const std::vector<std::string>& args);
  absl::Status HandlePaletteExportCommand(const std::vector<std::string>& args);
  absl::Status HandlePaletteImportCommand(const std::vector<std::string>& args);
  absl::Status HandlePaletteCommand(const std::vector<std::string>& args);
  absl::Status HandleRomValidateCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldGetTileCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldFindTileCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldDescribeMapCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldListWarpsCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldSetTileCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldSelectRectCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldScrollToCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldSetZoomCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldGetVisibleRegionCommand(const std::vector<std::string>& args);
  absl::Status HandleSpriteCreateCommand(const std::vector<std::string>& args);
  absl::Status HandleChatEntryCommand(const std::vector<std::string>& args);
  absl::Status HandleProposalCommand(const std::vector<std::string>& args);
  absl::Status HandleWidgetCommand(const std::vector<std::string>& args);
};

// Legacy command classes removed - using new CommandHandler system
// See TODO comments in cli.cc for implementation roadmap

class Open : public CommandHandler {
 public:
  absl::Status Run(const std::vector<std::string>& arg_vec) override {
    auto const& arg = arg_vec[0];
    RETURN_IF_ERROR(rom_.LoadFromFile(arg, RomLoadOptions::CliDefaults()))
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

#endif  // YAZE_CLI_CLI_H
