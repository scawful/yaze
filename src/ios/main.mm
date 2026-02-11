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
#include "app/platform/ios/ios_platform_state.h"
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
- (void)queueOpenURL:(NSURL *)url;
@end

@implementation AppViewController {
  yaze::AppConfig app_config_;
  bool host_initialized_;
  UITouch *primary_touch_;
  NSURL *pending_open_url_;
  NSURL *security_scoped_url_;
  BOOL security_scope_granted_;
  float previous_pinch_scale_;
  float pinch_velocity_;       // Smoothed pinch velocity for zoom
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
  previous_pinch_scale_ = 1.0f;

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

  _twoFingerPanRecognizer =
      [[UIPanGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(HandleTwoFingerPan:)];
  _twoFingerPanRecognizer.minimumNumberOfTouches = 2;
  _twoFingerPanRecognizer.maximumNumberOfTouches = 2;
  _twoFingerPanRecognizer.cancelsTouchesInView = NO;
  _twoFingerPanRecognizer.delegate = self;
  [self.view addGestureRecognizer:_twoFingerPanRecognizer];

  if (@available(iOS 9.0, *)) {
    NSArray<NSNumber *> *directTouches = @[ @(UITouchTypeDirect) ];
    _pinchRecognizer.allowedTouchTypes = directTouches;
    _longPressRecognizer.allowedTouchTypes = directTouches;
    _twoFingerPanRecognizer.allowedTouchTypes = directTouches;
  }

  // Three-finger swipe left → Undo (iOS 13+ convention)
  UISwipeGestureRecognizer *threeFingerSwipeLeft =
      [[UISwipeGestureRecognizer alloc] initWithTarget:self
                                                action:@selector(handleThreeFingerSwipeLeft:)];
  threeFingerSwipeLeft.numberOfTouchesRequired = 3;
  threeFingerSwipeLeft.direction = UISwipeGestureRecognizerDirectionLeft;
  threeFingerSwipeLeft.cancelsTouchesInView = NO;
  threeFingerSwipeLeft.delegate = self;
  [self.view addGestureRecognizer:threeFingerSwipeLeft];

  // Three-finger swipe right → Redo
  UISwipeGestureRecognizer *threeFingerSwipeRight =
      [[UISwipeGestureRecognizer alloc] initWithTarget:self
                                                action:@selector(handleThreeFingerSwipeRight:)];
  threeFingerSwipeRight.numberOfTouchesRequired = 3;
  threeFingerSwipeRight.direction = UISwipeGestureRecognizerDirectionRight;
  threeFingerSwipeRight.cancelsTouchesInView = NO;
  threeFingerSwipeRight.delegate = self;
  [self.view addGestureRecognizer:threeFingerSwipeRight];

  // Left-edge swipe → Toggle panel sidebar
  UIScreenEdgePanGestureRecognizer *edgeSwipe =
      [[UIScreenEdgePanGestureRecognizer alloc] initWithTarget:self
                                                        action:@selector(handleEdgeSwipe:)];
  edgeSwipe.edges = UIRectEdgeLeft;
  edgeSwipe.delegate = self;
  [self.view addGestureRecognizer:edgeSwipe];

  pinch_velocity_ = 0.0f;

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

  // If the app was launched by opening a document (Files app), we may have been
  // handed a URL before the host/controller existed. Open it now that the host
  // is initialized.
  if (pending_open_url_) {
    [self queueOpenURL:pending_open_url_];
    pending_open_url_ = nil;
  }
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

// ----------------------------------------------------------------------------
// Open-In-Place (.yazeproj) document handling
// ----------------------------------------------------------------------------

- (NSURL *)yazeSettingsFileURL {
  NSArray<NSURL *> *urls =
      [[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
                                             inDomains:NSUserDomainMask];
  NSURL *documents = urls.firstObject;
  if (!documents) {
    return nil;
  }

  NSURL *root = [documents URLByAppendingPathComponent:@"Yaze" isDirectory:YES];
  [[NSFileManager defaultManager] createDirectoryAtURL:root
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:nil];
  return [root URLByAppendingPathComponent:@"settings.json"];
}

- (void)updateSettingsLastProjectPath:(NSString *)projectPath
                              romPath:(NSString *)romPath {
  NSURL *settingsURL = [self yazeSettingsFileURL];
  if (!settingsURL) {
    return;
  }

  NSFileManager *fm = [NSFileManager defaultManager];
  NSMutableDictionary *root = [NSMutableDictionary dictionary];
  if ([fm fileExistsAtPath:settingsURL.path]) {
    NSData *data = [NSData dataWithContentsOfURL:settingsURL];
    if (data.length > 0) {
      id obj = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
      if ([obj isKindOfClass:[NSDictionary class]]) {
        root = [obj mutableCopy];
      }
    }
  }

  NSMutableDictionary *general = nil;
  id existing_general = root[@"general"];
  if ([existing_general isKindOfClass:[NSDictionary class]]) {
    general = [existing_general mutableCopy];
  } else {
    general = [NSMutableDictionary dictionary];
  }

  if (projectPath.length > 0) {
    general[@"last_project_path"] = projectPath;
  }
  if (romPath.length > 0) {
    general[@"last_rom_path"] = romPath;
  }
  root[@"general"] = general;
  if (!root[@"version"]) {
    root[@"version"] = @(1);
  }

  NSJSONWritingOptions options = 0;
  if (@available(iOS 11.0, *)) {
    options = NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys;
  } else {
    options = NSJSONWritingPrettyPrinted;
  }

  NSData *out = [NSJSONSerialization dataWithJSONObject:root options:options error:nil];
  if (!out) {
    return;
  }
  [out writeToURL:settingsURL atomically:YES];

  // Ask the live settings store (Swift) to reload, so the overlay updates
  // immediately when the app is already running.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:@"yaze.settings.reload"
                    object:nil];
}

- (void)beginSecurityScopeForURL:(NSURL *)url {
  if (security_scoped_url_ && security_scope_granted_) {
    [security_scoped_url_ stopAccessingSecurityScopedResource];
  }
  security_scoped_url_ = url;
  security_scope_granted_ = NO;
  if (!url) {
    return;
  }
  security_scope_granted_ = [url startAccessingSecurityScopedResource];
}

// If the system hands us a URL inside a `.yazeproj` package (common with file
// provider/iCloud workflows), walk up to the bundle root so subsequent file
// access (rom, project snapshot) succeeds under a single security scope.
- (NSURL *)resolveYazeProjectBundleRootForURL:(NSURL *)url {
  if (!url) {
    return nil;
  }

  NSString *ext = [[url pathExtension] lowercaseString];
  if ([ext isEqualToString:@"yazeproj"]) {
    return url;
  }

  NSURL *current = url;
  for (int i = 0; i < 10; i++) {
    NSURL *parent = [current URLByDeletingLastPathComponent];
    if (!parent || [parent.path isEqualToString:current.path]) {
      break;
    }
    NSString *parent_ext = [[parent pathExtension] lowercaseString];
    if ([parent_ext isEqualToString:@"yazeproj"]) {
      return parent;
    }
    current = parent;
  }

  // Heuristic fallback: directory containing expected bundle markers.
  BOOL is_dir = NO;
  if ([[NSFileManager defaultManager] fileExistsAtPath:url.path
                                           isDirectory:&is_dir] &&
      is_dir) {
    NSURL *project_file = [url URLByAppendingPathComponent:@"project.yaze"];
    NSURL *rom_file = [url URLByAppendingPathComponent:@"rom"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:project_file.path] &&
        [[NSFileManager defaultManager] fileExistsAtPath:rom_file.path]) {
      return url;
    }
  }

  return url;
}

- (void)openURLNow:(NSURL *)url {
  if (!url) {
    return;
  }

  url = [self resolveYazeProjectBundleRootForURL:url] ?: url;

  // iCloud Drive items may be lazily downloaded. Request a download if needed.
  [[NSFileManager defaultManager] startDownloadingUbiquitousItemAtURL:url error:nil];
  if ([[[url pathExtension] lowercaseString] isEqualToString:@"yazeproj"]) {
    [[NSFileManager defaultManager]
        startDownloadingUbiquitousItemAtURL:[url URLByAppendingPathComponent:@"project.yaze"]
                                     error:nil];
    [[NSFileManager defaultManager]
        startDownloadingUbiquitousItemAtURL:[url URLByAppendingPathComponent:@"rom"]
                                     error:nil];
  }

  NSString *path = url.path;
  if (!path || path.length == 0) {
    return;
  }

  // Update settings for UX (overlay status + "restore last session").
  if ([[url.pathExtension lowercaseString] isEqualToString:@"yazeproj"]) {
    NSString *romPath = [[url URLByAppendingPathComponent:@"rom"] path];
    [self updateSettingsLastProjectPath:path romPath:romPath ?: @""];
  }

  if (!self.controller || !self.controller->editor_manager()) {
    return;
  }
  std::string cpp_path([path UTF8String]);
  (void)self.controller->editor_manager()->OpenRomOrProject(cpp_path);
}

- (void)queueOpenURL:(NSURL *)url {
  if (!url) {
    return;
  }

  NSURL *resolved = [self resolveYazeProjectBundleRootForURL:url] ?: url;

  // Keep security-scoped access alive for the duration of the opened project.
  // Prefer the bundle root so child reads succeed.
  [self beginSecurityScopeForURL:resolved];
  if (!security_scope_granted_ && resolved != url) {
    // Fallback: keep access to the picked URL if root access wasn't granted.
    [self beginSecurityScopeForURL:url];
    resolved = url;
  }

  if (!host_initialized_) {
    pending_open_url_ = resolved;
    return;
  }

  // Avoid scene-update watchdog termination: return quickly from the UIKit
  // callback, then perform project loading on the next run loop tick.
  dispatch_async(dispatch_get_main_queue(), ^{
    [self openURLNow:resolved];
  });
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
  const bool primary_was_stylus =
      primary_touch_ != nil && primary_touch_.type == UITouchTypeStylus;

  // Prefer Apple Pencil/stylus if present for precision input.
  for (UITouch *touch in event.allTouches) {
    if (touch.type == UITouchTypeStylus &&
        touch.phase != UITouchPhaseEnded &&
        touch.phase != UITouchPhaseCancelled) {
      active_touch = touch;
      break;
    }
  }

  // If no stylus, keep the primary touch if it's still active.
  if (!active_touch && primary_touch_ &&
      [event.allTouches containsObject:primary_touch_]) {
    if (primary_touch_.phase != UITouchPhaseEnded &&
        primary_touch_.phase != UITouchPhaseCancelled) {
      active_touch = primary_touch_;
    }
  }

  // If we were interacting with the stylus and it just lifted, do NOT adopt a
  // different in-flight touch (e.g. palm/finger resting on the screen). This
  // prevents ImGui drags (like moving windows) from snapping back when the
  // stylus ends.
  if (!active_touch && primary_was_stylus) {
    primary_touch_ = nil;
    io.AddMouseSourceEvent(ImGuiMouseSource_Pen);
    io.AddMouseButtonEvent(0, false);
    return;
  }

  // Otherwise, fall back to a new direct touch (only on Began).
  if (!active_touch) {
    for (UITouch *touch in event.allTouches) {
      if (touch.type == UITouchTypeDirect &&
          touch.phase == UITouchPhaseBegan) {
        active_touch = touch;
        break;
      }
    }
  }

  if (!active_touch) {
    for (UITouch *touch in event.allTouches) {
      if (touch.phase == UITouchPhaseBegan) {
        active_touch = touch;
        break;
      }
    }
  }

  if (active_touch) {
    UITouch *sample_touch = active_touch;
    if (@available(iOS 9.0, *)) {
      NSArray<UITouch *> *coalesced =
          [event coalescedTouchesForTouch:active_touch];
      if (coalesced.count > 0) {
        sample_touch = coalesced.lastObject;
      }
    }

    primary_touch_ = active_touch;
    ImGuiMouseSource source = ImGuiMouseSource_TouchScreen;
    if (active_touch.type == UITouchTypeStylus) {
      source = ImGuiMouseSource_Pen;
    }
    io.AddMouseSourceEvent(source);

    CGPoint touchLocation = CGPointZero;
    if (@available(iOS 9.0, *)) {
      if (active_touch.type == UITouchTypeStylus) {
        touchLocation = [sample_touch preciseLocationInView:self.view];
      } else {
        touchLocation = [sample_touch locationInView:self.view];
      }
    } else {
      touchLocation = [sample_touch locationInView:self.view];
    }

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

  // Update cursor position to pinch midpoint for zoom-toward-center behavior
  CGPoint location = [gesture locationInView:self.view];
  io.AddMousePosEvent(location.x, location.y);

  if (gesture.state == UIGestureRecognizerStateBegan) {
    previous_pinch_scale_ = gesture.scale;
    pinch_velocity_ = 0.0f;
    io.AddKeyEvent(ImGuiKey_ModCtrl, true);
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    io.AddKeyEvent(ImGuiKey_ModCtrl, true);
    float raw_delta = gesture.scale - previous_pinch_scale_;

    // Exponential moving average for smoother zoom
    constexpr float kSmoothingFactor = 0.3f;
    pinch_velocity_ = pinch_velocity_ * (1.0f - kSmoothingFactor) +
                      raw_delta * kSmoothingFactor;

    io.AddMouseWheelEvent(0.0f, pinch_velocity_);
    previous_pinch_scale_ = gesture.scale;
  } else if (gesture.state == UIGestureRecognizerStateEnded ||
             gesture.state == UIGestureRecognizerStateCancelled) {
    io.AddKeyEvent(ImGuiKey_ModCtrl, false);
    previous_pinch_scale_ = 1.0f;
    pinch_velocity_ = 0.0f;
  }
}

- (void)HandleTwoFingerPan:(UIPanGestureRecognizer *)gesture {
  ImGuiIO &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);

  CGPoint translation = [gesture translationInView:self.view];
  // Send as mouse wheel events so Canvas pan responds
  // Scale down to get reasonable scroll speed
  float dx = static_cast<float>(translation.x) * 0.05f;
  float dy = static_cast<float>(translation.y) * 0.05f;
  io.AddMouseWheelEvent(dx, dy);

  // Reset translation so we get deltas each callback
  [gesture setTranslation:CGPointZero inView:self.view];

  CGPoint location = [gesture locationInView:self.view];
  io.AddMousePosEvent(location.x, location.y);
}

- (void)handleLongPress:(UILongPressGestureRecognizer *)gesture {
  ImGuiIO &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  io.AddMouseButtonEvent(1, gesture.state == UIGestureRecognizerStateBegan);
  UIGestureRecognizer *gestureRecognizer = (UIGestureRecognizer *)gesture;
  io.AddMousePosEvent([gestureRecognizer locationInView:self.view].x,
                      [gestureRecognizer locationInView:self.view].y);
  if (gesture.state == UIGestureRecognizerStateBegan) {
    yaze::platform::ios::TriggerHaptic(yaze::platform::ios::HapticStyle::kMedium);
  }
}

- (void)handleThreeFingerSwipeLeft:(UISwipeGestureRecognizer *)gesture {
  // Three-finger swipe left = Undo (iOS convention)
  ImGuiIO &io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Super, true);
  io.AddKeyEvent(ImGuiKey_Z, true);
  io.AddKeyEvent(ImGuiKey_Z, false);
  io.AddKeyEvent(ImGuiMod_Super, false);
  yaze::platform::ios::TriggerHaptic(yaze::platform::ios::HapticStyle::kLight);
}

