#ifndef YAZE_APP_PLATFORM_APP_DELEGATE_H_
#define YAZE_APP_PLATFORM_APP_DELEGATE_H_

#include "app/application.h"

#if defined(__APPLE__) && defined(__MACH__)

extern "C" {
// Initialize Cocoa Application Delegate
void yaze_initialize_cocoa();

// Run the main loop with Cocoa App Delegate
int yaze_run_cocoa_app_delegate(const yaze::AppConfig& config);
}

#endif

#endif  // YAZE_APP_PLATFORM_APP_DELEGATE_H_
