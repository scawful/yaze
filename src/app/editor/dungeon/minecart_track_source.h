#ifndef YAZE_APP_EDITOR_DUNGEON_MINECART_TRACK_SOURCE_H_
#define YAZE_APP_EDITOR_DUNGEON_MINECART_TRACK_SOURCE_H_

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::editor {

inline constexpr size_t kMinecartTrackSlotCount = 32;

struct MinecartTrack {
  int id;
  int room_id;
  int start_x;
  int start_y;

  bool operator==(const MinecartTrack&) const = default;
};

struct MinecartTrackSourceTokenSpan {
  size_t offset = 0;
  size_t length = 0;

  bool operator==(const MinecartTrackSourceTokenSpan&) const = default;
};

// Losslessly models the three minecart `dw` tables. Parsing retains the exact
// source bytes and the spans of only the active table tokens. Rendering can
// therefore replace edited `$hhhh` operands without reconstructing comments,
// whitespace, guard directives, or the guarded disabled branches.
class MinecartTrackSourceDocument {
 public:
  using FieldTokenSpans =
      std::array<MinecartTrackSourceTokenSpan, kMinecartTrackSlotCount>;
  using EditableTokenSpans = std::array<FieldTokenSpans, 3>;

  static absl::StatusOr<MinecartTrackSourceDocument> Parse(
      std::string source_bytes);

  absl::StatusOr<std::string> Render(
      const std::vector<MinecartTrack>& tracks) const;

  [[nodiscard]] const std::string& source_bytes() const {
    return source_bytes_;
  }
  [[nodiscard]] const std::vector<MinecartTrack>& tracks() const {
    return tracks_;
  }
  [[nodiscard]] const EditableTokenSpans& editable_token_spans() const {
    return editable_token_spans_;
  }
  [[nodiscard]] bool guarded() const { return guarded_; }

 private:
  std::string source_bytes_;
  std::vector<MinecartTrack> tracks_;
  EditableTokenSpans editable_token_spans_{};
  bool guarded_ = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_MINECART_TRACK_SOURCE_H_
