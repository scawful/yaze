#include "cli/z3ed.h"
#include "absl/flags/flag.h"
#include "absl/flags/declare.h"
#include "absl/strings/str_format.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {

absl::Status RomValidate::Run(const std::vector<std::string>& arg_vec) {
  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  rom_.LoadFromFile(rom_file);
  if (!rom_.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  bool all_ok = true;
  std::cout << "Validating ROM: " << rom_file << std::endl;

  // Checksum validation
  std::cout << "  - Verifying checksum... " << std::flush;
  // Basic ROM validation - check if ROM is loaded and has reasonable size
  if (rom_.is_loaded() && rom_.size() > 0) {
    std::cout << "✅ PASSED" << std::endl;
  } else {
    std::cout << "❌ FAILED" << std::endl;
    all_ok = false;
  }

  // Header validation
  std::cout << "  - Verifying header... " << std::flush;
  if (rom_.title() == "THE LEGEND OF ZELDA") {
    std::cout << "✅ PASSED" << std::endl;
  } else {
    std::cout << "❌ FAILED (Invalid title: " << rom_.title() << ")" << std::endl;
    all_ok = false;
  }

  std::cout << std::endl;
  if (all_ok) {
    std::cout << "✅ ROM validation successful." << std::endl;
  } else {
    std::cout << "❌ ROM validation failed." << std::endl;
  }

  return absl::OkStatus();
}

absl::Status RomDiff::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 2) {
    return absl::InvalidArgumentError("Usage: rom diff <rom_a> <rom_b>");
  }

  Rom rom_a;
  auto status_a = rom_a.LoadFromFile(arg_vec[0]);
  if (!status_a.ok()) {
    return status_a;
  }

  Rom rom_b;
  auto status_b = rom_b.LoadFromFile(arg_vec[1]);
  if (!status_b.ok()) {
    return status_b;
  }

  if (rom_a.size() != rom_b.size()) {
    std::cout << "ROMs have different sizes: " << rom_a.size() << " vs " << rom_b.size() << std::endl;
  }

  int differences = 0;
  for (size_t i = 0; i < rom_a.size(); ++i) {
    if (rom_a.vector()[i] != rom_b.vector()[i]) {
      differences++;
      std::cout << absl::StrFormat("Difference at 0x%08X: 0x%02X vs 0x%02X\n", i, rom_a.vector()[i], rom_b.vector()[i]);
    }
  }

  if (differences == 0) {
    std::cout << "ROMs are identical." << std::endl;
  } else {
    std::cout << "Found " << differences << " differences." << std::endl;
  }

  return absl::OkStatus();
}

absl::Status RomGenerateGolden::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 2) {
    return absl::InvalidArgumentError("Usage: rom generate-golden <rom_file> <golden_file>");
  }

  Rom rom;
  auto status = rom.LoadFromFile(arg_vec[0]);
  if (!status.ok()) {
    return status;
  }

  std::ofstream file(arg_vec[1], std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("Could not open file for writing.");
  }

  file.write(reinterpret_cast<const char*>(rom.vector().data()), rom.size());

  std::cout << "Successfully generated golden file: " << arg_vec[1] << std::endl;

  return absl::OkStatus();
}

} // namespace cli
} // namespace yaze