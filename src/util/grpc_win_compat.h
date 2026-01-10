#ifndef YAZE_UTIL_GRPC_WIN_COMPAT_H_
#define YAZE_UTIL_GRPC_WIN_COMPAT_H_

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifdef SendMessage
#undef SendMessage
#endif
#ifdef ERROR
#undef ERROR
#endif
#ifdef IGNORE
#undef IGNORE
#endif
#ifdef OVERFLOW
#undef OVERFLOW
#endif
#ifdef DWORD
#undef DWORD
#endif
#endif  // _WIN32

#endif  // YAZE_UTIL_GRPC_WIN_COMPAT_H_
