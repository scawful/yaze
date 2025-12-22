#include "rom/rom_diagnostics.h"

#include <iomanip>
#include <sstream>

#include "absl/strings/str_cat.h"

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

  // Check for header misalignment
  // If pointer tables point to 0 or weird locations, it might be misalignment
  // A simple heuristic: if sheet 0 offset is > rom_size, it's definitely broken.
  if (sheets[0].pc_offset > rom_size) {
      header_misalignment = true;
  }
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
