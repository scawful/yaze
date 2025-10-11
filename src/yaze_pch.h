// This is a precompiled header for the yaze project.
// It includes a set of common, relatively stable headers that are used across
// multiple source files to speed up compilation.

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

// Third-party libraries
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

// Project-specific headers
#include "util/log.h"


#endif // YAZE_PCH_H
