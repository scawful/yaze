// AppDelegate.mm
#import "app/core/platform/app_delegate.h"
#import "app/core/controller.h"
#import "app/core/platform/file_dialog.h"
#import "app/editor/editor.h"
#import "app/rom.h"

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
// - (void)changeApplicationIcon;
@end

@implementation AppDelegate

// - (void)changeApplicationIcon {
//   NSImage *newIcon = [NSImage imageNamed:@"newIcon"];
//   [NSApp setApplicationIconImage:newIcon];
// }

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  [self setupMenus];
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

    // Open Recent
    NSMenu *openRecentMenu = [[NSMenu alloc] initWithTitle:@"Open Recent"];
    NSMenuItem *openRecentMenuItem = [[NSMenuItem alloc] initWithTitle:@"Open Recent"
                                                                action:nil
                                                         keyEquivalent:@""];
    [openRecentMenuItem setSubmenu:openRecentMenu];
    [fileMenu addItem:openRecentMenuItem];

    // Add a separator
    [fileMenu addItem:[NSMenuItem separatorItem]];

    // Save
    NSMenuItem *saveItem = [[NSMenuItem alloc] initWithTitle:@"Save" action:nil keyEquivalent:@"s"];
    [fileMenu addItem:saveItem];

    // Separator
    [fileMenu addItem:[NSMenuItem separatorItem]];

    // Options submenu
    NSMenu *optionsMenu = [[NSMenu alloc] initWithTitle:@"Options"];
    NSMenuItem *optionsMenuItem = [[NSMenuItem alloc] initWithTitle:@"Options"
                                                             action:nil
                                                      keyEquivalent:@""];
    [optionsMenuItem setSubmenu:optionsMenu];

    // Flag checkmark field
    NSMenuItem *flagItem = [[NSMenuItem alloc] initWithTitle:@"Flag"
                                                      action:@selector(toggleFlagAction:)
                                               keyEquivalent:@""];
    [flagItem setTarget:self];
    [flagItem setState:NSControlStateValueOff];
    [optionsMenu addItem:flagItem];
    [fileMenu addItem:optionsMenuItem];

    [mainMenu insertItem:fileMenuItem atIndex:1];
  }

  NSMenuItem *editMenuItem = [mainMenu itemWithTitle:@"Edit"];
  if (!editMenuItem) {
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    editMenuItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
    [editMenuItem setSubmenu:editMenu];

    NSMenuItem *undoItem = [[NSMenuItem alloc] initWithTitle:@"Undo" action:nil keyEquivalent:@"z"];

    [editMenu addItem:undoItem];

    NSMenuItem *redoItem = [[NSMenuItem alloc] initWithTitle:@"Redo" action:nil keyEquivalent:@"Z"];

    [editMenu addItem:redoItem];

    // Add a separator
    [editMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *cutItem = [[NSMenuItem alloc] initWithTitle:@"Cut"
                                                     action:@selector(cutAction:)
                                              keyEquivalent:@"x"];
    [editMenu addItem:cutItem];

    NSMenuItem *copyItem = [[NSMenuItem alloc] initWithTitle:@"Copy" action:nil keyEquivalent:@"c"];
    [editMenu addItem:copyItem];

    NSMenuItem *pasteItem = [[NSMenuItem alloc] initWithTitle:@"Paste"
                                                       action:nil
                                                keyEquivalent:@"v"];

    [editMenu addItem:pasteItem];

    // Add a separator
    [editMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *selectAllItem = [[NSMenuItem alloc] initWithTitle:@"Select All"
                                                           action:nil
                                                    keyEquivalent:@"a"];

    [editMenu addItem:selectAllItem];

    [mainMenu insertItem:editMenuItem atIndex:2];
  }

  NSMenuItem *viewMenuItem = [mainMenu itemWithTitle:@"View"];
  if (!viewMenuItem) {
    NSMenu *viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
    viewMenuItem = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
    [viewMenuItem setSubmenu:viewMenu];

    // Emulator view button
    NSMenuItem *emulatorViewItem = [[NSMenuItem alloc] initWithTitle:@"Emulator View"
                                                              action:nil
                                                       keyEquivalent:@"1"];

    [viewMenu addItem:emulatorViewItem];

    // Hex Editor View
    NSMenuItem *hexEditorViewItem = [[NSMenuItem alloc] initWithTitle:@"Hex Editor View"
                                                               action:nil
                                                        keyEquivalent:@"2"];

    [viewMenu addItem:hexEditorViewItem];

    // Disassembly view button
    NSMenuItem *disassemblyViewItem = [[NSMenuItem alloc] initWithTitle:@"Disassembly View"
                                                                 action:nil
                                                          keyEquivalent:@"3"];

    [viewMenu addItem:disassemblyViewItem];

    // Memory view button
    NSMenuItem *memoryViewItem = [[NSMenuItem alloc] initWithTitle:@"Memory View"
                                                            action:nil
                                                     keyEquivalent:@"4"];

    [viewMenu addItem:memoryViewItem];

    // Add a separator
    [viewMenu addItem:[NSMenuItem separatorItem]];

    // Toggle fullscreen button
    NSMenuItem *toggleFullscreenItem = [[NSMenuItem alloc] initWithTitle:@"Toggle Fullscreen"
                                                                  action:nil
                                                           keyEquivalent:@"f"];

    [viewMenu addItem:toggleFullscreenItem];

    [mainMenu insertItem:viewMenuItem atIndex:3];
  }

  NSMenuItem *helpMenuItem = [mainMenu itemWithTitle:@"Help"];
  if (!helpMenuItem) {
    NSMenu *helpMenu = [[NSMenu alloc] initWithTitle:@"Help"];
    helpMenuItem = [[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""];
    [helpMenuItem setSubmenu:helpMenu];

    // URL to online documentation
    NSMenuItem *documentationItem = [[NSMenuItem alloc] initWithTitle:@"Documentation"
                                                               action:nil
                                                        keyEquivalent:@"?"];
    [helpMenu addItem:documentationItem];

    [mainMenu insertItem:helpMenuItem atIndex:4];
  }
}

// Action method for the New menu item
- (void)newFileAction:(id)sender {
  NSLog(@"New File action triggered");
}

- (void)toggleFlagAction:(id)sender {
  NSMenuItem *flagItem = (NSMenuItem *)sender;
  if ([flagItem state] == NSControlStateValueOff) {
    [flagItem setState:NSControlStateValueOn];
  } else {
    [flagItem setState:NSControlStateValueOff];
  }
}

- (void)openFileAction:(id)sender {
  if (!yaze::SharedRom::shared_rom_
           ->LoadFromFile(yaze::core::FileDialogWrapper::ShowOpenFileDialog())
           .ok()) {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Error"];
    [alert setInformativeText:@"Failed to load file."];
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
  }
}

- (void)cutAction:(id)sender {
  // TODO: Implement
}

- (void)openRecentFileAction:(id)sender {
  NSLog(@"Open Recent File action triggered");
}

@end

extern "C" void yaze_initialize_cococa() {
  @autoreleasepool {
    AppDelegate *delegate = [[AppDelegate alloc] init];
    [NSApplication sharedApplication];
    [NSApp setDelegate:delegate];
    [NSApp finishLaunching];
  }
}

extern "C" int yaze_run_cocoa_app_delegate(const char *filename) {
  yaze_initialize_cococa();
  yaze::core::Controller controller;
  EXIT_IF_ERROR(controller.OnEntry(filename));
  while (controller.IsActive()) {
    @autoreleasepool {
      controller.OnInput();
      if (auto status = controller.OnLoad(); !status.ok()) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Error"];
        [alert setInformativeText:[NSString stringWithUTF8String:status.message().data()]];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
        break;
      }
      controller.DoRender();
    }
  }
  controller.OnExit();
  return EXIT_SUCCESS;
}

#endif

#endif
