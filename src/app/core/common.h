#ifndef YAZE_CORE_COMMON_H
#define YAZE_CORE_COMMON_H

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace yaze {

/**
 * @namespace yaze::core
 * @brief Core application logic and utilities.
 */
namespace core {

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

}  // namespace core
}  // namespace yaze

#endif
