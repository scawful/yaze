#ifndef YAZE_ZELDA3_DUNGEON_DUNGEON_WALL_PROFILE_H_
#define YAZE_ZELDA3_DUNGEON_DUNGEON_WALL_PROFILE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "zelda3/dungeon/dungeon_tile_patterns.h"

namespace yaze::zelda3 {

inline constexpr size_t kWallProfileMaxSourceBytes = 1024 * 1024;
inline constexpr size_t kWallProfileMaxProfiles = 128;
inline constexpr size_t kWallProfileMaxPatterns = 128;
inline constexpr size_t kWallProfileMaxWordsPerProfile = 4096;
inline constexpr size_t kWallProfileMaxBindings = 4096;

struct DungeonWallPattern {
  std::string name;
  uint32_t pc_offset;
  std::vector<uint16_t> words;
};

struct DungeonWallProfile {
  std::string id;
  std::vector<DungeonWallPattern> patterns;
};

struct DungeonWallProfileBinding {
  uint16_t room;
  std::string profile;
  std::optional<uint8_t> world;
  std::optional<uint16_t> entrance;
};

// Missing optional context never satisfies an entrance/world constraint.
struct WallProfileContext {
  int room = -1;
  std::optional<int> world;
  std::optional<int> entrance;
};

// A validated immutable source snapshot. This model owns no global state and
// performs no publication, ROM mutation, editor history or rendering.
class DungeonWallProfileCollection {
 public:
  const std::vector<DungeonWallProfile>& profiles() const { return profiles_; }
  const std::vector<DungeonWallProfileBinding>& bindings() const {
    return bindings_;
  }
  const std::string& source_text() const { return source_text_; }
  const std::string& source_path() const { return source_path_; }

 private:
  friend absl::StatusOr<std::shared_ptr<const DungeonWallProfileCollection>>
  ParseDungeonWallProfiles(absl::string_view source);
  friend absl::StatusOr<std::shared_ptr<const DungeonWallProfileCollection>>
  LoadDungeonWallProfiles(const std::string& path);

  DungeonWallProfileCollection(std::vector<DungeonWallProfile> profiles,
                               std::vector<DungeonWallProfileBinding> bindings,
                               std::string source_text,
                               std::string source_path);

  std::vector<DungeonWallProfile> profiles_;
  std::vector<DungeonWallProfileBinding> bindings_;
  std::string source_text_;
  std::string source_path_;
};

// The merged words are read-only. A selected profile has no legacy publication
// provenance: its words must never be published into the legacy ASM source.
class ResolvedWallProfile {
 public:
  const std::optional<std::string>& profile_id() const { return profile_id_; }
  const DungeonTilePatterns& patterns() const { return patterns_; }

 private:
  friend absl::StatusOr<ResolvedWallProfile> ResolveWallProfile(
      const DungeonWallProfileCollection& collection,
      const WallProfileContext& context, const DungeonTilePatterns& legacy);

  ResolvedWallProfile(std::optional<std::string> profile_id,
                      DungeonTilePatterns patterns);

  std::optional<std::string> profile_id_;
  DungeonTilePatterns patterns_;
};

// Strict JSON v1. Unknown/duplicate keys, signed/float/bool numeric fields,
// invalid identifiers, overlapping profile spans and resource excesses reject
// the whole source. All addresses are aligned headerless bank-00 pattern PCs.
absl::StatusOr<std::shared_ptr<const DungeonWallProfileCollection>>
ParseDungeonWallProfiles(absl::string_view source);
absl::StatusOr<std::shared_ptr<const DungeonWallProfileCollection>>
LoadDungeonWallProfiles(const std::string& path);

// Chooses the matching binding with the most optional constraints. Equal-best
// matches are an error, even if they name the same profile. Equal-rank
// intersecting bindings are rejected while parsing. No match preserves the
// legacy snapshot exactly.
absl::StatusOr<ResolvedWallProfile> ResolveWallProfile(
    const DungeonWallProfileCollection& collection,
    const WallProfileContext& context, const DungeonTilePatterns& legacy);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_DUNGEON_WALL_PROFILE_H_
