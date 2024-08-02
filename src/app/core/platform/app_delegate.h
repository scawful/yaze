#ifndef YAZE_APP_CORE_PLATFORM_APP_DELEGATE_H
#define YAZE_APP_CORE_PLATFORM_APP_DELEGATE_H

#if defined(__APPLE__) && defined(__MACH__)
/* Apple OSX and iOS (Darwin). */
#import <CoreText/CoreText.h>
#include <TargetConditionals.h>

#if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
/* iOS in Xcode simulator */
#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate, UIDocumentPickerDelegate>
@property (strong, nonatomic) UIWindow *window;

@property  UIDocumentPickerViewController *documentPicker;
@property (nonatomic, copy) void (^completionHandler)(NSString *selectedFile);

- (void)presentDocumentPickerWithCompletionHandler:(void (^)(NSString *selectedFile))completionHandler;

@end

#elif TARGET_OS_MAC == 1

#ifdef __cplusplus
extern "C" {
#endif

void InitializeCocoa();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // TARGET_OS_MAC

#endif  // defined(__APPLE__) && defined(__MACH__)

#endif  // YAZE_APP_CORE_PLATFORM_APP_DELEGATE_H
