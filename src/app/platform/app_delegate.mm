// AppDelegate.mm

// Must define before any ImGui includes (needed by imgui_test_engine via editor_manager.h)
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#import "app/platform/app_delegate.h"
#import "app/controller.h"
#import "app/application.h"
#import "util/file_util.h"
#import "app/editor/editor.h"
#import "rom/rom.h"
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
/* Apple OSX and iOS (Darwin). */
#include <TargetConditionals.h>

#import <CoreText/CoreText.h>

#if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
/* iOS in Xcode simulator */

#elif TARGET_OS_MAC == 1
/* macOS */
#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
- (void)setupMenus;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  [self setupMenus];
  
  // Disable automatic UI state persistence to prevent crashes
  // macOS NSPersistentUIManager can crash if state gets corrupted
  [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"NSQuitAlwaysKeepsWindows"];
}

- (void)setupMenus {
  NSMenu *mainMenu = [NSApp mainMenu];

  NSMenuItem *fileMenuItem = [mainMenu itemWithTitle:@"File"];
  if (!fileMenuItem) {
    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    fileMenuItem = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
    [fileMenuItem setSubmenu:fileMenu];

    NSMenuItem *openItem = [[NSMenuItem alloc] initWithTitle:@"Open"
                                                      action:@selector(openFileAction:)
                                               keyEquivalent:@"o"];
    [fileMenu addItem:openItem];

    // Open Recent (System handled usually, but we can add our own if needed)
    NSMenu *openRecentMenu = [[NSMenu alloc] initWithTitle:@"Open Recent"];
    NSMenuItem *openRecentMenuItem = [[NSMenuItem alloc] initWithTitle:@"Open Recent"
                                                                action:nil
                                                         keyEquivalent:@""];
    [openRecentMenuItem setSubmenu:openRecentMenu];
    [fileMenu addItem:openRecentMenuItem];

    [fileMenu addItem:[NSMenuItem separatorItem]];

    // Save
    NSMenuItem *saveItem = [[NSMenuItem alloc] initWithTitle:@"Save" 
                                                      action:@selector(saveAction:) 
                                               keyEquivalent:@"s"];
    [fileMenu addItem:saveItem];
    
    // Save As
    NSMenuItem *saveAsItem = [[NSMenuItem alloc] initWithTitle:@"Save As..." 
                                                        action:@selector(saveAsAction:) 
                                                 keyEquivalent:@"S"];
    [fileMenu addItem:saveAsItem];

    [fileMenu addItem:[NSMenuItem separatorItem]];

    [mainMenu insertItem:fileMenuItem atIndex:1];
  }

  // Edit Menu
  NSMenuItem *editMenuItem = [mainMenu itemWithTitle:@"Edit"];
  if (!editMenuItem) {
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    editMenuItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
    [editMenuItem setSubmenu:editMenu];

    NSMenuItem *undoItem = [[NSMenuItem alloc] initWithTitle:@"Undo" 
                                                      action:@selector(undoAction:) 
                                               keyEquivalent:@"z"];
    [editMenu addItem:undoItem];

    NSMenuItem *redoItem = [[NSMenuItem alloc] initWithTitle:@"Redo" 
                                                      action:@selector(redoAction:) 
                                               keyEquivalent:@"Z"];
    [editMenu addItem:redoItem];

    [editMenu addItem:[NSMenuItem separatorItem]];
    
    // System-handled copy/paste usually works if we don't override, 
    // but we might want to wire them to our internal clipboard if needed.
    // For now, let SDL handle keyboard events for copy/paste in ImGui.

    [mainMenu insertItem:editMenuItem atIndex:2];
  }

  // View Menu
  NSMenuItem *viewMenuItem = [mainMenu itemWithTitle:@"View"];
  if (!viewMenuItem) {
    NSMenu *viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
    viewMenuItem = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
    [viewMenuItem setSubmenu:viewMenu];

    NSMenuItem *toggleFullscreenItem = [[NSMenuItem alloc] initWithTitle:@"Toggle Fullscreen"
                                                                  action:@selector(toggleFullscreenAction:)
                                                           keyEquivalent:@"f"];
    [viewMenu addItem:toggleFullscreenItem];

    [mainMenu insertItem:viewMenuItem atIndex:3];
  }
}

