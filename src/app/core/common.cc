#include "common.h"

#include <cstdint>

namespace yaze {
namespace app {
namespace core {

unsigned int SnesToPc(unsigned int addr) {
  if (addr >= 0x808000) {
    addr -= 0x808000;
  }
  unsigned int temp = (addr & 0x7FFF) + ((addr / 2) & 0xFF8000);
  return (temp + 0x0);
}

int AddressFromBytes(uint8_t addr1, uint8_t addr2, uint8_t addr3) {
  return (addr1 << 16) | (addr2 << 8) | addr3;
}


}  // namespace core
}  // namespace app
}  // namespace premia
