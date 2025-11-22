#include "util/hyrule_magic.h"

namespace yaze {
namespace zelda3 {

namespace {

// "load little endian value at the given byte offset and shift to get its
// value relative to the base offset (powers of 256, essentially)"
unsigned ldle(uint8_t const* const p_arr, unsigned const p_index) {
  uint32_t v = p_arr[p_index];
  v <<= (8 * p_index);
  return v;
}

void stle(uint8_t* const p_arr, size_t const p_index, unsigned const p_val) {
  uint8_t v = (p_val >> (8 * p_index)) & 0xff;
  p_arr[p_index] = v;
}

void stle0(uint8_t* const p_arr, unsigned const p_val) {
  stle(p_arr, 0, p_val);
}

void stle1(uint8_t* const p_arr, unsigned const p_val) {
  stle(p_arr, 1, p_val);
}

void stle2(uint8_t* const p_arr, unsigned const p_val) {
  stle(p_arr, 2, p_val);
}

void stle3(uint8_t* const p_arr, unsigned const p_val) {
  stle(p_arr, 3, p_val);
}

// Helper function to get the first byte in a little endian number
uint32_t ldle0(uint8_t const* const p_arr) {
  return ldle(p_arr, 0);
}

// Helper function to get the second byte in a little endian number
uint32_t ldle1(uint8_t const* const p_arr) {
  return ldle(p_arr, 1);
}

// Helper function to get the third byte in a little endian number
uint32_t ldle2(uint8_t const* const p_arr) {
  return ldle(p_arr, 2);
}

// Helper function to get the third byte in a little endian number
uint32_t ldle3(uint8_t const* const p_arr) {
  return ldle(p_arr, 3);
}

}  // namespace

void stle16b_i(uint8_t* const p_arr, size_t const p_index,
               uint16_t const p_val) {
  stle16b(p_arr + (p_index * 2), p_val);
}

void stle16b(uint8_t* const p_arr, uint16_t const p_val) {
  stle0(p_arr, p_val);
  stle1(p_arr, p_val);
}

uint16_t ldle16b(uint8_t const* const p_arr) {
  uint16_t v = 0;
  v |= (ldle0(p_arr) | ldle1(p_arr));
  return v;
}

uint16_t ldle16b_i(uint8_t const* const p_arr, size_t const p_index) {
  return ldle16b(p_arr + (2 * p_index));
}

}  // namespace zelda3
}  // namespace yaze