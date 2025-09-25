#include "asar-dll-bindings/c/asar.h"
#include "cli/z3ed.h"
#include "util/bps.h"

namespace yaze {
namespace cli {

absl::Status ApplyPatch::Run(const std::vector<std::string>& arg_vec) {
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
  util::ApplyBpsPatch(source, patch, patched);

  // Save patched file
  std::ofstream patched_rom("patched.sfc", std::ios::binary);
  patched_rom.write((char*)patched.data(), patched.size());
  patched_rom.close();
  return absl::OkStatus();
}

absl::Status AsarPatch::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 2) {
    return absl::InvalidArgumentError("Usage: asar <patch_file> <rom_file>");
  }

  std::string patch_filename = arg_vec[0];
  std::string rom_filename = arg_vec[1];
  
  // Load ROM file
  RETURN_IF_ERROR(rom_.LoadFromFile(rom_filename))
  
  // Get ROM data
  auto rom_data = rom_.vector();
  int buflen = static_cast<int>(rom_data.size());
  int romlen = buflen;
  
  // Ensure we have enough buffer space
  const int max_rom_size = asar_maxromsize();
  if (buflen < max_rom_size) {
    rom_data.resize(max_rom_size, 0);
    buflen = max_rom_size;
  }
  
  // Apply Asar patch
  if (!asar_patch(patch_filename.c_str(), 
                  reinterpret_cast<char*>(rom_data.data()), 
                  buflen, &romlen)) {
    std::string error_message = "Failed to apply Asar patch:\n";
    int num_errors = 0;
    const errordata* errors = asar_geterrors(&num_errors);
    for (int i = 0; i < num_errors; i++) {
      error_message += absl::StrFormat("  %s\n", errors[i].fullerrdata);
    }
    return absl::InternalError(error_message);
  }
  
  // Resize ROM to actual size 
  rom_data.resize(romlen);
  
  // Update the ROM data by writing the patched data back
  for (size_t i = 0; i < rom_data.size(); ++i) {
    auto status = rom_.WriteByte(i, rom_data[i]);
    if (!status.ok()) {
      return status;
    }
  }
  
  // Save patched ROM
  std::string output_filename = rom_filename;
  size_t dot_pos = output_filename.find_last_of('.');
  if (dot_pos != std::string::npos) {
    output_filename.insert(dot_pos, "_patched");
  } else {
    output_filename += "_patched";
  }
  
  Rom::SaveSettings settings;
  settings.filename = output_filename;
  RETURN_IF_ERROR(rom_.SaveToFile(settings))
  
  std::cout << "✅ Asar patch applied successfully!" << std::endl;
  std::cout << "📁 Output: " << output_filename << std::endl;
  std::cout << "📊 Final ROM size: " << romlen << " bytes" << std::endl;
  
  // Show warnings if any
  int num_warnings = 0;
  const errordata* warnings = asar_getwarnings(&num_warnings);
  if (num_warnings > 0) {
    std::cout << "⚠️  Warnings:" << std::endl;
    for (int i = 0; i < num_warnings; i++) {
      std::cout << "  " << warnings[i].fullerrdata << std::endl;
    }
  }
  
  // Show extracted symbols
  int num_labels = 0;
  const labeldata* labels = asar_getalllabels(&num_labels);
  if (num_labels > 0) {
    std::cout << "🏷️  Extracted " << num_labels << " symbols:" << std::endl;
    for (int i = 0; i < std::min(10, num_labels); i++) {  // Show first 10
      std::cout << "  " << labels[i].name << " @ $" 
                << std::hex << std::uppercase << labels[i].location << std::endl;
    }
    if (num_labels > 10) {
      std::cout << "  ... and " << (num_labels - 10) << " more" << std::endl;
    }
  }
  
  return absl::OkStatus();
}

absl::Status CreatePatch::Run(const std::vector<std::string>& arg_vec) {
  std::vector<uint8_t> source;
  std::vector<uint8_t> target;
  std::vector<uint8_t> patch;
  // Create patch
  util::CreateBpsPatch(source, target, patch);

  // Save patch to file
  // std::ofstream patchFile("patch.bps", ios::binary);
  // patchFile.write(reinterpret_cast<const char*>(patch.data()),
  // patch.size()); patchFile.close();
  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze