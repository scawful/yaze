#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

#include "app/core/platform/file_dialog.h"

std::string FileDialogWrapper::ShowOpenFileDialog() {
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

std::string FileDialogWrapper::ShowOpenFolderDialog() {
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

std::vector<std::string> FileDialogWrapper::GetFilesInFolder(const std::string& folder) {
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

std::vector<std::string> FileDialogWrapper::GetSubdirectoriesInFolder(const std::string& folder) {
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