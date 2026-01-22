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

#if !TARGET_OS_OSX
#import "yaze-Swift.h"
#endif

#include <string>
#include <vector>
#include <algorithm>

#include "app/controller.h"
#include "app/application.h"
#include "app/platform/app_delegate.h"
#include "app/platform/font_loader.h"
#include "app/platform/ios/ios_host.h"
#include "app/platform/window.h"
#include "rom/rom.h"

#include "app/platform/sdl_compat.h"

#ifdef main
#undef main
#endif

#include "app/platform/view_controller.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"

namespace {
yaze::ios::IOSHost g_ios_host;
}  // namespace

// ----------------------------------------------------------------------------
// AppViewController
// ----------------------------------------------------------------------------

@interface AppViewController ()
@property(nonatomic, strong) UIViewController *overlayController;
@end

@implementation AppViewController {
  yaze::AppConfig app_config_;
  bool host_initialized_;
  UITouch *primary_touch_;
}

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
  SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "ambient");
  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    NSLog(@"SDL_Init failed: %s", SDL_GetError());
  }
#endif
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

  // Initialize Application singleton
  yaze::AppConfig config;
  config.rom_file = rom_filename;
  app_config_ = config;
  host_initialized_ = false;

  // Setup gesture recognizers
  _hoverGestureRecognizer =
      [[UIHoverGestureRecognizer alloc] initWithTarget:self action:@selector(HoverGesture:)];
  _hoverGestureRecognizer.cancelsTouchesInView = NO;
  _hoverGestureRecognizer.delegate = self;
  [self.view addGestureRecognizer:_hoverGestureRecognizer];

  _pinchRecognizer = [[UIPinchGestureRecognizer alloc] initWithTarget:self
                                                               action:@selector(HandlePinch:)];
  _pinchRecognizer.cancelsTouchesInView = NO;
  _pinchRecognizer.delegate = self;
  [self.view addGestureRecognizer:_pinchRecognizer];

  _longPressRecognizer =
      [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(handleLongPress:)];
  _longPressRecognizer.cancelsTouchesInView = NO;
  _longPressRecognizer.delegate = self;
  [self.view addGestureRecognizer:_longPressRecognizer];

  _swipeRecognizer = [[UISwipeGestureRecognizer alloc] initWithTarget:self
                                                               action:@selector(HandleSwipe:)];
  _swipeRecognizer.direction =
      UISwipeGestureRecognizerDirectionRight | UISwipeGestureRecognizerDirectionLeft;
  _swipeRecognizer.cancelsTouchesInView = NO;
  _swipeRecognizer.delegate = self;
  [self.view addGestureRecognizer:_swipeRecognizer];

  if (@available(iOS 9.0, *)) {
    NSArray<NSNumber *> *directTouches = @[ @(UITouchTypeDirect) ];
    _pinchRecognizer.allowedTouchTypes = directTouches;
    _longPressRecognizer.allowedTouchTypes = directTouches;
    _swipeRecognizer.allowedTouchTypes = directTouches;
  }
  
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

  self.view.multipleTouchEnabled = YES;

  self.mtkView.device = self.device;
  self.mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
  self.mtkView.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
  self.mtkView.framebufferOnly = NO;
  self.mtkView.delegate = self;

  if (!host_initialized_) {
    g_ios_host.SetMetalView((__bridge void *)self.view);
    yaze::ios::IOSHostConfig host_config;
    host_config.app_config = app_config_;
    auto status = g_ios_host.Initialize(host_config);
    if (!status.ok()) {
      NSLog(@"Failed to initialize iOS host: %s",
            std::string(status.message()).c_str());
      abort();
    }

    self.controller = yaze::Application::Instance().GetController();
    if (!self.controller) {
      NSLog(@"Failed to initialize application controller");
      abort();
    }
    host_initialized_ = true;
  }

  [self attachSwiftUIOverlayIfNeeded];
}

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self becomeFirstResponder];
}

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (void)postOverlayCommand:(NSString *)command {
  if (!command || command.length == 0) {
    return;
  }
  [[NSNotificationCenter defaultCenter]
      postNotificationName:@"yaze.overlay.command"
                    object:nil
                  userInfo:@{@"command": command}];
}

