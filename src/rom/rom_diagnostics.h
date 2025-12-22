#ifndef YAZE_APP_ROM_DIAGNOSTICS_H
#define YAZE_APP_ROM_DIAGNOSTICS_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace yaze {

struct SheetDiagnostics {
  uint32_t index = 0;
  uint32_t pc_offset = 0;
  uint32_t snes_address = 0;
  bool is_compressed = false;
  bool decompression_succeeded = false;
  int decomp_size_param = -1;     // The size passed to DecompressV2 (init to -1)
  size_t actual_decomp_size = 0; // The size returned
  std::vector<uint8_t> first_bytes; // First 8 bytes of raw data
};

struct GraphicsLoadDiagnostics {
  size_t rom_size = 0;
  bool header_stripped = false;
  bool checksum_valid = false;
  
  // Pointer table values
  uint32_t ptr1_loc = 0;
  uint32_t ptr2_loc = 0;
  uint32_t ptr3_loc = 0;
  
  // Sheet specific diagnostics
  std::array<SheetDiagnostics, 223> sheets;

  // Pattern detection flags
  bool size_zero_regression = false;   // DecompressV2 called with size=0
  bool header_misalignment = false;    // Pointer tables seem offset by 512
  bool all_sheets_0xFF = false;        // Symptom detection
  
  std::string ToJson() const;
  void Analyze(); // Sets the flags
};

} // namespace yaze

#endif // YAZE_APP_ROM_DIAGNOSTICS_H
