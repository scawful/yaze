#include "app/editor/system/commands/command_palette_goto.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/events/core_events.h"
#include "app/editor/registry/content_registry.h"
#include "zelda3/common.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace editor {
namespace {

std::string Lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

bool IsSpace(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::string_view Trim(std::string_view text) {
  while (!text.empty() && IsSpace(text.front()))
    text.remove_prefix(1);
  while (!text.empty() && IsSpace(text.back()))
    text.remove_suffix(1);
  return text;
}

std::optional<int> ParseDigits(std::string_view digits, int base) {
  // Six digits covers every supported range and cannot overflow int.
  if (digits.empty() || digits.size() > 6)
    return std::nullopt;
  int value = 0;
  for (char c : digits) {
    int digit = -1;
    if (c >= '0' && c <= '9') {
      digit = c - '0';
    } else if (base == 16 && c >= 'a' && c <= 'f') {
      digit = 10 + (c - 'a');
    }
    if (digit < 0 || digit >= base)
      return std::nullopt;
    value = value * base + digit;
  }
  return value;
}

struct KeywordInfo {
  const char* word;
  GotoKind kind;
};

constexpr KeywordInfo kKeywords[] = {
    {"room", GotoKind::kRoom},       {"rm", GotoKind::kRoom},
    {"r", GotoKind::kRoom},          {"map", GotoKind::kOverworldMap},
    {"ow", GotoKind::kOverworldMap}, {"message", GotoKind::kMessage},
    {"msg", GotoKind::kMessage},     {"sprite", GotoKind::kSprite},
    {"spr", GotoKind::kSprite},
};

}  // namespace

std::optional<int> ParseGotoId(std::string_view token) {
  const std::string lower = Lower(Trim(token));
  std::string_view text(lower);
  if (text.empty())
    return std::nullopt;
  if (text.front() == '#')
    return ParseDigits(text.substr(1), 10);
  if (text.front() == '$')
    return ParseDigits(text.substr(1), 16);
  if (text.size() > 2 && text.substr(0, 2) == "0x")
    return ParseDigits(text.substr(2), 16);
  if (text.size() > 1 && text.back() == 'h')
    return ParseDigits(text.substr(0, text.size() - 1), 16);
  return ParseDigits(text, 16);
}

int GotoMaxId(GotoKind kind) {
  switch (kind) {
    case GotoKind::kRoom:
      return 0x127;
    case GotoKind::kOverworldMap:
      return zelda3::kNumOverworldMaps - 1;
    case GotoKind::kMessage:
      return 0xFFFF;
    case GotoKind::kSprite:
      return 0xFF;
  }
  return 0;
}

const char* GotoKindName(GotoKind kind) {
  switch (kind) {
    case GotoKind::kRoom:
      return "room";
    case GotoKind::kOverworldMap:
      return "map";
    case GotoKind::kMessage:
      return "message";
    case GotoKind::kSprite:
      return "sprite";
  }
  return "";
}

bool GotoKindHasJumpPath(GotoKind kind) {
  return kind != GotoKind::kSprite;
}

std::optional<GotoQuery> ParseGotoQuery(std::string_view query) {
  std::string lower = Lower(Trim(query));
  std::string_view text(lower);
  for (const char* prefix : {"go to ", "goto ", "go "}) {
    const std::string_view p(prefix);
    if (text.substr(0, p.size()) == p) {
      text = Trim(text.substr(p.size()));
      break;
    }
  }

  // Keyword: leading letters.
  size_t word_end = 0;
  while (word_end < text.size() &&
         std::isalpha(static_cast<unsigned char>(text[word_end]))) {
    ++word_end;
  }
  const std::string_view word = text.substr(0, word_end);
  const KeywordInfo* keyword = nullptr;
  for (const auto& info : kKeywords) {
    if (word == info.word) {
      keyword = &info;
      break;
    }
  }
  if (!keyword)
    return std::nullopt;

  // Separator: whitespace and/or ':' is required ("r4a" is not a go-to).
  // `query` is used untrimmed so "room " (trailing space) counts.
  std::string_view rest = text.substr(word_end);
  const bool query_has_trailing_space =
      !query.empty() && IsSpace(query.back()) && rest.empty();
  if (rest.empty() && !query_has_trailing_space)
    return std::nullopt;
  if (!rest.empty() && rest.front() != ':' && !IsSpace(rest.front()))
    return std::nullopt;
  if (!rest.empty() && rest.front() == ':')
    rest.remove_prefix(1);
  rest = Trim(rest);

  GotoQuery result;
  result.kind = keyword->kind;
  if (rest.empty()) {
    result.status = GotoQuery::Status::kMissingId;
    return result;
  }
  for (char c : rest) {
    if (IsSpace(c))
      return std::nullopt;  // "map editor stuff": not a go-to
  }
  const auto id = ParseGotoId(rest);
  if (!id)
    return std::nullopt;
  result.id = *id;
  result.status = (*id >= 0 && *id <= GotoMaxId(result.kind))
                      ? GotoQuery::Status::kOk
                      : GotoQuery::Status::kOutOfRange;
  return result;
}

CommandEntry BuildGotoEntry(const GotoQuery& query, size_t session_id) {
  CommandEntry entry;
  entry.category = CommandCategory::kNavigation;
  const char* kind_name = GotoKindName(query.kind);
  const int max_id = GotoMaxId(query.kind);
  const int width = max_id > 0xFF ? 3 : 2;

  if (query.status == GotoQuery::Status::kMissingId) {
    entry.name = absl::StrFormat("Go to %s …", kind_name);
    entry.enabled = false;
    entry.note = absl::StrFormat(
        "Type an id: 4A, 0x4A, $4A or 4Ah (hex), #74 (decimal). Range "
        "0x%0*X-0x%0*X",
        width, 0, width, max_id);
    return entry;
  }
  if (query.status == GotoQuery::Status::kOutOfRange) {
    entry.name = absl::StrFormat("Go to %s 0x%0*X", kind_name, width, query.id);
    entry.enabled = false;
    entry.note = absl::StrFormat("Out of range: %s ids are 0x%0*X-0x%0*X",
                                 kind_name, width, 0, width, max_id);
    return entry;
  }

  std::string label;
  switch (query.kind) {
    case GotoKind::kRoom:
      label = zelda3::GetRoomLabel(query.id);
      break;
    case GotoKind::kOverworldMap:
      label = zelda3::GetOverworldMapLabel(query.id);
      break;
    case GotoKind::kSprite:
      label = zelda3::GetSpriteLabel(query.id);
      break;
    case GotoKind::kMessage:
      break;  // Messages have no resource label; the editor shows the text.
  }
  entry.name =
      label.empty()
          ? absl::StrFormat("Go to %s 0x%0*X", kind_name, width, query.id)
          : absl::StrFormat("Go to %s 0x%0*X — %s", kind_name, width, query.id,
                            label);
  entry.description = absl::StrFormat("Jump to %s 0x%0*X (%d)", kind_name,
                                      width, query.id, query.id);

  if (!GotoKindHasJumpPath(query.kind)) {
    entry.enabled = false;
    entry.note = "No sprite jump path yet; open the Sprite editor manually";
    return entry;
  }

  const int id = query.id;
  switch (query.kind) {
    case GotoKind::kRoom:
      entry.callback = [id, session_id]() {
        if (auto* bus = ContentRegistry::Context::event_bus())
          bus->Publish(JumpToRoomRequestEvent::Create(id, session_id));
      };
      break;
    case GotoKind::kOverworldMap:
      entry.callback = [id, session_id]() {
        if (auto* bus = ContentRegistry::Context::event_bus())
          bus->Publish(JumpToMapRequestEvent::Create(id, session_id));
      };
      break;
    case GotoKind::kMessage:
      entry.callback = [id, session_id]() {
        if (auto* bus = ContentRegistry::Context::event_bus())
          bus->Publish(JumpToMessageRequestEvent::Create(id, session_id));
      };
      break;
    case GotoKind::kSprite:
      break;
  }
  return entry;
}

}  // namespace editor
}  // namespace yaze
