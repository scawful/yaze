#ifndef YAZE_APP_CORE_PLATFORM_CLIPBOARD_H
#define YAZE_APP_CORE_PLATFORM_CLIPBOARD_H

#include <cstdint>
#include <vector>

namespace yaze {
namespace core {

void CopyImageToClipboard(const std::vector<uint8_t> &data);
void GetImageFromClipboard(std::vector<uint8_t> &data, int &width, int &height);

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_PLATFORM_CLIPBOARD_H