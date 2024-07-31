#ifndef YAZE_APP_CORE_PLATFORM_APP_DELEGATE_H
#define YAZE_APP_CORE_PLATFORM_APP_DELEGATE_H

#ifdef TARGET_OS_MAC

#ifdef __cplusplus
extern "C" {
#endif

void InitializeCocoa();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // TARGET_OS_MAC

#endif  // YAZE_APP_CORE_PLATFORM_APP_DELEGATE_H
