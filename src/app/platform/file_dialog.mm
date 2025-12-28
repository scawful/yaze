#include "util/file_util.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "core/features.h"
#include "util/platform_paths.h"

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
#import <dispatch/dispatch.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "app/platform/app_delegate.h"

@interface AppDelegate : UIResponder <UIApplicationDelegate, UIDocumentPickerDelegate>
@end

@interface AppDelegate (FileDialog)
- (void)PresentDocumentPickerWithCompletionHandler:
            (void (^)(NSString *selectedFile))completionHandler
                                   allowedTypes:(NSArray<UTType*> *)allowedTypes;
@end

namespace {
std::string TrimCopy(const std::string& input) {
  const auto start = input.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) {
    return "";
  }
  const auto end = input.find_last_not_of(" \t\n\r");
  return input.substr(start, end - start + 1);
}

std::vector<std::string> SplitFilterSpec(const std::string& spec) {
  std::vector<std::string> tokens;
  std::string current;
  for (char ch : spec) {
    if (ch == ',') {
      std::string trimmed = TrimCopy(current);
      if (!trimmed.empty() && trimmed[0] == '.') {
        trimmed.erase(0, 1);
      }
      if (!trimmed.empty()) {
        tokens.push_back(trimmed);
      }
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  std::string trimmed = TrimCopy(current);
  if (!trimmed.empty() && trimmed[0] == '.') {
    trimmed.erase(0, 1);
  }
  if (!trimmed.empty()) {
    tokens.push_back(trimmed);
  }
  return tokens;
}

NSArray<UTType*>* BuildAllowedTypes(const yaze::util::FileDialogOptions& options) {
  if (options.filters.empty()) {
    return @[ UTTypeData ];
  }

  bool allow_all = false;
  NSMutableArray<UTType*>* types = [NSMutableArray array];

  for (const auto& filter : options.filters) {
    const std::string spec = TrimCopy(filter.spec);
    if (spec.empty() || spec == "*") {
      allow_all = true;
      continue;
    }

    for (const auto& token : SplitFilterSpec(spec)) {
      if (token == "*") {
        allow_all = true;
        continue;
      }

      NSString* ext = [NSString stringWithUTF8String:token.c_str()];
      UTType* type = [UTType typeWithFilenameExtension:ext];
      if (!type) {
        NSString* identifier = [NSString stringWithUTF8String:token.c_str()];
        type = [UTType typeWithIdentifier:identifier];
      }
      if (type) {
        [types addObject:type];
      }
    }
  }

  if (allow_all || [types count] == 0) {
    return @[ UTTypeData ];
  }

  return types;
}

std::filesystem::path ResolveDocumentsPath() {
  auto docs_result = yaze::util::PlatformPaths::GetUserDocumentsDirectory();
  if (docs_result.ok()) {
    return *docs_result;
  }
  auto temp_result = yaze::util::PlatformPaths::GetTempDirectory();
  if (temp_result.ok()) {
    return *temp_result;
  }
  std::error_code ec;
  auto cwd = std::filesystem::current_path(ec);
  if (!ec) {
    return cwd;
  }
  return std::filesystem::path(".");
}

std::string NormalizeExtension(const std::string& ext) {
  if (ext.empty()) {
    return "";
  }
  if (ext.front() == '.') {
    return ext;
  }
  return "." + ext;
}

std::string BuildSaveFilename(const std::string& default_name,
                              const std::string& default_extension) {
  std::string name = default_name.empty() ? "yaze_output" : default_name;
  const std::string normalized_ext = NormalizeExtension(default_extension);
  if (!normalized_ext.empty()) {
    auto dot_pos = name.find_last_of('.');
    if (dot_pos == std::string::npos || dot_pos == 0) {
      name += normalized_ext;
    }
  }
  return name;
}

void ShowOpenFileDialogImpl(NSArray<UTType*>* allowed_types,
                            void (^completionHandler)(std::string)) {
  AppDelegate *appDelegate = (AppDelegate *)[UIApplication sharedApplication].delegate;
  if (!appDelegate) {
    completionHandler("");
    return;
  }
  [appDelegate PresentDocumentPickerWithCompletionHandler:^(NSString *filePath) {
    completionHandler(std::string([filePath UTF8String]));
  }
                                            allowedTypes:allowed_types];
}

std::string ShowOpenFileDialogSync(
    const yaze::util::FileDialogOptions& options) {
  __block std::string result;
  __block bool done = false;
  NSArray<UTType*>* allowed_types = BuildAllowedTypes(options);

  auto present_picker = ^{
    ShowOpenFileDialogImpl(allowed_types, ^(std::string filePath) {
      result = filePath;
      done = true;
    });
  };

  if ([NSThread isMainThread]) {
    present_picker();
    // Run a nested loop to keep UI responsive while waiting on selection.
    while (!done) {
      [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                               beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }
  } else {
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    dispatch_async(dispatch_get_main_queue(), ^{
      ShowOpenFileDialogImpl(allowed_types, ^(std::string filePath) {
        result = filePath;
        dispatch_semaphore_signal(semaphore);
      });
    });
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  }

  return result;
}
}  // namespace

std::string yaze::util::FileDialogWrapper::ShowOpenFileDialog(
    const FileDialogOptions& options) {
  return ShowOpenFileDialogSync(options);
}

std::string yaze::util::FileDialogWrapper::ShowOpenFileDialog() {
  return ShowOpenFileDialog(FileDialogOptions{});
}

std::string yaze::util::FileDialogWrapper::ShowOpenFileDialogNFD() {
  return ShowOpenFileDialog(FileDialogOptions{});
}

std::string yaze::util::FileDialogWrapper::ShowOpenFileDialogBespoke() {
  return ShowOpenFileDialog(FileDialogOptions{});
}

void yaze::util::FileDialogWrapper::ShowOpenFileDialogAsync(
    const FileDialogOptions& options,
    std::function<void(const std::string&)> callback) {
  if (!callback) {
    return;
  }
  NSArray<UTType*>* allowed_types = BuildAllowedTypes(options);
  auto callback_ptr =
      std::make_shared<std::function<void(const std::string&)>>(
          std::move(callback));

  auto present_picker = ^{
    ShowOpenFileDialogImpl(allowed_types, ^(std::string filePath) {
      (*callback_ptr)(filePath);
    });
  };

  if ([NSThread isMainThread]) {
    present_picker();
  } else {
    dispatch_async(dispatch_get_main_queue(), present_picker);
  }
}

std::string yaze::util::FileDialogWrapper::ShowOpenFolderDialog() {
  return ResolveDocumentsPath().string();
}

std::string yaze::util::FileDialogWrapper::ShowSaveFileDialog(
    const std::string& default_name, const std::string& default_extension) {
  const auto base_dir = ResolveDocumentsPath();
  const std::string filename = BuildSaveFilename(default_name, default_extension);
  return (base_dir / filename).string();
}

std::string yaze::util::FileDialogWrapper::ShowSaveFileDialogNFD(
    const std::string& default_name, const std::string& default_extension) {
  return ShowSaveFileDialog(default_name, default_extension);
}

std::string yaze::util::FileDialogWrapper::ShowSaveFileDialogBespoke(
    const std::string& default_name, const std::string& default_extension) {
  return ShowSaveFileDialog(default_name, default_extension);
}

std::vector<std::string> yaze::util::FileDialogWrapper::GetFilesInFolder(
    const std::string &folder) {
  std::vector<std::string> files;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
    if (ec) {
      break;
    }
    if (entry.is_regular_file()) {
      files.push_back(entry.path().string());
    }
  }
  return files;
}

std::vector<std::string> yaze::util::FileDialogWrapper::GetSubdirectoriesInFolder(
    const std::string &folder) {
  std::vector<std::string> directories;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
    if (ec) {
      break;
    }
    if (entry.is_directory()) {
      directories.push_back(entry.path().string());
    }
  }
  return directories;
}

