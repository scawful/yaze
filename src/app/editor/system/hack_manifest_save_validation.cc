#include "app/editor/system/hack_manifest_save_validation.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "util/log.h"

namespace yaze::editor {

absl::Status ValidateHackManifestSaveConflicts(
    const core::HackManifest& manifest, project::RomWritePolicy write_policy,
    const std::vector<std::pair<uint32_t, uint32_t>>& ranges,
    absl::string_view save_scope, const char* log_tag,
    ToastManager* toast_manager) {
  auto conflicts = manifest.AnalyzePcWriteRanges(ranges);
  if (conflicts.empty()) {
    return absl::OkStatus();
  }

  std::string error_msg = absl::StrFormat(
      "Hack manifest write conflicts while saving %s:\n\n", save_scope);
  for (const auto& conflict : conflicts) {
    absl::StrAppend(
        &error_msg,
        absl::StrFormat("- Address 0x%06X is %s", conflict.address,
                        core::AddressOwnershipToString(conflict.ownership)));
    if (!conflict.module.empty()) {
      absl::StrAppend(&error_msg, " (Module: ", conflict.module, ")");
    }
    absl::StrAppend(&error_msg, "\n");
  }

  if (write_policy == project::RomWritePolicy::kAllow) {
    LOG_DEBUG(log_tag, "%s", error_msg.c_str());
  } else {
    LOG_WARN(log_tag, "%s", error_msg.c_str());
  }

  if (toast_manager && write_policy == project::RomWritePolicy::kWarn) {
    toast_manager->Show(
        "Save warning: write conflict with hack manifest (see log)",
        ToastType::kWarning);
  }

  if (write_policy == project::RomWritePolicy::kBlock) {
    if (toast_manager) {
      toast_manager->Show(
          "Save blocked: write conflict with hack manifest (see log)",
          ToastType::kError);
    }
    return absl::PermissionDeniedError("Write conflict with Hack Manifest");
  }

  return absl::OkStatus();
}

}  // namespace yaze::editor
