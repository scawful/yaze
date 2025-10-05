#include "cli/z3ed.h"
#include "app/gfx/scad_format.h"
#include "app/gfx/arena.h"
#include "absl/flags/flag.h"
#include "absl/flags/declare.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {

absl::Status GfxExport::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 2) {
    return absl::InvalidArgumentError("Usage: gfx export-sheet <sheet_id> --to <file>");
  }

  int sheet_id = std::stoi(arg_vec[0]);
  std::string output_file = arg_vec[1];

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  rom_.LoadFromFile(rom_file);
  if (!rom_.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  auto& arena = gfx::Arena::Get();
  auto sheet = arena.gfx_sheet(sheet_id);
  if (!sheet.is_active()) {
      return absl::NotFoundError("Graphics sheet not found.");
  }

  // For now, we will just save the raw 8bpp data.
  // TODO: Convert the 8bpp data to the correct SNES bpp format.
  std::vector<uint8_t> header; // Empty header for now
  auto status = gfx::SaveCgx(sheet.depth(), output_file, sheet.vector(), header);
  if (!status.ok()) {
      return status;
  }

  std::cout << "Successfully exported graphics sheet " << sheet_id << " to " << output_file << std::endl;

  return absl::OkStatus();
}

absl::Status GfxImport::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 2) {
    return absl::InvalidArgumentError("Usage: gfx import-sheet <sheet_id> --from <file>");
  }

  int sheet_id = std::stoi(arg_vec[0]);
  std::string input_file = arg_vec[1];

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  rom_.LoadFromFile(rom_file);
  if (!rom_.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  std::vector<uint8_t> cgx_data, cgx_loaded, cgx_header;
  auto status = gfx::LoadCgx(8, input_file, cgx_data, cgx_loaded, cgx_header);
  if (!status.ok()) {
      return status;
  }

  auto& arena = gfx::Arena::Get();
  auto sheet = arena.gfx_sheet(sheet_id);
  if (!sheet.is_active()) {
      return absl::NotFoundError("Graphics sheet not found.");
  }

  // TODO: Convert the 8bpp data to the correct SNES bpp format before writing.
  // For now, we just replace the data directly.
  sheet.set_data(cgx_loaded);

  // TODO: Implement saving the modified graphics sheet back to the ROM.
  auto save_status = rom_.SaveToFile({.save_new = false});
  if (!save_status.ok()) {
    return save_status;
  }

  std::cout << "Successfully imported graphics sheet " << sheet_id << " from " << input_file << std::endl;
  std::cout << "✅ ROM saved to: " << rom_.filename() << std::endl;

  return absl::OkStatus();
}

} // namespace cli
} // namespace yaze
