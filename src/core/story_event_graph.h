#ifndef YAZE_CORE_STORY_EVENT_GRAPH_H
#define YAZE_CORE_STORY_EVENT_GRAPH_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"

namespace yaze::core {

/**
 * @brief A flag set or cleared by a story event.
 */
struct StoryFlag {
  std::string name;
  std::string value;      // e.g., "2" for GameState=2
  std::string reg;        // e.g., "OOSPROG" for bit-based flags
  int bit = -1;           // Bit index if applicable (-1 = not a bitfield)
  std::string operation;  // "increment", "set", etc.
};

/**
 * @brief A location associated with a story event.
 */
struct StoryLocation {
  std::string name;
  std::string entrance_id;       // e.g., "0/1", "2"
  std::string overworld_id;      // e.g., "0x33"
  std::string special_world_id;  // e.g., "0x80"
  std::string room_id;           // e.g., "0x202"
};

/**
 * @brief A directed edge in the story event graph.
 */
struct StoryEdge {
  std::string from;
  std::string to;
  std::string type;  // "dependency"
};

/**
 * @brief Completion status of a story event node for rendering.
 */
enum class StoryNodeStatus : uint8_t {
  kLocked,     // Prerequisites not met (gray)
  kAvailable,  // Prerequisites met, not yet done (yellow)
  kCompleted,  // Event has occurred (green)
  kBlocked,    // Has a blocker annotation (red)
};

/**
 * @brief A node in the Oracle story event graph.
 *
 * Each node represents a narrative event with:
 * - Flags it sets/clears in SRAM
 * - Locations/rooms where it occurs
 * - Scripts/routines that trigger it
 * - Text/message IDs associated with it
 * - Dependencies (inbound edges) and unlocks (outbound edges)
 */
struct StoryEventNode {
  std::string id;    // "EV-001"
  std::string name;  // "Intro begins"

  std::vector<StoryFlag> flags;
  std::vector<StoryLocation> locations;
  std::vector<std::string> scripts;
  std::vector<std::string> text_ids;  // hex: "0x1F"

  std::vector<std::string> dependencies;  // Inbound edge source IDs
  std::vector<std::string> unlocks;       // Outbound edge target IDs

  std::string evidence;
  std::string last_verified;
  std::string notes;

  // Layout position (computed by AutoLayout)
  float pos_x = 0.0f;
  float pos_y = 0.0f;

  // Runtime state
  StoryNodeStatus status = StoryNodeStatus::kLocked;
};

/**
 * @class StoryEventGraph
 * @brief The complete Oracle narrative progression graph.
 *
 * Loaded from `story_events.json` in the Oracle code folder.
 * Used by the StoryEventGraphPanel to render an interactive node graph
 * colored by SRAM completion state.
 *
 * Usage:
 *   StoryEventGraph graph;
 *   auto status = graph.LoadFromJson(path);
 *   if (status.ok()) {
 *     graph.AutoLayout();
 *     graph.UpdateStatus(crystal_bitfield, game_state);
 *     // Render nodes with graph.nodes()
 *   }
 */
class StoryEventGraph {
 public:
  StoryEventGraph() = default;

  /**
   * @brief Load the graph from a JSON file.
   */
  absl::Status LoadFromJson(const std::string& path);

  /**
   * @brief Load the graph from a JSON string.
   */
  absl::Status LoadFromString(const std::string& json_content);

  /**
   * @brief Check if the graph has been loaded.
   */
  [[nodiscard]] bool loaded() const { return !nodes_.empty(); }

  /**
   * @brief Compute layout positions using topological sort + layered
   * positioning.
   *
   * Places nodes in horizontal layers based on dependency depth.
   * Within each layer, nodes are spaced vertically.
   */
  void AutoLayout();

  /**
   * @brief Update node completion status based on SRAM state.
   *
   * Determines which events are completed, available, or locked
   * based on the crystal bitfield and game state.
   */
  void UpdateStatus(uint8_t crystal_bitfield, uint8_t game_state);

  /**
   * @brief Get IDs of events that are completed based on SRAM state.
   */
  [[nodiscard]] std::vector<std::string> GetCompletedNodes(
      uint8_t crystal_bitfield, uint8_t game_state) const;

  // ─── Accessors ─────────────────────────────────────────────────

  [[nodiscard]] const std::vector<StoryEventNode>& nodes() const {
    return nodes_;
  }

  [[nodiscard]] const std::vector<StoryEdge>& edges() const { return edges_; }

  [[nodiscard]] const StoryEventNode* GetNode(const std::string& id) const;

 private:
  std::vector<StoryEventNode> nodes_;
  std::vector<StoryEdge> edges_;
  std::unordered_map<std::string, size_t> node_index_;  // id -> nodes_ index
};

}  // namespace yaze::core

#endif  // YAZE_CORE_STORY_EVENT_GRAPH_H
