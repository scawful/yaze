#ifndef YAZE_UTIL_HYRULE_MAGIC_H
#define YAZE_UTIL_HYRULE_MAGIC_H

#include <cstdint>
#include <cstring>

namespace yaze {
namespace zelda3 {
/**
 * @brief Store little endian 16-bit value using a byte pointer, offset by an
 * index before dereferencing
 */
void stle16b_i(uint8_t *const p_arr, size_t const p_index,
               uint16_t const p_val);

void stle16b(uint8_t *const p_arr, uint16_t const p_val);

/**
 * @brief Load little endian halfword (16-bit) dereferenced from an arrays of
 * bytes. This version provides an index that will be multiplied by 2 and added
 * to the base address.
 */
uint16_t ldle16b_i(uint8_t const *const p_arr, size_t const p_index);

// Load little endian halfword (16-bit) dereferenced from
uint16_t ldle16b(uint8_t const *const p_arr);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_UTIL_HYRULE_MAGIC_H