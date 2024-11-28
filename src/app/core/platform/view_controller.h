#ifndef YAZE_APP_CORE_PLATFORM_VIEW_CONTROLLER_H
#define YAZE_APP_CORE_PLATFORM_VIEW_CONTROLLER_H

#ifdef __APPLE__
#include <TargetConditionals.h>

#if TARGET_OS_OSX
#include "imgui_impl_osx.h"
@interface AppViewController : NSViewController <NSWindowDelegate>
@end
#else
@interface AppViewController : UIViewController <MTKViewDelegate>
@property(nonatomic) yaze::app::core::Controller *controller;
@property(nonatomic) UIHoverGestureRecognizer *hoverGestureRecognizer;
@property(nonatomic) UIPinchGestureRecognizer *pinchRecognizer;
@property(nonatomic) UISwipeGestureRecognizer *swipeRecognizer;
@property(nonatomic) UILongPressGestureRecognizer *longPressRecognizer;
@end
#endif

@interface AppViewController () <MTKViewDelegate>
@property(nonatomic, readonly) MTKView *mtkView;
@property(nonatomic, strong) id<MTLDevice> device;
@property(nonatomic, strong) id<MTLCommandQueue> commandQueue;
@end

#endif  // __APPLE__

#endif  // YAZE_APP_CORE_PLATFORM_APP_VIEW_CONTROLLER_H
