#ifndef YAZE_ROM_NATIVE_OVERWORLD_STORAGE_H_
#define YAZE_ROM_NATIVE_OVERWORLD_STORAGE_H_

#include <array>
#include <cstddef>
#include <cstdint>

namespace yaze::rom::internal {
struct NativeOverworldTableStorage {
  uint16_t id;
  size_t native_pc;
  uint16_t native_first;
  uint16_t native_count;
};
// Existing reader storage, not allocation inferred from unreferenced bytes.
inline constexpr std::array<NativeOverworldTableStorage, 31> kProperties = {{
    {1, 0x140000, 0, 160},  {2, 0x140160, 0, 160},  {3, 0x140200, 0, 160},
    {4, 0x1402A0, 0, 160},  {5, 0x140340, 0, 160},  {6, 0x140480, 0, 160},
    {19, 0x1417F8, 0, 160}, {24, 0x166E1, 128, 32}, {25, 0x16701, 128, 32},
    {26, 0x7C9C, 0, 160},   {27, 0x16821, 128, 32}, {28, 0x14403, 64, 96},
    {31, 0x4C635, 0, 160},  {32, 0x14303, 0, 64},   {33, 0x14343, 0, 64},
    {34, 0x14383, 0, 64},   {35, 0x143C3, 0, 64},   {36, 0x166E1, 128, 32},
    {37, 0x16701, 128, 32}, {38, 0x16701, 128, 32}, {39, 0x166E1, 128, 32},
    {7, 0x140998, 0, 160},  {8, 0x140A38, 0, 160},  {9, 0x140B78, 0, 160},
    {10, 0x140CB8, 0, 160}, {11, 0x140DF8, 0, 160}, {12, 0x140F38, 0, 160},
    {13, 0x141078, 0, 160}, {14, 0x1411B8, 0, 160}, {15, 0x1412F8, 0, 160},
    {30, 0x1788D, 0, 160},
}};
inline constexpr std::array<NativeOverworldTableStorage, 9>
    kOtherNativeStorage = {{
        {16, 0x141438, 0, 160},
        {17, 0x141578, 0, 160},
        {18, 0x1416B8, 0, 160},
        {20, 0x1794D, 0, 160},
        {21, 0x17B2D, 0, 160},
        {22, 0x12644, 0, 160},
        {23, 0x12784, 0, 160},
        // Native LW/DW actor selectors are not expanded directory properties.
        {500, 0x7A41, 0, 256},
        {501, 0x7B41, 0, 256},
    }};

}  // namespace yaze::rom::internal
#endif
