#ifndef YAZE_APP_EDITOR_SYSTEM_COMMANDS_COMMAND_PALETTE_GOTO_H_
#define YAZE_APP_EDITOR_SYSTEM_COMMANDS_COMMAND_PALETTE_GOTO_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "app/editor/system/commands/command_palette.h"

namespace yaze {
namespace editor {

/**
 * Go-to queries typed into the command palette.
 *
 * Grammar (case-insensitive, optional leading "go to" / "goto"):
 *   room|r|rm <id>        dungeon room      0x000-0x127
 *   map|ow <id>           overworld map     0x00-0x9F
 *   msg|message <id>      message           0x000-0xFFFF (editor validates)
 *   sprite|spr <id>       sprite            0x00-0xFF
 * The keyword and id are separated by whitespace and/or ':'.
 *
 * Id formats. yaze shows ids in hex everywhere, so hex is the default:
 *   4A  0x4A  $4A  4Ah     -> hex 0x4A
 *   #74                    -> decimal 74 (the only decimal form)
 */
enum class GotoKind { kRoom, kOverworldMap, kMessage, kSprite };

struct GotoQuery {
  enum class Status {
    kOk,          // id parsed and in range
    kMissingId,   // keyword + separator typed, id not yet
    kOutOfRange,  // id parsed but outside the kind's range
  };
  GotoKind kind = GotoKind::kRoom;
  Status status = Status::kOk;
  int id = -1;
};

/// Parse @p query. Returns nullopt when the text is not a go-to query (no
/// keyword, no separator, or an id token that is not a number), so ordinary
/// searches like "map editor" fall through to fuzzy search.
std::optional<GotoQuery> ParseGotoQuery(std::string_view query);

/// Parse a single id token using the formats above. nullopt if malformed.
std::optional<int> ParseGotoId(std::string_view token);

/// Inclusive upper bound for @p kind.
int GotoMaxId(GotoKind kind);
const char* GotoKindName(GotoKind kind);

/// True when a jump path exists for @p kind (sprites have none yet).
bool GotoKindHasJumpPath(GotoKind kind);

/// Build the palette row for @p query: "Go to room 0x04A — <label>". The
/// entry is disabled (with `note`) when the id is missing/out of range or the
/// kind has no jump path. Enabled entries publish the matching Jump*Request
/// event on the ContentRegistry event bus.
CommandEntry BuildGotoEntry(const GotoQuery& query, size_t session_id);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_COMMANDS_COMMAND_PALETTE_GOTO_H_