- (void)handleShowMenuCommand:(id)sender {
  [self postOverlayCommand:@"show_menu"];
}

- (void)handleOpenRomCommand:(id)sender {
  [self postOverlayCommand:@"open_rom"];
}

- (void)handleOpenProjectCommand:(id)sender {
  [self postOverlayCommand:@"open_project"];
}

- (void)handleShowPanelBrowserCommand:(id)sender {
  [self postOverlayCommand:@"show_panel_browser"];
}

- (void)handleShowCommandPaletteCommand:(id)sender {
  [self postOverlayCommand:@"show_command_palette"];
}

- (void)handleOpenSettingsCommand:(id)sender {
  [self postOverlayCommand:@"open_settings"];
}

- (void)handleOpenAiCommand:(id)sender {
  [self postOverlayCommand:@"open_ai"];
}

- (void)handleOpenBuildCommand:(id)sender {
  [self postOverlayCommand:@"open_build"];
}

- (void)handleOpenFilesCommand:(id)sender {
  [self postOverlayCommand:@"open_files"];
}

- (void)handleHideOverlayCommand:(id)sender {
  [self postOverlayCommand:@"hide_overlay"];
}

- (UIKeyCommand *)yazeKeyCommandWithTitle:(NSString *)title
                                imageName:(NSString *)imageName
                                    input:(NSString *)input
                            modifierFlags:(UIKeyModifierFlags)flags
                                   action:(SEL)action {
  UIKeyCommand *command = nil;
  if (@available(iOS 13.0, *)) {
    UIImage *image = imageName.length > 0 ? [UIImage systemImageNamed:imageName] : nil;
    command = [UIKeyCommand commandWithTitle:title
                                       image:image
                                      action:action
                                       input:input
                               modifierFlags:flags
                                propertyList:nil];
    command.discoverabilityTitle = title;
  } else {
    command = [UIKeyCommand keyCommandWithInput:input
                                  modifierFlags:flags
                                         action:action];
    command.discoverabilityTitle = title;
  }
  return command;
}

- (NSArray<UIKeyCommand *> *)keyCommands {
  if (@available(iOS 13.0, *)) {
    UIKeyCommand *menu =
        [self yazeKeyCommandWithTitle:@"Yaze Menu"
                            imageName:@"line.3.horizontal"
                                input:@"M"
                        modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                               action:@selector(handleShowMenuCommand:)];
    UIKeyCommand *openRom =
        [self yazeKeyCommandWithTitle:@"Open ROM"
                            imageName:@"folder"
                                input:@"O"
                        modifierFlags:UIKeyModifierCommand
                               action:@selector(handleOpenRomCommand:)];
    UIKeyCommand *openProject =
        [self yazeKeyCommandWithTitle:@"Open Project"
                            imageName:@"folder.badge.person.crop"
                                input:@"O"
                        modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                               action:@selector(handleOpenProjectCommand:)];
    UIKeyCommand *panelBrowser =
        [self yazeKeyCommandWithTitle:@"Panel Browser"
                            imageName:@"rectangle.stack"
                                input:@"B"
                        modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                               action:@selector(handleShowPanelBrowserCommand:)];
    UIKeyCommand *commandPalette =
        [self yazeKeyCommandWithTitle:@"Command Palette"
                            imageName:@"command"
                                input:@"P"
                        modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                               action:@selector(handleShowCommandPaletteCommand:)];
    UIKeyCommand *settings =
        [self yazeKeyCommandWithTitle:@"Settings"
                            imageName:@"gearshape"
                                input:@","
                        modifierFlags:UIKeyModifierCommand
                               action:@selector(handleOpenSettingsCommand:)];
    return @[menu, openRom, openProject, panelBrowser, commandPalette, settings];
  }
  return @[];
}

- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder {
  [super buildMenuWithBuilder:builder];
  if (@available(iOS 13.0, *)) {
    UIKeyCommand *menu =
        [self yazeKeyCommandWithTitle:@"Yaze Menu"
                            imageName:@"line.3.horizontal"
                                input:@"M"
                        modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                               action:@selector(handleShowMenuCommand:)];
    UIKeyCommand *openRom =
        [self yazeKeyCommandWithTitle:@"Open ROM"
                            imageName:@"folder"
                                input:@"O"
                        modifierFlags:UIKeyModifierCommand
                               action:@selector(handleOpenRomCommand:)];
    UIKeyCommand *openProject =
        [self yazeKeyCommandWithTitle:@"Open Project"
                            imageName:@"folder.badge.person.crop"
                                input:@"O"
                        modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                               action:@selector(handleOpenProjectCommand:)];
    UIKeyCommand *panelBrowser =
        [self yazeKeyCommandWithTitle:@"Panel Browser"
                            imageName:@"rectangle.stack"
                                input:@"B"
                        modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                               action:@selector(handleShowPanelBrowserCommand:)];
    UIKeyCommand *commandPalette =
        [self yazeKeyCommandWithTitle:@"Command Palette"
                            imageName:@"command"
                                input:@"P"
                        modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                               action:@selector(handleShowCommandPaletteCommand:)];
    UICommand *settings =
        [UICommand commandWithTitle:@"Settings"
                              image:[UIImage systemImageNamed:@"gearshape"]
                             action:@selector(handleOpenSettingsCommand:)
                       propertyList:nil];
    UICommand *aiHosts =
        [UICommand commandWithTitle:@"AI Hosts"
                              image:[UIImage systemImageNamed:@"sparkles"]
                             action:@selector(handleOpenAiCommand:)
                       propertyList:nil];
    UICommand *remoteBuild =
        [UICommand commandWithTitle:@"Remote Build"
                              image:[UIImage systemImageNamed:@"hammer"]
                             action:@selector(handleOpenBuildCommand:)
                       propertyList:nil];
    UICommand *files =
        [UICommand commandWithTitle:@"Files"
                              image:[UIImage systemImageNamed:@"doc.on.doc"]
                             action:@selector(handleOpenFilesCommand:)
                       propertyList:nil];
    UICommand *hideOverlay =
        [UICommand commandWithTitle:@"Hide Top Bar"
                              image:[UIImage systemImageNamed:@"chevron.up"]
                             action:@selector(handleHideOverlayCommand:)
                       propertyList:nil];

    UIMenu *yazeMenu = [UIMenu menuWithTitle:@"Yaze"
                                       image:[UIImage systemImageNamed:@"line.3.horizontal"]
                                  identifier:@"org.halext.yaze.menu"
                                     options:UIMenuOptionsDisplayInline
                                    children:@[
                                      menu,
                                      openRom,
                                      openProject,
                                      panelBrowser,
                                      commandPalette,
                                      settings,
                                      aiHosts,
                                      remoteBuild,
                                      files,
                                      hideOverlay
                                    ]];

    [builder insertChildMenu:yazeMenu atStartOfMenuForIdentifier:UIMenuFile];
  }
}
#endif