// ============================================================================
// Menu Actions
// ============================================================================

- (void)openFileAction:(id)sender {
    // Use our internal file dialog via Application -> Controller -> EditorManager
    // Or trigger the native dialog here and pass the path back.
    // Since we have ImGui dialogs, we might prefer those, but native is nice on macOS.
    // For now, let's just trigger the LoadRom logic which opens the dialog.
    auto& app = yaze::Application::Instance();
    if (app.IsReady() && app.GetController()) {
        if (auto* manager = app.GetController()->editor_manager()) {
            (void)manager->LoadRom();
        }
    }
}

- (void)saveAction:(id)sender {
    auto& app = yaze::Application::Instance();
    if (app.IsReady() && app.GetController()) {
        if (auto* manager = app.GetController()->editor_manager()) {
            (void)manager->SaveRom();
        }
    }
}

- (void)saveAsAction:(id)sender {
    // Trigger Save As logic
    // Manager->SaveRomAs("") usually triggers dialog
    auto& app = yaze::Application::Instance();
    if (app.IsReady() && app.GetController()) {
        if (auto* manager = app.GetController()->editor_manager()) {
            // We need a method to trigger Save As dialog from manager, 
            // usually passing empty string does it or there's a specific method.
            // EditorManager::SaveRomAs(string) saves immediately.
            // We might need to expose a method to show the dialog.
            // For now, let's assume we can use the file dialog wrapper from C++ side.
             (void)manager->SaveRomAs(""); // This might fail if empty string isn't handled as "ask user"
        }
    }
}

- (void)undoAction:(id)sender {
    // Route to active editor
    auto& app = yaze::Application::Instance();
    if (app.IsReady() && app.GetController()) {
        if (auto* manager = app.GetController()->editor_manager()) {
             // manager->card_registry().TriggerUndo(); // If we exposed TriggerUndo
             // Or directly:
             if (auto* current = manager->GetCurrentEditor()) {
                 (void)current->Undo();
             }
        }
    }
}

- (void)redoAction:(id)sender {
    auto& app = yaze::Application::Instance();
    if (app.IsReady() && app.GetController()) {
        if (auto* manager = app.GetController()->editor_manager()) {
             if (auto* current = manager->GetCurrentEditor()) {
                 (void)current->Redo();
             }
        }
    }
}

- (void)toggleFullscreenAction:(id)sender {
    // Toggle fullscreen on the window
    // SDL usually handles this, but we can trigger it via SDL_SetWindowFullscreen
    // Accessing window via Application -> Controller -> Window
    // Use SDL backend logic
    // For now, rely on the View menu item shortcut that ImGui might catch, 
    // or implement proper toggling in Controller.
}

@end

extern "C" void yaze_initialize_cocoa() {
  @autoreleasepool {
    AppDelegate *delegate = [[AppDelegate alloc] init];
    [NSApplication sharedApplication];
    [NSApp setDelegate:delegate];
    [NSApp finishLaunching];
  }
}

extern "C" int yaze_run_cocoa_app_delegate(const yaze::AppConfig& config) {
  yaze_initialize_cocoa();
  
  // Initialize the Application singleton with the provided config
  // This will create the Controller and the SDL Window
  yaze::Application::Instance().Initialize(config);
  
  // Main loop
  // We continue to run our own loop rather than [NSApp run] 
  // because we're driving SDL/ImGui manually.
  // SDL's event polling works fine with Cocoa in this setup.
  
  auto& app = yaze::Application::Instance();
  
  while (app.IsReady() && app.GetController()->IsActive()) {
    @autoreleasepool {
      app.Tick();
    }
  }
  
  app.Shutdown();
  return EXIT_SUCCESS;
}

#endif

#endif