std::string yaze::util::GetBundleResourcePath() {
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* resourceDirectoryPath = [bundle bundlePath];
  NSString* path = [resourceDirectoryPath stringByAppendingString:@"/"];
  return [path UTF8String];
}

#elif TARGET_OS_MAC == 1
/* macOS */

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace {
std::string TrimCopy(const std::string& input) {
  const auto start = input.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) {
    return "";
  }
  const auto end = input.find_last_not_of(" \t\n\r");
  return input.substr(start, end - start + 1);
}

std::vector<std::string> SplitFilterSpec(const std::string& spec) {
  std::vector<std::string> tokens;
  std::string current;
  for (char ch : spec) {
    if (ch == ',') {
      std::string trimmed = TrimCopy(current);
      if (!trimmed.empty() && trimmed[0] == '.') {
        trimmed.erase(0, 1);
      }
      if (!trimmed.empty()) {
        tokens.push_back(trimmed);
      }
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  std::string trimmed = TrimCopy(current);
  if (!trimmed.empty() && trimmed[0] == '.') {
    trimmed.erase(0, 1);
  }
  if (!trimmed.empty()) {
    tokens.push_back(trimmed);
  }
  return tokens;
}

std::vector<std::string> CollectExtensions(
    const yaze::util::FileDialogOptions& options, bool* allow_all) {
  std::vector<std::string> extensions;
  if (!allow_all) {
    return extensions;
  }
  *allow_all = false;

  for (const auto& filter : options.filters) {
    const std::string spec = TrimCopy(filter.spec);
    if (spec.empty() || spec == "*") {
      *allow_all = true;
      continue;
    }

    for (const auto& token : SplitFilterSpec(spec)) {
      if (token == "*") {
        *allow_all = true;
      } else {
        extensions.push_back(token);
      }
    }
  }

  return extensions;
}

std::string ShowOpenFileDialogBespokeWithOptions(
    const yaze::util::FileDialogOptions& options) {
  NSOpenPanel* openPanel = [NSOpenPanel openPanel];
  [openPanel setCanChooseFiles:YES];
  [openPanel setCanChooseDirectories:NO];
  [openPanel setAllowsMultipleSelection:NO];

  bool allow_all = false;
  std::vector<std::string> extensions = CollectExtensions(options, &allow_all);
  if (allow_all || extensions.empty()) {
    [openPanel setAllowedFileTypes:nil];
  } else {
    NSMutableArray<NSString*>* allowed_types = [NSMutableArray array];
    for (const auto& extension : extensions) {
      NSString* ext = [NSString stringWithUTF8String:extension.c_str()];
      if (ext) {
        [allowed_types addObject:ext];
      }
    }
    [openPanel setAllowedFileTypes:allowed_types];
  }
  
  if ([openPanel runModal] == NSModalResponseOK) {
    NSURL* url = [[openPanel URLs] objectAtIndex:0];
    NSString* path = [url path];
    return std::string([path UTF8String]);
  }

  return "";
}

std::string ShowOpenFileDialogNFDWithOptions(
    const yaze::util::FileDialogOptions& options) {
#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
  NFD_Init();
  nfdu8char_t* out_path = NULL;
  const nfdu8filteritem_t* filter_list = nullptr;
  size_t filter_count = 0;
  std::vector<nfdu8filteritem_t> filter_items;
  std::vector<std::string> filter_names;
  std::vector<std::string> filter_specs;

  if (!options.filters.empty()) {
    filter_items.reserve(options.filters.size());
    filter_names.reserve(options.filters.size());
    filter_specs.reserve(options.filters.size());

    for (const auto& filter : options.filters) {
      std::string label = filter.label.empty() ? "Files" : filter.label;
      std::string spec = filter.spec.empty() ? "*" : filter.spec;
      filter_names.push_back(label);
      filter_specs.push_back(spec);
      filter_items.push_back(
          {filter_names.back().c_str(), filter_specs.back().c_str()});
    }

    filter_list = filter_items.data();
    filter_count = filter_items.size();
  }

  nfdopendialogu8args_t args = {0};
  args.filterList = filter_list;
  args.filterCount = filter_count;

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
  return ShowOpenFileDialogBespokeWithOptions(options);
#endif
}
}  // namespace

std::string yaze::util::FileDialogWrapper::ShowOpenFileDialogBespoke() {
  return ShowOpenFileDialogBespokeWithOptions(FileDialogOptions{});
}

void yaze::util::FileDialogWrapper::ShowOpenFileDialogAsync(
    const FileDialogOptions& options,
    std::function<void(const std::string&)> callback) {
  if (!callback) {
    return;
  }
  callback(ShowOpenFileDialog(options));
}

std::string yaze::util::FileDialogWrapper::ShowSaveFileDialogBespoke(const std::string& default_name, 
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
std::string yaze::util::FileDialogWrapper::ShowOpenFileDialog(
    const FileDialogOptions& options) {
  if (core::FeatureFlags::get().kUseNativeFileDialog) {
    return ShowOpenFileDialogNFDWithOptions(options);
  }
  return ShowOpenFileDialogBespokeWithOptions(options);
}

std::string yaze::util::FileDialogWrapper::ShowOpenFileDialog() {
  return ShowOpenFileDialog(FileDialogOptions{});
}

std::string yaze::util::FileDialogWrapper::ShowOpenFolderDialog() {
  if (core::FeatureFlags::get().kUseNativeFileDialog) {
    return ShowOpenFolderDialogNFD();
  } else {
    return ShowOpenFolderDialogBespoke();
  }
}

std::string yaze::util::FileDialogWrapper::ShowSaveFileDialog(const std::string& default_name, 
                                                             const std::string& default_extension) {
  if (core::FeatureFlags::get().kUseNativeFileDialog) {
    return ShowSaveFileDialogNFD(default_name, default_extension);
  } else {
    return ShowSaveFileDialogBespoke(default_name, default_extension);
  }
}

// NFD implementation for macOS (fallback to bespoke if NFD not available)
std::string yaze::util::FileDialogWrapper::ShowOpenFileDialogNFD() {
  return ShowOpenFileDialogNFDWithOptions(FileDialogOptions{});
}

std::string yaze::util::FileDialogWrapper::ShowOpenFolderDialogNFD() {
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

std::string yaze::util::FileDialogWrapper::ShowSaveFileDialogNFD(const std::string& default_name, 
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

std::string yaze::util::FileDialogWrapper::ShowOpenFolderDialogBespoke() {
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

std::vector<std::string> yaze::util::FileDialogWrapper::GetFilesInFolder(
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

std::vector<std::string> yaze::util::FileDialogWrapper::GetSubdirectoriesInFolder(
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

std::string yaze::util::GetBundleResourcePath() {
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* resourceDirectoryPath = [bundle bundlePath];
  NSString* path = [resourceDirectoryPath stringByAppendingString:@"/"];
  return [path UTF8String];
}

#else
// Unsupported platform
#endif  // TARGET_OS_MAC

#endif  // __APPLE__ && __MACH__
