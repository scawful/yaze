// This is a precompiled header for the yaze project.
// It includes a set of common, relatively stable headers that are used across
// multiple source files to speed up compilation.
//
// Note: We only include standard library headers here to avoid circular
// dependencies. Project headers like util/log.h require Abseil, which is
// built later in the dependency chain.

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

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif

// Note: Project-specific headers are NOT included here to avoid dependency
// issues. Each source file should include what it needs directly.

#endif  // YAZE_PCH_H
