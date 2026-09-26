#ifndef YAZE_UTIL_IMAGE_CLIPBOARD_H
#define YAZE_UTIL_IMAGE_CLIPBOARD_H

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::util {

/**
 * @file image_clipboard.h
 * @brief PNG images on the system clipboard. SDL's clipboard is text-only.
 *
 * Implemented with NSPasteboard on macOS (app/platform/image_clipboard.mm);
 * other platforms return Unimplemented (app/platform/image_clipboard_stub.cc).
 * `pasteboard` names a private pasteboard, which tests use so they never
 * touch the user's clipboard; empty means the system clipboard.
 */

bool ImageClipboardSupported();

/// Replaces the clipboard contents with one PNG image.
absl::Status SetClipboardPng(const std::vector<uint8_t>& png,
                             const std::string& pasteboard = "");

/// The clipboard's image as PNG bytes. A TIFF-only image (macOS screenshots
/// and many apps) is converted to PNG. NotFound when there is no image.
absl::StatusOr<std::vector<uint8_t>> GetClipboardPng(
    const std::string& pasteboard = "");

}  // namespace yaze::util

#endif  // YAZE_UTIL_IMAGE_CLIPBOARD_H
