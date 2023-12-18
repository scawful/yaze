#include "clipboard.h"

#include <iostream>
#include <vector>

#import <Cocoa/Cocoa.h>

void CopyImageToClipboard(const std::vector<uint8_t>& pngData) {
  NSData* data = [NSData dataWithBytes:pngData.data() length:pngData.size()];
  NSImage* image = [[NSImage alloc] initWithData:data];

  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  [pasteboard clearContents];
  [pasteboard writeObjects:@[ image ]];
}

void GetImageFromClipboard(std::vector<uint8_t>& pixel_data, int& width, int& height) {
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  NSArray* classArray = [NSArray arrayWithObject:[NSImage class]];
  NSDictionary* options = [NSDictionary dictionary];

  NSImage* image = [pasteboard readObjectsForClasses:classArray options:options].firstObject;
  if (!image) {
    width = height = 0;
    return;
  }

  // Assuming the image is in an RGBA format
  CGImageRef cgImage = [image CGImageForProposedRect:nil context:nil hints:nil];
  width = (int)CGImageGetWidth(cgImage);
  height = (int)CGImageGetHeight(cgImage);

  size_t bytesPerRow = 4 * width;
  size_t totalBytes = bytesPerRow * height;
  pixel_data.resize(totalBytes);

  CGContextRef context = CGBitmapContextCreate(
      pixel_data.data(), width, height, 8, bytesPerRow, CGColorSpaceCreateDeviceRGB(),
      kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
  CGContextDrawImage(context, CGRectMake(0, 0, width, height), cgImage);
  CGContextRelease(context);
}