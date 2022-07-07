#ifndef YAZE_CORE_COMMON_H
#define YAZE_CORE_COMMON_H

#include <cstdint>

namespace yaze {
namespace app {
namespace core {

unsigned int SnesToPc(unsigned int addr);
int AddressFromBytes(uint8_t addr1, uint8_t addr2, uint8_t addr3);

}  // namespace core
}  // namespace app
}  // namespace premia

#endif