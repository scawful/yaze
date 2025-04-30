#ifndef YAZE_SNES_H_
#define YAZE_SNES_H_

#include <array>
#include <cstdint>

#include "app/gfx/bitmap.h"

namespace yaze {

class GraphicsSheetManager {
 public:
  static GraphicsSheetManager& GetInstance() {
    static GraphicsSheetManager instance;
    return instance;
  }
  GraphicsSheetManager() = default;
  virtual ~GraphicsSheetManager() = default;
  std::array<gfx::Bitmap, 223>& gfx_sheets() { return gfx_sheets_; }
  auto gfx_sheet(int i) { return gfx_sheets_[i]; }
  auto mutable_gfx_sheet(int i) { return &gfx_sheets_[i]; }
  auto mutable_gfx_sheets() { return &gfx_sheets_; }

 private:
  std::array<gfx::Bitmap, 223> gfx_sheets_;
};

inline uint32_t SnesToPc(uint32_t addr) noexcept {
  constexpr uint32_t kFastRomRegion = 0x808000;
  if (addr >= kFastRomRegion) {
    addr -= kFastRomRegion;
  }
  uint32_t temp = (addr & 0x7FFF) + ((addr / 2) & 0xFF8000);
  return (temp + 0x0);
}

inline uint32_t PcToSnes(uint32_t addr) {
  uint8_t* b = reinterpret_cast<uint8_t*>(&addr);
  b[2] = static_cast<uint8_t>(b[2] * 2);

  if (b[1] >= 0x80) {
    b[2] += 1;
  } else {
    b[1] += 0x80;
  }

  return addr;
}

inline uint32_t Get24LocalFromPC(uint8_t* data, int addr, bool pc = true) {
  uint32_t ret =
      (PcToSnes(addr) & 0xFF0000) | (data[addr + 1] << 8) | data[addr];
  if (pc) {
    return SnesToPc(ret);
  }
  return ret;
}

inline int AddressFromBytes(uint8_t bank, uint8_t high, uint8_t low) noexcept {
  return (bank << 16) | (high << 8) | low;
}

inline uint32_t MapBankToWordAddress(uint8_t bank, uint16_t addr) noexcept {
  uint32_t result = 0;
  result = (bank << 16) | addr;
  return result;
}

}  // namespace yaze

#endif  // YAZE_SNES_H_