- (void)handleThreeFingerSwipeRight:(UISwipeGestureRecognizer *)gesture {
  // Three-finger swipe right = Redo (iOS convention)
  ImGuiIO &io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Super, true);
  io.AddKeyEvent(ImGuiMod_Shift, true);
  io.AddKeyEvent(ImGuiKey_Z, true);
  io.AddKeyEvent(ImGuiKey_Z, false);
  io.AddKeyEvent(ImGuiMod_Shift, false);
  io.AddKeyEvent(ImGuiMod_Super, false);
  yaze::platform::ios::TriggerHaptic(yaze::platform::ios::HapticStyle::kLight);
}

- (void)handleEdgeSwipe:(UIScreenEdgePanGestureRecognizer *)gesture {
  if (gesture.state == UIGestureRecognizerStateBegan) {
    yaze::platform::ios::PostToggleSidebar();
    yaze::platform::ios::TriggerHaptic(yaze::platform::ios::HapticStyle::kMedium);
  }
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
  AppViewController *rootViewController = [[AppViewController alloc] init];
  self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
  self.window.rootViewController = rootViewController;
  [self.window makeKeyAndVisible];

  // Handle "tap to open" from Files app at launch.
  if (@available(iOS 13.0, *)) {
    for (UIOpenURLContext *context in connectionOptions.URLContexts) {
      if (context.URL) {
        [rootViewController queueOpenURL:context.URL];
        break;
      }
    }
  }
}

- (void)scene:(UIScene *)scene openURLContexts:(NSSet<UIOpenURLContext *> *)URLContexts {
  (void)scene;
  AppViewController *root =
      (AppViewController *)self.window.rootViewController;
  if (![root isKindOfClass:[AppViewController class]]) {
    return;
  }
  for (UIOpenURLContext *context in URLContexts) {
    if (context.URL) {
      [root queueOpenURL:context.URL];
      break;
    }
  }
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

- (BOOL)application:(UIApplication *)application
            openURL:(NSURL *)url
            options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options {
  (void)application;
  (void)options;

  if (!url) {
    return NO;
  }

  UIViewController *rootViewController = [self RootViewControllerForPresenting];
  if ([rootViewController isKindOfClass:[AppViewController class]]) {
    [(AppViewController *)rootViewController queueOpenURL:url];
    return YES;
  }
  return NO;
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