- (void)drawInMTKView:(MTKView *)view {
  auto& app = yaze::Application::Instance();
  if (!host_initialized_ || !app.IsReady() || !app.GetController()->IsActive()) {
    return;
  }

  // Update ImGui display size for iOS before Tick
  // Note: Tick() calls OnInput() then OnLoad() (NewFrame) then DoRender()
  // We want to update IO before NewFrame.
  // OnInput handles SDL events.
  
  ImGuiIO &io = ImGui::GetIO();
  const CGSize bounds = view.bounds.size;
  io.DisplaySize = ImVec2(bounds.width, bounds.height);

  const float scale_x =
      bounds.width > 0.0f ? view.drawableSize.width / bounds.width : 1.0f;
  const float scale_y =
      bounds.height > 0.0f ? view.drawableSize.height / bounds.height : 1.0f;
  io.DisplayFramebufferScale = ImVec2(scale_x, scale_y);

  g_ios_host.Tick();
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (self.overlayController) {
    self.overlayController.view.frame = self.view.bounds;
  }
}

- (void)attachSwiftUIOverlayIfNeeded {
  if (self.overlayController) {
    return;
  }
  Class overlayClass = NSClassFromString(@"YazeOverlayHostingController");
  if (!overlayClass) {
    NSLog(@"SwiftUI overlay controller not found");
    return;
  }
  UIViewController *overlay = [[overlayClass alloc] init];
  if (!overlay) {
    return;
  }
  overlay.view.backgroundColor = [UIColor clearColor];
  overlay.view.opaque = NO;
  overlay.view.frame = self.view.bounds;
  overlay.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self addChildViewController:overlay];
  [self.view addSubview:overlay.view];
  [overlay didMoveToParentViewController:self];
  self.overlayController = overlay;
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
  ImGuiIO &io = ImGui::GetIO();

  UITouch *active_touch = nil;
  if (primary_touch_ && [event.allTouches containsObject:primary_touch_]) {
    if (primary_touch_.phase != UITouchPhaseEnded &&
        primary_touch_.phase != UITouchPhaseCancelled) {
      active_touch = primary_touch_;
    }
  }

  if (!active_touch) {
    for (UITouch *touch in event.allTouches) {
      if (touch.type == UITouchTypeDirect &&
          touch.phase != UITouchPhaseEnded &&
          touch.phase != UITouchPhaseCancelled) {
        active_touch = touch;
        break;
      }
    }
  }

  if (!active_touch) {
    for (UITouch *touch in event.allTouches) {
      if (touch.type == UITouchTypeStylus &&
          touch.phase != UITouchPhaseEnded &&
          touch.phase != UITouchPhaseCancelled) {
        active_touch = touch;
        break;
      }
    }
  }

  if (!active_touch) {
    for (UITouch *touch in event.allTouches) {
      if (touch.phase != UITouchPhaseEnded &&
          touch.phase != UITouchPhaseCancelled) {
        active_touch = touch;
        break;
      }
    }
  }

  if (active_touch) {
    primary_touch_ = active_touch;
    ImGuiMouseSource source = ImGuiMouseSource_TouchScreen;
    if (active_touch.type == UITouchTypeStylus) {
      source = ImGuiMouseSource_Pen;
    }
    io.AddMouseSourceEvent(source);
    CGPoint touchLocation = [active_touch locationInView:self.view];
    io.AddMousePosEvent(touchLocation.x, touchLocation.y);
    bool is_down = active_touch.phase != UITouchPhaseEnded &&
                   active_touch.phase != UITouchPhaseCancelled;
    io.AddMouseButtonEvent(0, is_down);
  } else {
    primary_touch_ = nil;
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    io.AddMouseButtonEvent(0, false);
  }
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
  io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
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

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer *)otherGestureRecognizer {
  (void)gestureRecognizer;
  (void)otherGestureRecognizer;
  return YES;
}

#endif

@end

// ----------------------------------------------------------------------------
// SceneDelegate (UIScene lifecycle)
// ----------------------------------------------------------------------------

#if !TARGET_OS_OSX

@interface SceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic, strong) UIWindow *window;
@end

@implementation SceneDelegate

- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
                 options:(UISceneConnectionOptions *)connectionOptions {
  if (![scene isKindOfClass:[UIWindowScene class]]) {
    return;
  }
  UIWindowScene *windowScene = (UIWindowScene *)scene;
  UIViewController *rootViewController = [[AppViewController alloc] init];
  self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
  self.window.rootViewController = rootViewController;
  [self.window makeKeyAndVisible];
}

