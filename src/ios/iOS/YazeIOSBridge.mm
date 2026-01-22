#import "YazeIOSBridge.h"

#include <string>

#include "app/application.h"
#include "app/controller.h"
#include "app/editor/editor_manager.h"
#include "rom/rom.h"

@implementation YazeIOSBridge

+ (void)loadRomAtPath:(NSString *)path {
  if (!path || path.length == 0) {
    return;
  }
  std::string cpp_path([path UTF8String]);
  yaze::Application::Instance().LoadRom(cpp_path);
}

+ (void)openProjectAtPath:(NSString *)path {
  if (!path || path.length == 0) {
    return;
  }
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller || !controller->editor_manager()) {
    return;
  }

  std::string cpp_path([path UTF8String]);
  (void)controller->editor_manager()->OpenRomOrProject(cpp_path);
}

+ (NSString *)currentRomTitle {
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller) {
    return @"";
  }
  auto *rom = controller->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    return @"";
  }
  return [NSString stringWithUTF8String:rom->title().c_str()];
}

@end
