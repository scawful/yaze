#ifndef YAZE_TEST_FRAMEWORK_ROM_SAVE_FAULT_H_
#define YAZE_TEST_FRAMEWORK_ROM_SAVE_FAULT_H_

#include "rom/rom.h"

namespace yaze::test {

// Makes Rom::SaveToFile fail after any backup is written and before the
// destination is replaced, as a full disk or a revoked permission would.
// SaveToFile stages through a uniquely named file, so a pre-created
// `<rom>.tmp` no longer blocks it.
class ScopedRomStagingFailure {
 public:
  ScopedRomStagingFailure() { Rom::SetStagingFailureForTesting(true); }
  ~ScopedRomStagingFailure() { Rom::SetStagingFailureForTesting(false); }
  ScopedRomStagingFailure(const ScopedRomStagingFailure&) = delete;
  ScopedRomStagingFailure& operator=(const ScopedRomStagingFailure&) = delete;
};

}  // namespace yaze::test

#endif  // YAZE_TEST_FRAMEWORK_ROM_SAVE_FAULT_H_
