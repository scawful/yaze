#ifndef YAZE_APP_EDITOR_SYSTEM_HACK_MANIFEST_SAVE_VALIDATION_H
#define YAZE_APP_EDITOR_SYSTEM_HACK_MANIFEST_SAVE_VALIDATION_H

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "core/hack_manifest.h"
#include "core/project.h"

namespace yaze::editor {

absl::Status ValidateHackManifestSaveConflicts(
    const core::HackManifest& manifest, project::RomWritePolicy write_policy,
    const std::vector<std::pair<uint32_t, uint32_t>>& ranges,
    absl::string_view save_scope, const char* log_tag,
    ToastManager* toast_manager);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_HACK_MANIFEST_SAVE_VALIDATION_H
