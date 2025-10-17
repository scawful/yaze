// yaze iOS Application
// Uses SDL2 and ImGui
//
// This file implements the iOS-specific entry point and UI management for yaze.
// It integrates with the modern Controller API and EditorManager infrastructure.
//
// Key components:
// - AppViewController: Main view controller managing the MTKView and Controller lifecycle
// - AppDelegate: iOS app lifecycle management and document picker integration
// - Touch gesture handlers: Maps iOS gestures to ImGui input events
//
// Updated to use:
// - Modern Controller::OnEntry/OnLoad/DoRender API
// - EditorManager for ROM management (no SharedRom singleton)
// - Proper SDL2 initialization for iOS
// - Updated ImGui backends (SDL2 renderer, not Metal directly)

#import <Foundation/Foundation.h>

#if TARGET_OS_OSX
#import <Cocoa/Cocoa.h>
#else
#import <UIKit/UIKit.h>
#endif

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <string>
#include <vector>
#include <algorithm>

#include "app/controller.h"
#include "app/platform/app_delegate.h"
#include "app/platform/font_loader.h"
#include "app/platform/window.h"
#include "app/rom.h"

#include <SDL.h>

#ifdef main
#undef main
#endif

#include "app/platform/view_controller.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"

// ----------------------------------------------------------------------------
// AppViewController
// ----------------------------------------------------------------------------

@implementation AppViewController

- (instancetype)initWithNibName:(nullable NSString *)nibNameOrNil
                         bundle:(nullable NSBundle *)nibBundleOrNil {
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];

  _device = MTLCreateSystemDefaultDevice();
  _commandQueue = [_device newCommandQueue];

  if (!self.device) {
    NSLog(@"Metal is not supported");
    abort();
  }

  // Initialize SDL for iOS
  SDL_SetMainReady();
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
  SDL_iOSSetEventPump(SDL_TRUE);
#endif
  
  // Parse command line arguments
  int argc = NSProcessInfo.processInfo.arguments.count;
  char **argv = new char *[argc];
  for (int i = 0; i < argc; i++) {
    NSString *arg = NSProcessInfo.processInfo.arguments[i];
    const char *cString = [arg UTF8String];
    argv[i] = new char[strlen(cString) + 1];
    strcpy(argv[i], cString);
  }

  std::string rom_filename = "";
  if (argc > 1) {
    rom_filename = argv[1];
  }
  
  // Clean up argv
  for (int i = 0; i < argc; i++) {
    delete[] argv[i];
  }
  delete[] argv;
  
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
  SDL_iOSSetEventPump(SDL_FALSE);
#endif

  // Enable native IME
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

  // Create and initialize controller with modern API
  _controller = new yaze::Controller();
  auto init_status = _controller->OnEntry(rom_filename);
  if (!init_status.ok()) {
    NSLog(@"Failed to initialize controller: %s", init_status.message().data());
    abort();
  }

  // Setup gesture recognizers
  _hoverGestureRecognizer =
      [[UIHoverGestureRecognizer alloc] initWithTarget:self action:@selector(HoverGesture:)];
  [self.view addGestureRecognizer:_hoverGestureRecognizer];

  _pinchRecognizer = [[UIPinchGestureRecognizer alloc] initWithTarget:self
                                                               action:@selector(HandlePinch:)];
  [self.view addGestureRecognizer:_pinchRecognizer];

  _longPressRecognizer =
      [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(handleLongPress:)];
  [self.view addGestureRecognizer:_longPressRecognizer];

  _swipeRecognizer = [[UISwipeGestureRecognizer alloc] initWithTarget:self
                                                               action:@selector(HandleSwipe:)];
  _swipeRecognizer.direction =
      UISwipeGestureRecognizerDirectionRight | UISwipeGestureRecognizerDirectionLeft;
  [self.view addGestureRecognizer:_swipeRecognizer];
  
  return self;
}

- (MTKView *)mtkView {
  return (MTKView *)self.view;
}

- (void)loadView {
  self.view = [[MTKView alloc] initWithFrame:CGRectMake(0, 0, 1200, 720)];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.mtkView.device = self.device;
  self.mtkView.delegate = self;
}

