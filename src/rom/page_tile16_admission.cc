#include "rom/page_tile16_admission.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "absl/strings/strip.h"
#include "rom/lyra_layout_descriptor.h"
#include "util/rom_hash.h"

namespace yaze {
namespace rom {
namespace {

bool PathsReferToSameBackingFile(const std::string& lhs,
                                 const std::string& rhs) {
  if (lhs.empty() || rhs.empty()) {
    return false;
  }
#ifndef __EMSCRIPTEN__
  std::error_code equivalent_ec;
  if (std::filesystem::equivalent(lhs, rhs, equivalent_ec) && !equivalent_ec) {
    return true;
  }
#endif
  std::error_code lhs_ec;
  std::error_code rhs_ec;
  const auto normalized_lhs =
      std::filesystem::weakly_canonical(lhs, lhs_ec).lexically_normal();
  const auto normalized_rhs =
      std::filesystem::weakly_canonical(rhs, rhs_ec).lexically_normal();
  return !lhs_ec && !rhs_ec && normalized_lhs == normalized_rhs;
}

}  // namespace

bool AdmitProjectExperimentalPageTile16(const project::YazeProject& project,
                                        Rom* rom) {
  if (!project.project_opened() || !project.experimental_page_tile16 ||
      rom == nullptr || !rom->is_loaded() || project.rom_filename.empty()) {
    return false;
  }
  const auto layout = InspectLyraLayout(rom->data(), rom->size());
  if (layout.presence != LyraLayoutPresence::kValid || layout.revision != 5) {
    return false;
  }
  if (!PathsReferToSameBackingFile(
          rom->filename(), project.GetAbsolutePath(project.rom_filename))) {
    return false;
  }
  const std::string expected(
      absl::StripAsciiWhitespace(project.rom_metadata.expected_hash));
  if (expected.empty() ||
      util::ComputeRomIdentityHashes(rom->data(), rom->size())
              .Match(expected) == util::RomHashMatch::kMismatch) {
    return false;
  }
  rom->EnableExperimentalPageTile16();
  return true;
}

bool EnvRequestsExperimentalPageTile16() {
  const char* value = std::getenv(kExperimentalPageTile16EnvVar);
  return value != nullptr && std::strcmp(value, "1") == 0;
}

}  // namespace rom
}  // namespace yaze
