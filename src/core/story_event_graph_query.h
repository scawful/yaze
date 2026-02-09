#ifndef YAZE_CORE_STORY_EVENT_GRAPH_QUERY_H
#define YAZE_CORE_STORY_EVENT_GRAPH_QUERY_H

#include <string>
#include <string_view>
#include <vector>

#include "core/story_event_graph.h"

namespace yaze::core {

/**
 * @brief Filter options for StoryEventGraph node search in UI.
 *
 * This is intentionally UI-friendly: a free-text query plus status toggles.
 * Matching is case-insensitive and token-based (all tokens must match).
 */
struct StoryEventNodeFilter {
  std::string query;

  bool include_locked = true;
  bool include_available = true;
  bool include_completed = true;
  bool include_blocked = true;
};

// Returns true if `status` is included by the filter toggles.
bool StoryNodeStatusAllowed(StoryNodeStatus status,
                            const StoryEventNodeFilter& filter);

// Returns true if the free-text query matches the node (case-insensitive).
// Query is split by whitespace into tokens; all tokens must match somewhere.
bool StoryEventNodeMatchesQuery(const StoryEventNode& node,
                                std::string_view query);

// Combined status + query match.
bool StoryEventNodeMatchesFilter(const StoryEventNode& node,
                                 const StoryEventNodeFilter& filter);

// Exposed for caching/debugging: builds a normalized (lowercased) searchable
// string containing the node's primary metadata (id/name/flags/locations/etc).
std::string BuildStoryEventNodeSearchText(const StoryEventNode& node);

}  // namespace yaze::core

#endif  // YAZE_CORE_STORY_EVENT_GRAPH_QUERY_H

