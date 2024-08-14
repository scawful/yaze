#include "app/core/platform/file_dialog.h"

#include <string>
#include <vector>
#include "imgui/imgui.h"

#if defined(__APPLE__) && defined(__MACH__)
/* Apple OSX and iOS (Darwin). */
#include <TargetConditionals.h>

#import <CoreText/CoreText.h>

#if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
/* iOS in Xcode simulator */
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "app/core/platform/app_delegate.h"

namespace {
static std::string selectedFile;

void ShowOpenFileDialogImpl(void (^completionHandler)(std::string)) {
  AppDelegate *appDelegate = (AppDelegate *)[UIApplication sharedApplication].delegate;
  [appDelegate PresentDocumentPickerWithCompletionHandler:^(NSString *filePath) {
    selectedFile = std::string([filePath UTF8String]);
    completionHandler(selectedFile);
  }];
}

std::string ShowOpenFileDialogSync() {
  __block std::string result;

  ShowOpenFileDialogImpl(^(std::string filePath) {
    result = filePath;
  });

  return result;
}
}

std::string FileDialogWrapper::ShowOpenFileDialog() { return ShowOpenFileDialogSync(); }

std::string FileDialogWrapper::ShowOpenFolderDialog() { return ""; }

std::vector<std::string> FileDialogWrapper::GetFilesInFolder(const std::string &folder) {
  return {};
}

std::vector<std::string> FileDialogWrapper::GetSubdirectoriesInFolder(const std::string &folder) {
  return {};
}

#elif TARGET_OS_MAC == 1
/* macOS */

#import <Cocoa/Cocoa.h>

std::string yaze::app::core::FileDialogWrapper::ShowOpenFileDialog() {
  NSOpenPanel* openPanel = [NSOpenPanel openPanel];
  [openPanel setCanChooseFiles:YES];
  [openPanel setCanChooseDirectories:NO];
  [openPanel setAllowsMultipleSelection:NO];

  if ([openPanel runModal] == NSModalResponseOK) {
    NSURL* url = [[openPanel URLs] objectAtIndex:0];
    NSString* path = [url path];
    return std::string([path UTF8String]);
  }

  return "";
}

std::string yaze::app::core::FileDialogWrapper::ShowOpenFolderDialog() {
  NSOpenPanel* openPanel = [NSOpenPanel openPanel];
  [openPanel setCanChooseFiles:NO];
  [openPanel setCanChooseDirectories:YES];
  [openPanel setAllowsMultipleSelection:NO];

  if ([openPanel runModal] == NSModalResponseOK) {
    NSURL* url = [[openPanel URLs] objectAtIndex:0];
    NSString* path = [url path];
    return std::string([path UTF8String]);
  }

  return "";
}

std::vector<std::string> yaze::app::core::FileDialogWrapper::GetFilesInFolder(
    const std::string& folder) {
  std::vector<std::string> filenames;
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSDirectoryEnumerator* enumerator =
      [fileManager enumeratorAtPath:[NSString stringWithUTF8String:folder.c_str()]];
  NSString* file;
  while (file = [enumerator nextObject]) {
    if ([file hasPrefix:@"."]) {
      continue;
    }
    filenames.push_back(std::string([file UTF8String]));
  }
  return filenames;
}

std::vector<std::string> yaze::app::core::FileDialogWrapper::GetSubdirectoriesInFolder(
    const std::string& folder) {
  std::vector<std::string> subdirectories;
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSDirectoryEnumerator* enumerator =
      [fileManager enumeratorAtPath:[NSString stringWithUTF8String:folder.c_str()]];
  NSString* file;
  while (file = [enumerator nextObject]) {
    if ([file hasPrefix:@"."]) {
      continue;
    }
    BOOL isDirectory;
    NSString* path =
        [NSString stringWithFormat:@"%@/%@", [NSString stringWithUTF8String:folder.c_str()], file];
    [fileManager fileExistsAtPath:path isDirectory:&isDirectory];
    if (isDirectory) {
      subdirectories.push_back(std::string([file UTF8String]));
    }
  }
  return subdirectories;
}
#else
// Unsupported platform
#endif  // TARGET_OS_MAC

#endif  // __APPLE__ && __MACH__