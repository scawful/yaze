#include "rom/rom_diagnostics.h"

#include <iomanip>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "util/log.h"

namespace yaze {

void GraphicsLoadDiagnostics::Analyze() {
  size_zero_regression = false;
  header_misalignment = false;
  all_sheets_0xFF = true; // Assume true, prove false

  // Check for size zero regression
  for (const auto& sheet : sheets) {
    if (sheet.is_compressed && sheet.decomp_size_param == 0) {
      size_zero_regression = true;
    }
    
    // If any sheet succeeded and has non-0xFF data, clear the symptom flag
    if (sheet.decompression_succeeded && sheet.actual_decomp_size > 0) {
        all_sheets_0xFF = false; 
    }
  }

  if (sheets[0].pc_offset > rom_size) {
      header_misalignment = true;
  }

  if (size_zero_regression) {
      LOG_ERROR("Graphics", "CRITICAL: Graphics size zero regression detected!");
  }
  if (header_misalignment) {
      LOG_ERROR("Graphics", "CRITICAL: Graphics header misalignment detected! (Sheet 0 offset 0x%X > ROM size 0x%X)", 
                sheets[0].pc_offset, (uint32_t)rom_size);
  }
  if (all_sheets_0xFF) {
      LOG_WARN("Graphics", "WARNING: All graphics sheets appear to be empty (0xFF).");
  }

  LOG_INFO("Graphics", "Diagnostics: %d sheets processed, size_zero_fault=%s, alignment_fault=%s, empty_fault=%s",
           (int)sheets.size(), size_zero_regression ? "YES" : "NO", 
           header_misalignment ? "YES" : "NO", all_sheets_0xFF ? "YES" : "NO");
}

std::string GraphicsLoadDiagnostics::ToJson() const {
  std::ostringstream json;
  json << std::boolalpha;
  json << "{";
  json << "\"rom_size\":" << rom_size << ",";
  json << "\"header_stripped\":" << header_stripped << ",";
  json << "\"checksum_valid\":" << checksum_valid << ",";
  
  json << "\"analysis\":{";
  json << "\"size_zero_regression\":" << size_zero_regression << ",";
  json << "\"header_misalignment\":" << header_misalignment << ",";
  json << "\"all_sheets_0xFF\":" << all_sheets_0xFF;
  json << "},";

  json << "\"sheets\":[";
  for (size_t i = 0; i < sheets.size(); ++i) {
    const auto& s = sheets[i];
    if (i > 0) json << ",";
    json << "{";
    json << "\"idx\":" << s.index << ",";
    json << "\"pc\":" << s.pc_offset << ",";
    json << "\"snes\":" << s.snes_address << ",";
    json << "\"comp\":" << s.is_compressed << ",";
    json << "\"ok\":" << s.decompression_succeeded << ",";
    json << "\"param\":" << s.decomp_size_param << ",";
    json << "\"sz\":" << s.actual_decomp_size << ",";
    
    json << "\"bytes\":\"";
    for (size_t b = 0; b < s.first_bytes.size(); ++b) {
        json << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)s.first_bytes[b];
    }
    json << std::dec << "\"";
    
    json << "}";
  }
  json << "]";
  json << "}";
  return json.str();
}

} // namespace yaze
