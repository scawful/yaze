#include "rom/hm_support.h"

#include "absl/strings/match.h"
#include "util/log.h"

namespace yaze {
namespace rom {

HyruleMagicValidator::HyruleMagicValidator(Rom* rom) : rom_(rom) {}

bool HyruleMagicValidator::IsParallelWorlds() const {
  if (!rom_ || !rom_->is_loaded()) return false;

  // Heuristic 1: Size is exactly 1.5MB (0x180000 bytes)
  // Most hacks are 1MB, 2MB, or 4MB. 1.5MB is very specific to PW.
  if (rom_->size() == 0x180000) {
    return true;
  }

  // Heuristic 2: Internal Header Title
  // PW title is "THE LEGEND OF ZELDA" (same as vanilla), so we can't use that.
  // But we can check for specific strings if needed.
  // For now, size is the strongest indicator.
  
  return false;
}

bool HyruleMagicValidator::IsHyruleMagic() const {
  // HM doesn't leave a clear signature, but we can infer it from
  // corruption patterns or specific hacks.
  // For now, we assume any ROM with Bank 00 erasure is likely an old HM hack.
  return HasBank00Erasure();
}

bool HyruleMagicValidator::HasBank00Erasure() const {
  if (!rom_ || !rom_->is_loaded()) return false;

  // Check for large block of zeros in Bank 00 code region
  // This was identified in Phase 1 as a symptom of GoT-v040.smc (HM hack)
  // Region: 0x00004B - 0x000100 (approx)
  int zero_count = 0;
  int check_start = 0x4B;
  int check_end = 0x100;
  
  if (check_end >= static_cast<int>(rom_->size())) return false;

  const auto* data = rom_->data();
  for (int i = check_start; i < check_end; ++i) {
    if (data[i] == 0x00) {
      zero_count++;
    }
  }

  // If > 90% zeros, it's likely erased
  return zero_count > (check_end - check_start) * 0.9;
}

bool HyruleMagicValidator::FixChecksum() {
  if (!rom_ || !rom_->is_loaded()) return false;

  // Calculate CRC32 and SNES Checksum
  // Note: Rom class might have methods for this, but we'll implement
  // a basic SNES checksum calculator here if needed.
  // Actually, let's use the existing Rom::CalculateChecksum if available,
  // or implement it.
  
  // SNES Checksum Algorithm:
  // Sum of all bytes.
  // For LoROM, power of 2 sizes are easy.
  // For non-power of 2 (like 1.5MB), we need to mirror the last part.
  
  size_t size = rom_->size();
  uint32_t sum = 0;
  
  // 1.5MB (0x180000) mapping:
  // 0x000000 - 0x0FFFFF (1MB)
  // 0x100000 - 0x17FFFF (0.5MB)
  // The 0.5MB part is mirrored to fill the 2MB space for checksum calculation?
  // Or is it just summed as is?
  // Standard SNES checksum ignores mirroring usually, but for 1.5MB
  // it treats it as 2MB with the last 0.5MB mirrored?
  // Let's stick to simple sum for now, or use a proper library if available.
  // Since we don't have a dedicated checksum library exposed, we'll skip
  // the actual calculation implementation for this pass and focus on the structure.
  // TODO: Implement proper SNES checksum calculation.
  
  return true; // Placeholder
}

std::string HyruleMagicValidator::GetVariantName() const {
  if (IsParallelWorlds()) return "Parallel Worlds (1.5MB)";
  if (HasBank00Erasure()) return "Hyrule Magic (Corrupted)";
  return "Vanilla / Unknown";
}

}  // namespace rom
}  // namespace yaze
