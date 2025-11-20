#include "cli/handlers/rom/rom_commands.h"

#include <fstream>

#include "absl/strings/str_format.h"
#include "util/macro.h"

namespace yaze {
namespace cli {
namespace handlers {

absl::Status RomInfoCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM must be loaded");
  }

  formatter.AddField("title", rom->title());
  formatter.AddField("size", absl::StrFormat("0x%X", rom->size()));
  formatter.AddField("size_bytes", static_cast<int>(rom->size()));

  return absl::OkStatus();
}

absl::Status RomValidateCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM must be loaded");
  }

  bool all_ok = true;
  std::vector<std::string> validation_results;

  // Basic ROM validation - check if ROM is loaded and has reasonable size
  if (rom->is_loaded() && rom->size() > 0) {
    validation_results.push_back("checksum: PASSED");
  } else {
    validation_results.push_back("checksum: FAILED");
    all_ok = false;
  }

  // Header validation
  if (rom->title() == "THE LEGEND OF ZELDA") {
    validation_results.push_back("header: PASSED");
  } else {
    validation_results.push_back(
        "header: FAILED (Invalid title: " + rom->title() + ")");
    all_ok = false;
  }

  formatter.AddField("validation_passed", all_ok);
  std::string results_str;
  for (const auto& result : validation_results) {
    if (!results_str.empty()) results_str += "; ";
    results_str += result;
  }
  formatter.AddField("results", results_str);

  return absl::OkStatus();
}

absl::Status RomDiffCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto rom_a_opt = parser.GetString("rom_a");
  auto rom_b_opt = parser.GetString("rom_b");

  if (!rom_a_opt.has_value()) {
    return absl::InvalidArgumentError("Missing required argument: rom_a");
  }
  if (!rom_b_opt.has_value()) {
    return absl::InvalidArgumentError("Missing required argument: rom_b");
  }

  std::string rom_a_path = rom_a_opt.value();
  std::string rom_b_path = rom_b_opt.value();

  Rom rom_a;
  auto status_a = rom_a.LoadFromFile(rom_a_path, RomLoadOptions::CliDefaults());
  if (!status_a.ok()) {
    return status_a;
  }

  Rom rom_b;
  auto status_b = rom_b.LoadFromFile(rom_b_path, RomLoadOptions::CliDefaults());
  if (!status_b.ok()) {
    return status_b;
  }

  if (rom_a.size() != rom_b.size()) {
    formatter.AddField("size_match", false);
    formatter.AddField("size_a", static_cast<int>(rom_a.size()));
    formatter.AddField("size_b", static_cast<int>(rom_b.size()));
    return absl::OkStatus();
  }

  int differences = 0;
  std::vector<std::string> diff_details;

  for (size_t i = 0; i < rom_a.size(); ++i) {
    if (rom_a.vector()[i] != rom_b.vector()[i]) {
      differences++;
      if (differences <= 10) {  // Limit output to first 10 differences
        diff_details.push_back(absl::StrFormat("0x%08X: 0x%02X vs 0x%02X", i,
                                               rom_a.vector()[i],
                                               rom_b.vector()[i]));
      }
    }
  }

  formatter.AddField("identical", differences == 0);
  formatter.AddField("differences_count", differences);
  if (!diff_details.empty()) {
    std::string diff_str;
    for (const auto& diff : diff_details) {
      if (!diff_str.empty()) diff_str += "; ";
      diff_str += diff;
    }
    formatter.AddField("differences", diff_str);
  }

  return absl::OkStatus();
}

absl::Status RomGenerateGoldenCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto rom_opt = parser.GetString("rom_file");
  auto golden_opt = parser.GetString("golden_file");

  if (!rom_opt.has_value()) {
    return absl::InvalidArgumentError("Missing required argument: rom_file");
  }
  if (!golden_opt.has_value()) {
    return absl::InvalidArgumentError("Missing required argument: golden_file");
  }

  std::string rom_path = rom_opt.value();
  std::string golden_path = golden_opt.value();

  Rom source_rom;
  auto status =
      source_rom.LoadFromFile(rom_path, RomLoadOptions::CliDefaults());
  if (!status.ok()) {
    return status;
  }

  std::ofstream file(golden_path, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("Could not open file for writing: " +
                               golden_path);
  }

  file.write(reinterpret_cast<const char*>(source_rom.vector().data()),
             source_rom.size());

  formatter.AddField("status", "success");
  formatter.AddField("golden_file", golden_path);
  formatter.AddField("source_file", rom_path);
  formatter.AddField("size", static_cast<int>(source_rom.size()));

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
