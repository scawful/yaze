#ifndef YAZE_APP_CORE_PLATFORM_CLIPBOARD_H
#define YAZE_APP_CORE_PLATFORM_CLIPBOARD_H

#ifdef _WIN32

void CopyImageToClipboard(const std::vector<uint8_t>& data);
void GetImageFromClipboard(std::vector<uint8_t>& data, int& width, int& height);

#elif defined(__APPLE__)

#include <vector>

void CopyImageToClipboard(const std::vector<uint8_t>& data);
void GetImageFromClipboard(std::vector<uint8_t>& data, int& width, int& height);

#elif defined(__linux__)

#include <vector>

void CopyImageToClipboard(const std::vector<uint8_t>& data) {
  std::cout << "CopyImageToClipboard() is not implemented on Linux."
            << std::endl;
}
void GetImageFromClipboard(std::vector<uint8_t>& data, int& width,
                           int& height) {
  std::cout << "GetImageFromClipboard() is not implemented on Linux."
            << std::endl;
}

#endif

#endif  // YAZE_APP_CORE_PLATFORM_CLIPBOARD_H