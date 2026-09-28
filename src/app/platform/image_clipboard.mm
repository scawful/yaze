#include "util/image_clipboard.h"

#include <TargetConditionals.h>

#import <Foundation/Foundation.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#endif

namespace yaze::util {

#if TARGET_OS_OSX

namespace {

NSPasteboard* Board(const std::string& name) {
  if (name.empty()) {
    return [NSPasteboard generalPasteboard];
  }
  return [NSPasteboard
      pasteboardWithName:[NSString stringWithUTF8String:name.c_str()]];
}

}  // namespace

bool ImageClipboardSupported() { return true; }

absl::Status SetClipboardPng(const std::vector<uint8_t>& png,
                             const std::string& pasteboard) {
  @autoreleasepool {
    NSPasteboard* board = Board(pasteboard);
    NSData* data = [NSData dataWithBytes:png.data() length:png.size()];
    [board clearContents];
    if (![board setData:data forType:NSPasteboardTypePNG]) {
      return absl::InternalError("The clipboard refused the PNG image");
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<uint8_t>> GetClipboardPng(
    const std::string& pasteboard) {
  @autoreleasepool {
    NSPasteboard* board = Board(pasteboard);
    NSData* data = [board dataForType:NSPasteboardTypePNG];
    if (data == nil) {
      NSData* tiff = [board dataForType:NSPasteboardTypeTIFF];
      if (tiff != nil) {
        NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData:tiff];
        data = [rep representationUsingType:NSBitmapImageFileTypePNG
                                 properties:@{}];
      }
    }
    if (data == nil || data.length == 0) {
      return absl::NotFoundError("The clipboard holds no image");
    }
    const auto* bytes = static_cast<const uint8_t*>(data.bytes);
    return std::vector<uint8_t>(bytes, bytes + data.length);
  }
}

#else  // iOS: UIPasteboard is not wired up yet.

bool ImageClipboardSupported() { return false; }

absl::Status SetClipboardPng(const std::vector<uint8_t>&, const std::string&) {
  return absl::UnimplementedError("Image clipboard is not available here");
}

absl::StatusOr<std::vector<uint8_t>> GetClipboardPng(const std::string&) {
  return absl::UnimplementedError("Image clipboard is not available here");
}

#endif

}  // namespace yaze::util
