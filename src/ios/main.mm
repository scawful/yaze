// yaze iOS Application
// Uses SDL2 and ImGui

#import <Foundation/Foundation.h>

#if TARGET_OS_OSX
#import <Cocoa/Cocoa.h>
#else
#import <UIKit/UIKit.h>
#endif

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import <MobileCoreServices/MobileCoreServices.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "app/core/controller.h"
#include "app/core/platform/app_delegate.h"

#include <SDL.h>

#ifdef main
#undef main
#endif

#include "app/core/platform/view_controller.h"
#include "imgui/backends/imgui_impl_metal.h"
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

  _controller = new yaze::app::core::Controller();

  SDL_SetMainReady();
  SDL_iOSSetEventPump(SDL_TRUE);
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
  SDL_iOSSetEventPump(SDL_FALSE);

  // Enable native IME.
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
  if (!_controller->CreateWindow().ok()) {
    printf("Error creating window: %s\n", SDL_GetError());
    abort();
  }
  if (!_controller->CreateRenderer().ok()) {
    printf("Error creating renderer: %s\n", SDL_GetError());
    abort();
  }

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

  yaze::app::gui::ColorsYaze();

  ImGui_ImplSDL2_InitForSDLRenderer(_controller->window(), _controller->renderer());
  ImGui_ImplSDLRenderer2_Init(_controller->renderer());

  if (!_controller->LoadFontFamilies().ok()) {
    abort();
  }
  _controller->SetupScreen(rom_filename);
  _controller->set_active(true);

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

#if TARGET_OS_OSX
  ImGui_ImplOSX_Init(self.view);
  [NSApp activateIgnoringOtherApps:YES];
#endif
}

- (void)drawInMTKView:(MTKView *)view {
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize.x = view.bounds.size.width;
  io.DisplaySize.y = view.bounds.size.height;

#if TARGET_OS_OSX
  CGFloat framebufferScale =
      view.window.screen.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
#else
  CGFloat framebufferScale = view.window.screen.scale ?: UIScreen.mainScreen.scale;
#endif
  io.DisplayFramebufferScale = ImVec2(framebufferScale, framebufferScale);

  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
#if TARGET_OS_OSX
  ImGui_ImplOSX_NewFrame(view);
#endif

  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  ImGui::SetNextWindowSize(dimensions, ImGuiCond_Always);
  if (ImGui::Begin("##YazeMain", nullptr,
                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
                       ImGuiWindowFlags_NoBringToFrontOnFocus)) {
    auto controller_status = _controller->OnLoad();
    if (!controller_status.ok()) {
      abort();
    }
    ImGui::End();
  }
  _controller->DoRender();
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
}

// ----------------------------------------------------------------------------
// Input processing
// ----------------------------------------------------------------------------

#if TARGET_OS_OSX

- (void)viewWillAppear {
  [super viewWillAppear];
  self.view.window.delegate = self;
}

- (void)windowWillClose:(NSNotification *)notification {
  ImGui_ImplMetal_Shutdown();
  ImGui_ImplOSX_Shutdown();
  ImGui::DestroyContext();
}

#else

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
}

- (void)HandleSwipe:(UISwipeGestureRecognizer *)gesture {
  ImGuiIO &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  if (gesture.direction == UISwipeGestureRecognizerDirectionRight) {
    io.AddMouseWheelEvent(1.0f, 0.0f);  // Swipe Right
  } else if (gesture.direction == UISwipeGestureRecognizerDirectionLeft) {
    io.AddMouseWheelEvent(-1.0f, 0.0f);  // Swipe Left
  }
}

- (void)handleLongPress:(UILongPressGestureRecognizer *)gesture {
  ImGuiIO &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  io.AddMouseButtonEvent(1, gesture.state == UIGestureRecognizerStateBegan);
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
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_Quit();
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
      // Cast NSData* to uint8_t*
      uint8_t *bytes = (uint8_t *)[data bytes];
      // Size of the data
      size_t size = [data length];

      std::vector<uint8_t> rom_data;
      rom_data.resize(size);
      std::copy(bytes, bytes + size, rom_data.begin());

      PRINT_IF_ERROR(yaze::SharedRom::shared_rom_->LoadFromBytes(rom_data));
      std::string filename = std::string([selectedFileURL.path UTF8String]);
      yaze::SharedRom::shared_rom_->set_filename(filename);
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
