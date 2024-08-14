#include "app/core/platform/clipboard.h"

#include <cstdint>
#include <vector>

namespace yaze {
namespace app {
namespace core {

void CopyImageToClipboard(const std::vector<uint8_t>& data) {}
void GetImageFromClipboard(std::vector<uint8_t>& data, int& width,
                           int& height) {}

}  // namespace core
}  // namespace app
}  // namespace yaze