- (void)drawInMTKView:(MTKView *)view {
  if (!_controller->IsActive()) return;

  // Handle SDL input events
  _controller->OnInput();

  // Update ImGui display size for iOS
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize.x = view.bounds.size.width;
  io.DisplaySize.y = view.bounds.size.height;

  CGFloat framebufferScale = view.window.screen.scale ?: UIScreen.mainScreen.scale;
  io.DisplayFramebufferScale = ImVec2(framebufferScale, framebufferScale);

  // Process frame and render using Controller's API
  auto load_status = _controller->OnLoad();
  if (!load_status.ok()) {
    NSLog(@"Controller OnLoad failed: %s", load_status.message().data());
    return;
  }
  
  _controller->DoRender();
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
}

// ----------------------------------------------------------------------------
// Input processing
// ----------------------------------------------------------------------------

#if !TARGET_OS_OSX

// This touch mapping is super cheesy/hacky. We treat any touch on the screen
// as if it were a depressed left mouse button, and we don't bother handling
// multitouch correctly at all. This causes the "cursor" to behave very erratically
// when there are multiple active touches. But for demo purposes, single-touch
// interaction actually works surprisingly well.
- (void)UpdateIOWithTouchEvent:(UIEvent *)event {
  UITouch *anyTouch = event.allTouches.anyObject;
  CGPoint touchLocation = [anyTouch locationInView:self.view];
  ImGuiIO &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  io.AddMousePosEvent(touchLocation.x, touchLocation.y);

  BOOL hasActiveTouch = NO;
  for (UITouch *touch in event.allTouches) {
    if (touch.phase != UITouchPhaseEnded && touch.phase != UITouchPhaseCancelled) {
      hasActiveTouch = YES;
      break;
    }
  }
  io.AddMouseButtonEvent(0, hasActiveTouch);
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
  [self UpdateIOWithTouchEvent:event];
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
  [self UpdateIOWithTouchEvent:event];
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
  [self UpdateIOWithTouchEvent:event];
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
  [self UpdateIOWithTouchEvent:event];
}

- (void)HoverGesture:(UIHoverGestureRecognizer *)gesture {
  ImGuiIO &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  // Cast to UIGestureRecognizer to UIGestureRecognizer to get locationInView
  UIGestureRecognizer *gestureRecognizer = (UIGestureRecognizer *)gesture;
  if (gesture.zOffset < 0.50) {
    io.AddMousePosEvent([gestureRecognizer locationInView:self.view].x,
                        [gestureRecognizer locationInView:self.view].y);
  }
}

- (void)HandlePinch:(UIPinchGestureRecognizer *)gesture {
  ImGuiIO &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  io.AddMouseWheelEvent(0.0f, gesture.scale);
  UIGestureRecognizer *gestureRecognizer = (UIGestureRecognizer *)gesture;
  io.AddMousePosEvent([gestureRecognizer locationInView:self.view].x,
                      [gestureRecognizer locationInView:self.view].y);
}

- (void)HandleSwipe:(UISwipeGestureRecognizer *)gesture {
  ImGuiIO &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  if (gesture.direction == UISwipeGestureRecognizerDirectionRight) {
    io.AddMouseWheelEvent(1.0f, 0.0f);  // Swipe Right
  } else if (gesture.direction == UISwipeGestureRecognizerDirectionLeft) {
    io.AddMouseWheelEvent(-1.0f, 0.0f);  // Swipe Left
  }
  UIGestureRecognizer *gestureRecognizer = (UIGestureRecognizer *)gesture;
  io.AddMousePosEvent([gestureRecognizer locationInView:self.view].x,
                      [gestureRecognizer locationInView:self.view].y);
}

- (void)handleLongPress:(UILongPressGestureRecognizer *)gesture {
  ImGuiIO &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  io.AddMouseButtonEvent(1, gesture.state == UIGestureRecognizerStateBegan);
  UIGestureRecognizer *gestureRecognizer = (UIGestureRecognizer *)gesture;
  io.AddMousePosEvent([gestureRecognizer locationInView:self.view].x,
                      [gestureRecognizer locationInView:self.view].y);
}

