#ifndef YAZE_CORE_GFX_SHEET_SOURCE_H_
#define YAZE_CORE_GFX_SHEET_SOURCE_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "core/source_artifact_publisher.h"

namespace yaze::project {
struct YazeProject;
}
namespace yaze::core {

// Current authoring source, independent of a compiled ROM's pointer provenance.
// The owning session's GraphicsSheetStore holds draft pixels and Undo history.
class GfxSheetSource {
 public:
  struct Sheet {
    std::string path;
    std::string bytes;
    std::vector<uint8_t> pixels;
    // The !ENABLE_* define that selected this entry; empty when unflagged.
    std::string flag;
  };
  struct Plan {
    std::string descriptor;
    std::string spec;
    std::string sha1;
    std::vector<SourceArtifactUpdate> updates;
    std::map<uint16_t, Sheet> sheets;
  };
  static bool Selected(const project::YazeProject& project);
  static absl::StatusOr<GfxSheetSource> Load(
      const project::YazeProject& project);
  static absl::StatusOr<std::vector<uint8_t>> DecodeText(
      const std::string& text);
  static std::string EncodeText(const std::vector<uint8_t>& pixels);
  static std::string SpecSha1(const std::string& bytes);
  static std::string DecodedSha256(const std::vector<uint8_t>& pixels);

  const std::map<uint16_t, Sheet>& sheets() const { return sheets_; }
  const std::map<std::string, std::string>& inputs() const { return inputs_; }
  const std::string& spec_path() const { return spec_path_; }
  const std::string& spec_sha1() const { return spec_sha1_; }
  absl::Status CheckBinding(const project::YazeProject& project) const;
  absl::Status CheckDisk() const;
  absl::StatusOr<Plan> Prepare(
      const project::YazeProject& project,
      const std::map<uint16_t, std::vector<uint8_t>>& pixels) const;
  void Commit(Plan plan, std::string descriptor);

 private:
  std::map<std::string, std::string> inputs_;
  // Every define file read to select flagged entries, nullopt when absent at
  // load; checked, never published.
  std::map<std::string, std::optional<std::string>> defines_;
  std::string descriptor_;
  std::string root_;
  std::string spec_path_;
  std::string spec_;
  std::string spec_sha1_;
  std::map<uint16_t, Sheet> sheets_;
};
}  // namespace yaze::core
#endif
