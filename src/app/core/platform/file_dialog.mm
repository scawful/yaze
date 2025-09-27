#include "app/core/platform/file_dialog.h"

#include <iostream>
#include <string>
#include <vector>

#include "app/core/features.h"

#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
#include <nfd.h>
#endif

#if defined(__APPLE__) && defined(__MACH__)
/* Apple OSX and iOS (Darwin). */
#include <Foundation/Foundation.h>
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
}  // namespace

std::string yaze::core::FileDialogWrapper::ShowOpenFileDialog() { return ShowOpenFileDialogSync(); }

std::string yaze::core::FileDialogWrapper::ShowOpenFolderDialog() { return ""; }

std::vector<std::string> yaze::core::FileDialogWrapper::GetFilesInFolder(
    const std::string &folder) {
  return {};
}

std::vector<std::string> yaze::core::FileDialogWrapper::GetSubdirectoriesInFolder(
    const std::string &folder) {
  return {};
}

std::string yaze::core::GetBundleResourcePath() {
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* resourceDirectoryPath = [bundle bundlePath];
  NSString* path = [resourceDirectoryPath stringByAppendingString:@"/"];
  return [path UTF8String];
}

#elif TARGET_OS_MAC == 1
/* macOS */

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

std::string yaze::core::FileDialogWrapper::ShowOpenFileDialogBespoke() {
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

std::string yaze::core::FileDialogWrapper::ShowSaveFileDialogBespoke(const std::string& default_name, 
                                                                    const std::string& default_extension) {
  NSSavePanel* savePanel = [NSSavePanel savePanel];
  
  if (!default_name.empty()) {
    [savePanel setNameFieldStringValue:[NSString stringWithUTF8String:default_name.c_str()]];
  }
  
  if (!default_extension.empty()) {
    NSString* ext = [NSString stringWithUTF8String:default_extension.c_str()];
    [savePanel setAllowedFileTypes:@[ext]];
  }
  
  if ([savePanel runModal] == NSModalResponseOK) {
    NSURL* url = [savePanel URL];
    NSString* path = [url path];
    return std::string([path UTF8String]);
  }

  return "";
}

// Global feature flag-based dispatch methods
std::string yaze::core::FileDialogWrapper::ShowOpenFileDialog() {
  if (FeatureFlags::get().kUseNativeFileDialog) {
    return ShowOpenFileDialogNFD();
  } else {
    return ShowOpenFileDialogBespoke();
  }
}

std::string yaze::core::FileDialogWrapper::ShowOpenFolderDialog() {
  if (FeatureFlags::get().kUseNativeFileDialog) {
    return ShowOpenFolderDialogNFD();
  } else {
    return ShowOpenFolderDialogBespoke();
  }
}

std::string yaze::core::FileDialogWrapper::ShowSaveFileDialog(const std::string& default_name, 
                                                             const std::string& default_extension) {
  if (FeatureFlags::get().kUseNativeFileDialog) {
    return ShowSaveFileDialogNFD(default_name, default_extension);
  } else {
    return ShowSaveFileDialogBespoke(default_name, default_extension);
  }
}

// NFD implementation for macOS (fallback to bespoke if NFD not available)
std::string yaze::core::FileDialogWrapper::ShowOpenFileDialogNFD() {
#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
  NFD_Init();
  nfdu8char_t *out_path = NULL;
  nfdu8filteritem_t filters[1] = {{"Rom File", "sfc,smc"}};
  nfdopendialogu8args_t args = {0};
  args.filterList = filters;
  args.filterCount = 1;
  
  nfdresult_t result = NFD_OpenDialogU8_With(&out_path, &args);
  if (result == NFD_OKAY) {
    std::string file_path(out_path);
    NFD_FreePath(out_path);
    NFD_Quit();
    return file_path;
  } else if (result == NFD_CANCEL) {
    NFD_Quit();
    return "";
  }
  NFD_Quit();
  return "";
#else
  // NFD not compiled in, use bespoke
  return ShowOpenFileDialogBespoke();
#endif
}

std::string yaze::core::FileDialogWrapper::ShowOpenFolderDialogNFD() {
#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
  NFD_Init();
  nfdu8char_t *out_path = NULL;
  nfdresult_t result = NFD_PickFolderU8(&out_path, NULL);
  
  if (result == NFD_OKAY) {
    std::string folder_path(out_path);
    NFD_FreePath(out_path);
    NFD_Quit();
    return folder_path;
  } else if (result == NFD_CANCEL) {
    NFD_Quit();
    return "";
  }
  NFD_Quit();
  return "";
#else
  // NFD not compiled in, use bespoke
  return ShowOpenFolderDialogBespoke();
#endif
}

std::string yaze::core::FileDialogWrapper::ShowSaveFileDialogNFD(const std::string& default_name, 
                                                                const std::string& default_extension) {
#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
  NFD_Init();
  nfdu8char_t *out_path = NULL;
  
  nfdsavedialogu8args_t args = {0};
  if (!default_extension.empty()) {
    // Create filter for the save dialog
    static nfdu8filteritem_t filters[3] = {
      {"Theme File", "theme"},
      {"Project File", "yaze"},
      {"ROM File", "sfc,smc"}
    };
    
    if (default_extension == "theme") {
      args.filterList = &filters[0];
      args.filterCount = 1;
    } else if (default_extension == "yaze") {
      args.filterList = &filters[1]; 
      args.filterCount = 1;
    } else if (default_extension == "sfc" || default_extension == "smc") {
      args.filterList = &filters[2];
      args.filterCount = 1;
    }
  }
  
  if (!default_name.empty()) {
    args.defaultName = default_name.c_str();
  }
  
  nfdresult_t result = NFD_SaveDialogU8_With(&out_path, &args);
  if (result == NFD_OKAY) {
    std::string file_path(out_path);
    NFD_FreePath(out_path);
    NFD_Quit();
    return file_path;
  } else if (result == NFD_CANCEL) {
    NFD_Quit();
    return "";
  }
  NFD_Quit();
  return "";
#else
  // NFD not compiled in, use bespoke
  return ShowSaveFileDialogBespoke(default_name, default_extension);
#endif
}

std::string yaze::core::FileDialogWrapper::ShowOpenFolderDialogBespoke() {
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

std::vector<std::string> yaze::core::FileDialogWrapper::GetFilesInFolder(
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

std::vector<std::string> yaze::core::FileDialogWrapper::GetSubdirectoriesInFolder(
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

std::string yaze::core::GetBundleResourcePath() {
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* resourceDirectoryPath = [bundle bundlePath];
  NSString* path = [resourceDirectoryPath stringByAppendingString:@"/"];
  return [path UTF8String];
}

#else
// Unsupported platform
#endif  // TARGET_OS_MAC

#endif  // __APPLE__ && __MACH__