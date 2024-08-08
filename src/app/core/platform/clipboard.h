#ifndef YAZE_APP_CORE_PLATFORM_CLIPBOARD_H
#define YAZE_APP_CORE_PLATFORM_CLIPBOARD_H

#include <cstdint>
#include <vector>

void CopyImageToClipboard(const std::vector<uint8_t> &data);
void GetImageFromClipboard(std::vector<uint8_t> &data, int &width, int &height);

#endif // YAZE_APP_CORE_PLATFORM_CLIPBOARD_H