#ifndef YAZE_APP_CORE_PLATFORM_APP_DELEGATE_H
#define YAZE_APP_CORE_PLATFORM_APP_DELEGATE_H

#if defined(__APPLE__) && defined(__MACH__)
/* Apple OSX and iOS (Darwin). */
#import <CoreText/CoreText.h>
#include <TargetConditionals.h>

#if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
/* iOS in Xcode simulator */
#import <PencilKit/PencilKit.h>
#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate,
                                      UIDocumentPickerDelegate,
                                      UITabBarControllerDelegate,
                                      PKCanvasViewDelegate>
@property(strong, nonatomic) UIWindow *window;

@property UIDocumentPickerViewController *documentPicker;
@property(nonatomic, copy) void (^completionHandler)(NSString *selectedFile);
- (void)PresentDocumentPickerWithCompletionHandler:
    (void (^)(NSString *selectedFile))completionHandler;

// TODO: Setup a tab bar controller for multiple yaze instances
@property(nonatomic) UITabBarController *tabBarController;

// TODO: Setup a font picker for the text editor and display settings
@property(nonatomic) UIFontPickerViewController *fontPicker;

// TODO: Setup the pencil kit for drawing
@property PKToolPicker *toolPicker;
@property PKCanvasView *canvasView;

// TODO: Setup the file manager for file operations
@property NSFileManager *fileManager;

@end

#elif TARGET_OS_MAC == 1

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Cocoa application.
 */
void yaze_initialize_cocoa();

/**
 * @brief Run the Cocoa application delegate.
 */
void yaze_run_cocoa_app_delegate(const char *filename);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // TARGET_OS_MAC

#endif  // defined(__APPLE__) && defined(__MACH__)

#endif  // YAZE_APP_CORE_PLATFORM_APP_DELEGATE_H
