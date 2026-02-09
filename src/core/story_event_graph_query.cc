#include "core/story_event_graph_query.h"

#include <cctype>
#include <sstream>

namespace yaze::core {

namespace {

std::string ToLower(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (unsigned char ch : input) {
    out.push_back(static_cast<char>(std::tolower(ch)));
  }
  return out;
}

std::vector<std::string> SplitQueryTokens(std::string_view query) {
  std::vector<std::string> tokens;
  std::string current;
  current.reserve(query.size());

  for (unsigned char ch : query) {
    if (std::isspace(ch)) {
      if (!current.empty()) {
        tokens.push_back(ToLower(current));
        current.clear();
      }
      continue;
    }
    current.push_back(static_cast<char>(ch));
  }

  if (!current.empty()) {
    tokens.push_back(ToLower(current));
  }

  return tokens;
}

bool AllTokensMatch(std::string_view haystack_lower,
                    const std::vector<std::string>& tokens_lower) {
  for (const auto& token : tokens_lower) {
    if (token.empty()) continue;
    if (haystack_lower.find(token) == std::string_view::npos) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool StoryNodeStatusAllowed(StoryNodeStatus status,
                            const StoryEventNodeFilter& filter) {
  switch (status) {
    case StoryNodeStatus::kLocked:
      return filter.include_locked;
    case StoryNodeStatus::kAvailable:
      return filter.include_available;
    case StoryNodeStatus::kCompleted:
      return filter.include_completed;
    case StoryNodeStatus::kBlocked:
      return filter.include_blocked;
    default:
      return true;
  }
}

std::string BuildStoryEventNodeSearchText(const StoryEventNode& node) {
  std::ostringstream out;

  // Primary identifiers.
  out << node.id << ' ' << node.name << ' ';

  // Flags.
  for (const auto& flag : node.flags) {
    out << flag.name << ' ' << flag.reg << ' ' << flag.value << ' '
        << flag.operation << ' ';
    if (flag.bit >= 0) out << flag.bit << ' ';
  }

  // Locations (include IDs so search can match "0x25"/"0/1" etc).
  for (const auto& loc : node.locations) {
    out << loc.name << ' ' << loc.entrance_id << ' ' << loc.overworld_id << ' '
        << loc.special_world_id << ' ' << loc.room_id << ' ';
  }

  // Script references and message IDs.
  for (const auto& s : node.scripts) out << s << ' ';
  for (const auto& tid : node.text_ids) out << tid << ' ';

  // Misc notes/evidence.
  out << node.evidence << ' ' << node.last_verified << ' ' << node.notes << ' ';

  // Dependencies (useful when searching by event id).
  for (const auto& dep : node.dependencies) out << dep << ' ';
  for (const auto& u : node.unlocks) out << u << ' ';

  return ToLower(out.str());
}

bool StoryEventNodeMatchesQuery(const StoryEventNode& node,
                                std::string_view query) {
  const auto tokens = SplitQueryTokens(query);
  if (tokens.empty()) return true;

  const std::string haystack = BuildStoryEventNodeSearchText(node);
  return AllTokensMatch(haystack, tokens);
}

bool StoryEventNodeMatchesFilter(const StoryEventNode& node,
                                 const StoryEventNodeFilter& filter) {
  if (!StoryNodeStatusAllowed(node.status, filter)) {
    return false;
  }
  return StoryEventNodeMatchesQuery(node, filter.query);
}

}  // namespace yaze::core

