// This is a precompiled header for the yaze project.
// It includes a set of common, relatively stable headers that are used across
// multiple source files to speed up compilation.
//
// Note: Project headers are not included here to avoid dependency issues.
// Each source file should include what it needs directly; the PCH only saves
// parse time and must not be relied on to provide declarations.

#ifndef YAZE_PCH_H
#define YAZE_PCH_H

// Standard Library
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// Most yaze sources include these; absl/status/status.h alone (via absl::Cord)
// is a large share of a typical translation unit's parse time. Guarded so a
// target without Abseil on its include path still builds.
#if __has_include("absl/status/status.h")
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#endif

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif

#endif  // YAZE_PCH_H
