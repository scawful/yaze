// yaze iOS Application
// Uses SDL2, ImGui and Metal 

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

//#define SDL_main main

#include "app/core/platform/app_view_controller.h"
#include "imgui/backends/imgui_impl_metal.h"
#include "imgui/imgui.h"

//-----------------------------------------------------------------------------------
// AppViewController
//-----------------------------------------------------------------------------------

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

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

  yaze::app::gui::ColorsYaze();

  SDL_SetMainReady();
  SDL_iOSSetEventPump(SDL_TRUE);

  // TODO: Process arguments.
  //    auto argc = NSProcessInfo.processInfo.arguments.count;
  //    char** argv = NSProcessInfo.processInfo.arguments.firstObject.string;
  //    SDL_main(argc, argv);

  SDL_iOSSetEventPump(SDL_FALSE);

  // Enable native IME.
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

  //  SDL_Window* window = SDL_CreateWindow("Yet Another Zelda3 Editor", SDL_WINDOWPOS_CENTERED,
  //  SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  //  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED |
  //  SDL_RENDERER_PRESENTVSYNC);

  if (!_controller->CreateSDL_Window().ok()) {
    printf("Error creating window: %s\n", SDL_GetError());
    abort();
  }
  if (!_controller->CreateRenderer().ok()) {
    printf("Error creating renderer: %s\n", SDL_GetError());
    abort();
  }

  ImGui_ImplSDL2_InitForSDLRenderer(_controller->window(), _controller->renderer());
  ImGui_ImplSDLRenderer2_Init(_controller->renderer());

  _controller->editor_manager().overworld_editor().InitializeZeml();
  if (!_controller->LoadFontFamilies().ok()) {
    abort();
  }
  _controller->SetupScreen();
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

//-----------------------------------------------------------------------------------
// Input processing
//-----------------------------------------------------------------------------------

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
- (void)updateIOWithTouchEvent:(UIEvent *)event {
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
  
  UIHoverGestureRecognizer *hoverGestureRecognizer = [[UIHoverGestureRecognizer alloc] initWithTarget:self action:@selector(hoverGesture:)];
  [self.view addGestureRecognizer:hoverGestureRecognizer];
- (void)hoverGesture:(UIHoverGestureRecognizer *)gesture {
  ImGuiIO &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  // Cast to UIGestureRecognizer to UIGestureRecognizer to get locationInView
  UIGestureRecognizer *gestureRecognizer = (UIGestureRecognizer *)gesture;
  if (gesture.zOffset < 0.50) {
    io.AddMousePosEvent([gestureRecognizer locationInView:self.view].x, [gestureRecognizer locationInView:self.view].y);
  }
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
  [self updateIOWithTouchEvent:event];
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
  [self updateIOWithTouchEvent:event];
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
  [self updateIOWithTouchEvent:event];
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
  [self updateIOWithTouchEvent:event];
}

#endif

@end

//-----------------------------------------------------------------------------------
// AppDelegate
//-----------------------------------------------------------------------------------

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

- (void)presentDocumentPickerWithCompletionHandler:
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
      uchar *bytes = (uchar *)[data bytes];
      // Size of the data
      size_t size = [data length];
      
      PRINT_IF_ERROR(yaze::app::SharedRom::shared_rom_->LoadFromPointer(bytes, size));
      std::string filename = std::string([selectedFileURL.path UTF8String]);
      yaze::app::SharedRom::shared_rom_->set_filename(filename);
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

//-----------------------------------------------------------------------------------
// Application main() function
//-----------------------------------------------------------------------------------

#if TARGET_OS_OSX

int main(int argc, const char *argv[]) { return NSApplicationMain(argc, argv); }

#else

// int SDL_main(int argc, char*argv[]) {
//   yaze::app::core::Controller controller;
//   controller.OnEntry();
//   while (controller.IsActive()) {
//        controller.OnInput();
//        if (auto status = controller.OnLoad(); !status.ok()) {
//          std::cerr << status.message() << std::endl;
//          break;
//        }
//        controller.DoRender();
//      }
//      controller.OnExit();
//
//    return EXIT_SUCCESS;
//
//
//   return 0;
// }
// int main(int argc, char *argv[])
//{
//     return SDL_UIKitRunApp(argc, argv, SDL_main);
// }

int main(int argc, char *argv[]) {
  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
  }
}

#endif
