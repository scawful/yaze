#ifndef YAZE_CLI_HANDLERS_GAME_DUNGEON_RENDER_OBJECT_PLAN_H_
#define YAZE_CLI_HANDLERS_GAME_DUNGEON_RENDER_OBJECT_PLAN_H_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

namespace yaze {
class Rom;
namespace core {
class HackManifest;
}
namespace cli::handlers {

struct DungeonRenderObjectPreview {
  std::unique_ptr<Rom> rom;
  std::string source_stream_sha256;
  std::vector<std::pair<uint32_t, uint32_t>> write_ranges;
  int objects_before = 0;
  int objects_after = 0;
  int edit_count = 0;
};

// The bounded JSON input used by both render preview and persisted apply.
absl::StatusOr<nlohmann::json> LoadDungeonObjectPlanFile(
    const std::filesystem::path& path);

// Uses the normal room model, object-save plan and decoder on detached RAM.
// The source ROM, its filename/dirty state and all files remain untouched.
absl::StatusOr<DungeonRenderObjectPreview> StageDungeonRenderObjectPlan(
    const Rom& source, int room_id, const nlohmann::json& plan,
    const core::HackManifest* manifest = nullptr);

}  // namespace cli::handlers
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_GAME_DUNGEON_RENDER_OBJECT_PLAN_H_