#endif

@end

// ----------------------------------------------------------------------------
// AppDelegate
// ----------------------------------------------------------------------------

#if TARGET_OS_OSX

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSWindow *window;
@end

@implementation AppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
  return YES;
}

- (instancetype)init {
  if (self = [super init]) {
    NSViewController *rootViewController = [[AppViewController alloc] initWithNibName:nil
                                                                               bundle:nil];
    self.window = [[NSWindow alloc]
        initWithContentRect:NSZeroRect
                  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                            NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
                    backing:NSBackingStoreBuffered
                      defer:NO];
    self.window.contentViewController = rootViewController;
    [self.window center];
    [self.window makeKeyAndOrderFront:self];
  }
  return self;
}

@end

#else

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id> *)launchOptions {
  UIViewController *rootViewController = [[AppViewController alloc] init];
  self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
  self.window.rootViewController = rootViewController;
  [self.window makeKeyAndVisible];
  return YES;
}

- (void)applicationWillTerminate:(UIApplication *)application {
  // Controller OnExit handles cleanup
  AppViewController *viewController = (AppViewController *)self.window.rootViewController;
  if (viewController.controller) {
    viewController.controller->OnExit();
    delete viewController.controller;
    viewController.controller = nullptr;
  }
}

- (void)PresentDocumentPickerWithCompletionHandler:
    (void (^)(NSString *selectedFile))completionHandler {
  self.completionHandler = completionHandler;

  NSArray *documentTypes = @[ [UTType typeWithIdentifier:@"org.halext.sfc"] ];
  UIViewController *rootViewController = self.window.rootViewController;
  _documentPicker =
      [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:documentTypes];
  _documentPicker.delegate = self;
  _documentPicker.modalPresentationStyle = UIModalPresentationFormSheet;

  [rootViewController presentViewController:_documentPicker animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller
    didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
  NSURL *selectedFileURL = [urls firstObject];

  if (self.completionHandler) {
    if (selectedFileURL) {
      self.completionHandler(selectedFileURL.path);
      std::string fileName = std::string([selectedFileURL.path UTF8String]);

      // Extract the data from the file
      [selectedFileURL startAccessingSecurityScopedResource];

      auto data = [NSData dataWithContentsOfURL:selectedFileURL];
      uint8_t *bytes = (uint8_t *)[data bytes];
      size_t size = [data length];

      std::vector<uint8_t> rom_data;
      rom_data.resize(size);
      std::copy(bytes, bytes + size, rom_data.begin());

      // Load ROM using modern API
      // Get the AppViewController which has the controller
      AppViewController *viewController = (AppViewController *)self.window.rootViewController;
      if (viewController && viewController.controller) {
        // Access the controller's EditorManager to get the current ROM
        auto* current_rom = viewController.controller->GetCurrentRom();
        if (current_rom) {
          auto load_status = current_rom->LoadFromData(rom_data);
          if (load_status.ok()) {
            current_rom->set_filename(fileName);
            NSLog(@"ROM loaded successfully from %s", fileName.c_str());
          } else {
            NSLog(@"Failed to load ROM: %s", load_status.message().data());
          }
        } else {
          NSLog(@"No ROM instance available");
        }
      } else {
        NSLog(@"Controller not available");
      }
      
      [selectedFileURL stopAccessingSecurityScopedResource];
    } else {
      self.completionHandler(@"");
    }
  }
  self.completionHandler = nil;
  [controller dismissViewControllerAnimated:YES completion:nil];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {
  if (self.completionHandler) {
    self.completionHandler(@"");
  }
  self.completionHandler = nil;
  [controller dismissViewControllerAnimated:YES completion:nil];
}

@end

#endif

// ----------------------------------------------------------------------------
// Application main() function
// ----------------------------------------------------------------------------

#if TARGET_OS_OSX
int main(int argc, const char *argv[]) { return NSApplicationMain(argc, argv); }
#else

int main(int argc, char *argv[]) {
  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
  }
}

#endif
