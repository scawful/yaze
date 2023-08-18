#ifndef YAZE_CORE_COMMON_H
#define YAZE_CORE_COMMON_H

#include <cstdint>
#include <memory>
#include <string>

namespace yaze {
namespace app {
namespace core {

uint32_t SnesToPc(uint32_t addr);
uint32_t PcToSnes(uint32_t addr);

int AddressFromBytes(uint8_t addr1, uint8_t addr2, uint8_t addr3);
int HexToDec(char *input, int length);
bool StringReplace(std::string &str, const std::string &from,
                   const std::string &to);

void stle16b_i(uint8_t *const p_arr, size_t const p_index,
               uint16_t const p_val);
uint16_t ldle16b_i(uint8_t const *const p_arr, size_t const p_index);

void stle16b(uint8_t *const p_arr, uint16_t const p_val);
void stle32b(uint8_t *const p_arr, uint32_t const p_val);

void stle32b_i(uint8_t *const p_arr, size_t const p_index,
               uint32_t const p_val);

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif