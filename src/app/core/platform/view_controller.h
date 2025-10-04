#ifndef YAZE_APP_CORE_PLATFORM_VIEW_CONTROLLER_H
#define YAZE_APP_CORE_PLATFORM_VIEW_CONTROLLER_H

#ifdef __APPLE__
#include <TargetConditionals.h>

#if TARGET_OS_OSX
#ifdef __OBJC__
@interface AppViewController : NSViewController <NSWindowDelegate>
@property(nonatomic) yaze::core::Controller *controller;
@end
#endif
#else
#ifdef __OBJC__
@interface AppViewController : UIViewController <MTKViewDelegate>
@property(nonatomic) yaze::core::Controller *controller;
@property(nonatomic) UIHoverGestureRecognizer *hoverGestureRecognizer;
@property(nonatomic) UIPinchGestureRecognizer *pinchRecognizer;
@property(nonatomic) UISwipeGestureRecognizer *swipeRecognizer;
@property(nonatomic) UILongPressGestureRecognizer *longPressRecognizer;
@end
#endif
#endif

#ifdef __OBJC__
@interface AppViewController () <MTKViewDelegate>
@property(nonatomic, readonly) MTKView *mtkView;
@property(nonatomic, strong) id<MTLDevice> device;
@property(nonatomic, strong) id<MTLCommandQueue> commandQueue;
@end
#endif

#endif  // __APPLE__

#endif  // YAZE_APP_CORE_PLATFORM_VIEW_CONTROLLER_H
