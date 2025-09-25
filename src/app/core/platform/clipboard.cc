#include "app/core/platform/clipboard.h"

#include <cstdint>
#include <vector>

namespace yaze {
namespace core {

#if YAZE_LIB_PNG == 1
void CopyImageToClipboard(const std::vector<uint8_t>& data) {}
void GetImageFromClipboard(std::vector<uint8_t>& data, int& width,
                           int& height) {}
#endif

}  // namespace core
}  // namespace yaze