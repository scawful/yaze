#ifndef YAZE_APP_EDITOR_SYSTEM_SESSION_COLLISION_SOURCE_PAIRING_H_
#define YAZE_APP_EDITOR_SYSTEM_SESSION_COLLISION_SOURCE_PAIRING_H_

#include <filesystem>

#include "absl/status/status.h"
#include "rom/rom.h"

namespace yaze::editor {

// A project ROM rebind must keep its existing source contract intact. This
// read-only preflight requires the replacement ROM to reproduce the configured
// source data exactly, ignoring JSON whitespace and object-key order. It never
// publishes a new source or changes either ROM.
absl::Status ValidateRomCollisionSourceBinding(
    Rom* replacement_rom, const std::filesystem::path& json_path);

// Saves `rom` to its own filename and keeps a tracked custom_collision.json in
// step with it. Projects that name `custom_collision_json` (Oracle of Secrets)
// edit collision in the ROM, and their build requires matching collision data.
//
// - The JSON is regenerated from the in-memory ROM on every save. When its
//   data is unchanged, only the ROM is written; JSON bytes and formatting stay
//   untouched. Malformed or ambiguous JSON is refused.
// - Otherwise the ROM and the JSON are published as one rollback-protected
//   set: if either write or its readback fails, both files keep their
//   previous bytes. A crash between the two replacements is not covered.
// - The save is refused, writing nothing, when the JSON on disk matches
//   neither the ROM on disk nor the new ROM: it was changed outside yaze.
absl::Status SaveRomWithCollisionSource(Rom* rom,
                                        const std::filesystem::path& json_path);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_SESSION_COLLISION_SOURCE_PAIRING_H_
