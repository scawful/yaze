#include "file_path.h"

#include <iostream>
#include <string>

#if defined(__APPLE__) && defined(__MACH__)
#include <Foundation/Foundation.h>
#include <TargetConditionals.h>

#if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
std::string yaze::core::GetBundleResourcePath() {
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* resourceDirectoryPath = [bundle bundlePath];
  NSString* path = [resourceDirectoryPath stringByAppendingString:@"/"];
  return [path UTF8String];
}
#elif TARGET_OS_MAC == 1
std::string yaze::core::GetBundleResourcePath() {
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* resourceDirectoryPath = [bundle bundlePath];
  NSString* path = [resourceDirectoryPath stringByAppendingString:@"/"];
  return [path UTF8String];
}

#endif
#endif
