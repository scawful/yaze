#include "util/image_clipboard.h"

namespace yaze::util {

// SDL's clipboard is text-only; only macOS has an image bridge so far.
bool ImageClipboardSupported() {
  return false;
}

absl::Status SetClipboardPng(const std::vector<uint8_t>&, const std::string&) {
  return absl::UnimplementedError("Image clipboard is only available on macOS");
}

absl::StatusOr<std::vector<uint8_t>> GetClipboardPng(const std::string&) {
  return absl::UnimplementedError("Image clipboard is only available on macOS");
}

}  // namespace yaze::util