@end

#endif

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

@interface AppDelegate : UIResponder <UIApplicationDelegate, UIDocumentPickerDelegate>
@property(nonatomic, strong) UIWindow *window;
@property(nonatomic, copy) void (^completionHandler)(NSString *selectedFile);
@property(nonatomic, strong) UIDocumentPickerViewController *documentPicker;
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id> *)launchOptions {
  if (@available(iOS 13.0, *)) {
    return YES;
  }
  UIViewController *rootViewController = [[AppViewController alloc] init];
  self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
  self.window.rootViewController = rootViewController;
  [self.window makeKeyAndVisible];
  return YES;
}

- (UISceneConfiguration *)application:(UIApplication *)application
    configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession
                                   options:(UISceneConnectionOptions *)options {
  if (@available(iOS 13.0, *)) {
    UISceneConfiguration *configuration =
        [[UISceneConfiguration alloc] initWithName:@"Default Configuration"
                                       sessionRole:connectingSceneSession.role];
    configuration.delegateClass = [SceneDelegate class];
    configuration.sceneClass = [UIWindowScene class];
    return configuration;
  }
  return nil;
}

- (void)applicationWillTerminate:(UIApplication *)application {
  g_ios_host.Shutdown();
}

- (UIViewController *)RootViewControllerForPresenting {
  if (@available(iOS 13.0, *)) {
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
      if (scene.activationState != UISceneActivationStateForegroundActive) {
        continue;
      }
      if (![scene isKindOfClass:[UIWindowScene class]]) {
        continue;
      }
      UIWindowScene *windowScene = (UIWindowScene *)scene;
      for (UIWindow *window in windowScene.windows) {
        if (window.isKeyWindow && window.rootViewController) {
          return window.rootViewController;
        }
      }
      if (windowScene.windows.count > 0 &&
          windowScene.windows.firstObject.rootViewController) {
        return windowScene.windows.firstObject.rootViewController;
      }
    }
  }

  return self.window.rootViewController;
}

- (void)PresentDocumentPickerWithCompletionHandler:
    (void (^)(NSString *selectedFile))completionHandler
                                   allowedTypes:(NSArray<UTType*> *)allowedTypes {
  self.completionHandler = completionHandler;

  NSArray<UTType*>* documentTypes = allowedTypes;
  if (!documentTypes || documentTypes.count == 0) {
    documentTypes = @[ UTTypeData ];
  }
  UIViewController *rootViewController = [self RootViewControllerForPresenting];
  if (!rootViewController) {
    if (self.completionHandler) {
      self.completionHandler(@"");
    }
    self.completionHandler = nil;
    return;
  }
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
      // Create a temporary file path
      NSString *tempDir = NSTemporaryDirectory();
      NSString *fileName = [selectedFileURL lastPathComponent];
      NSString *tempPath = [tempDir stringByAppendingPathComponent:fileName];
      NSURL *tempURL = [NSURL fileURLWithPath:tempPath];
      
      // Copy the file to the temporary location
      NSError *error = nil;
      [[NSFileManager defaultManager] removeItemAtURL:tempURL error:nil]; // Remove if exists
      
      [selectedFileURL startAccessingSecurityScopedResource];
      BOOL success = [[NSFileManager defaultManager] copyItemAtURL:selectedFileURL toURL:tempURL error:&error];
      [selectedFileURL stopAccessingSecurityScopedResource];
      
      if (success) {
        std::string cppPath = std::string([tempPath UTF8String]);
        NSLog(@"File copied to temporary path: %s", cppPath.c_str());
        self.completionHandler(tempPath);
      } else {
        NSLog(@"Failed to copy ROM to temp directory: %@", error);
        self.completionHandler(@"");
      }
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
