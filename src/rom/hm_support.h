#ifndef YAZE_ROM_HM_SUPPORT_H
#define YAZE_ROM_HM_SUPPORT_H

#include <string>
#include <vector>

#include "rom/rom.h"

namespace yaze {
namespace rom {

class HyruleMagicValidator {
 public:
  explicit HyruleMagicValidator(Rom* rom);

  // Detection
  bool IsParallelWorlds() const;
  bool IsHyruleMagic() const;
  bool HasBank00Erasure() const;

  // Fixes
  bool FixChecksum();
  
  // Reporting
  std::string GetVariantName() const;

 private:
  Rom* rom_;
};

}  // namespace rom
}  // namespace yaze

#endif  // YAZE_ROM_HM_SUPPORT_H
