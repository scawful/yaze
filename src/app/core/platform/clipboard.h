#ifdef _WIN32

#elif defined(__APPLE__)

#include <vector>

void CopyImageToClipboard(const std::vector<uint8_t>& data);
void GetImageFromClipboard(std::vector<uint8_t>& data, int& width, int& height);

#endif