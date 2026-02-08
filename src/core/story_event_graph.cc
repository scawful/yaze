#include "core/story_event_graph.h"

#include <algorithm>
#include <fstream>
#include <queue>
#include <sstream>

#include "absl/status/status.h"
#include "nlohmann/json.hpp"

namespace yaze::core {

using json = nlohmann::json;

// ─── JSON Loading ────────────────────────────────────────────────

static std::vector<StoryFlag> ParseFlags(const json& arr) {
  std::vector<StoryFlag> flags;
  if (!arr.is_array()) return flags;
  for (const auto& item : arr) {
    StoryFlag flag;
    flag.name = item.value("name", "");
    flag.value = item.value("value", "");
    flag.reg = item.value("register", "");
    flag.bit = item.value("bit", -1);
    flag.operation = item.value("operation", "");
    flags.push_back(flag);
  }
  return flags;
}

static std::vector<StoryLocation> ParseLocations(const json& arr) {
  std::vector<StoryLocation> locations;
  if (!arr.is_array()) return locations;
  for (const auto& item : arr) {
    StoryLocation loc;
    loc.name = item.value("name", "");
    loc.entrance_id = item.value("entrance_id", "");
    loc.overworld_id = item.value("overworld_id", "");
    loc.special_world_id = item.value("special_world_id", "");
    loc.room_id = item.value("room_id", "");
    locations.push_back(loc);
  }
  return locations;
}

static std::vector<std::string> ParseStringArray(const json& arr) {
  std::vector<std::string> result;
  if (!arr.is_array()) return result;
  for (const auto& item : arr) {
    if (item.is_string()) {
      result.push_back(item.get<std::string>());
    }
  }
  return result;
}

absl::Status StoryEventGraph::LoadFromJson(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::NotFoundError("Cannot open story events file: " + path);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return LoadFromString(buffer.str());
}

absl::Status StoryEventGraph::LoadFromString(const std::string& json_content) {
  json root;
  try {
    root = json::parse(json_content);
  } catch (const json::parse_error& e) {
    return absl::InvalidArgumentError(
        std::string("JSON parse error: ") + e.what());
  }

  nodes_.clear();
  edges_.clear();
  node_index_.clear();

  // Parse events
  if (root.contains("events") && root["events"].is_array()) {
    for (const auto& item : root["events"]) {
      StoryEventNode node;
      node.id = item.value("id", "");
      node.name = item.value("name", "");
      node.flags = ParseFlags(item.value("flags", json::array()));
      node.locations = ParseLocations(item.value("locations", json::array()));
      node.scripts = ParseStringArray(item.value("scripts", json::array()));
      node.text_ids = ParseStringArray(item.value("text_ids", json::array()));
      node.dependencies =
          ParseStringArray(item.value("dependencies", json::array()));
      node.unlocks = ParseStringArray(item.value("unlocks", json::array()));
      node.evidence = item.value("evidence", "");
      node.last_verified = item.value("last_verified", "");
      node.notes = item.value("notes", "");

      node_index_[node.id] = nodes_.size();
      nodes_.push_back(std::move(node));
    }
  }

  // Parse edges
  if (root.contains("edges") && root["edges"].is_array()) {
    for (const auto& item : root["edges"]) {
      StoryEdge edge;
      edge.from = item.value("from", "");
      edge.to = item.value("to", "");
      edge.type = item.value("type", "dependency");
      edges_.push_back(std::move(edge));
    }
  }

  return absl::OkStatus();
}

// ─── Layout ──────────────────────────────────────────────────────

void StoryEventGraph::AutoLayout() {
  if (nodes_.empty()) return;

  // Topological sort to determine layers (BFS-based Kahn's algorithm)
  std::unordered_map<std::string, int> in_degree;
  std::unordered_map<std::string, std::vector<std::string>> adj;

  for (const auto& node : nodes_) {
    in_degree[node.id] = 0;
  }

  for (const auto& edge : edges_) {
    adj[edge.from].push_back(edge.to);
    in_degree[edge.to]++;
  }

  // BFS to assign layers
  std::queue<std::string> queue;
  std::unordered_map<std::string, int> layer;

  for (const auto& [id, deg] : in_degree) {
    if (deg == 0) {
      queue.push(id);
      layer[id] = 0;
    }
  }

  int max_layer = 0;
  while (!queue.empty()) {
    std::string current = queue.front();
    queue.pop();

    for (const auto& next : adj[current]) {
      int new_layer = layer[current] + 1;
      if (layer.find(next) == layer.end() || new_layer > layer[next]) {
        layer[next] = new_layer;
      }
      in_degree[next]--;
      if (in_degree[next] == 0) {
        queue.push(next);
        max_layer = std::max(max_layer, layer[next]);
      }
    }
  }

  // Group nodes by layer
  std::vector<std::vector<size_t>> layers(max_layer + 1);
  for (size_t i = 0; i < nodes_.size(); ++i) {
    int l = layer.count(nodes_[i].id) ? layer[nodes_[i].id] : 0;
    layers[l].push_back(i);
  }

  // Assign positions: X by layer, Y by position within layer
  constexpr float kLayerSpacing = 200.0f;
  constexpr float kNodeSpacing = 120.0f;

  for (int l = 0; l <= max_layer; ++l) {
    const size_t count = layers[l].size();
    if (count == 0) continue;

    float total_height = static_cast<float>(count - 1) * kNodeSpacing;
    float start_y = -total_height / 2.0f;

    for (size_t j = 0; j < count; ++j) {
      nodes_[layers[l][j]].pos_x = static_cast<float>(l) * kLayerSpacing;
      nodes_[layers[l][j]].pos_y =
          start_y + static_cast<float>(j) * kNodeSpacing;
    }
  }
}

// ─── Status ──────────────────────────────────────────────────────

void StoryEventGraph::UpdateStatus(uint8_t crystal_bitfield,
                                   uint8_t game_state) {
  auto completed = GetCompletedNodes(crystal_bitfield, game_state);
  std::unordered_map<std::string, bool> completed_set;
  for (const auto& id : completed) {
    completed_set[id] = true;
  }

  for (auto& node : nodes_) {
    if (completed_set.count(node.id)) {
      node.status = StoryNodeStatus::kCompleted;
      continue;
    }

    // Check if all dependencies are completed
    bool all_deps_met = true;
    for (const auto& dep : node.dependencies) {
      if (!completed_set.count(dep)) {
        all_deps_met = false;
        break;
      }
    }

    node.status =
        all_deps_met ? StoryNodeStatus::kAvailable : StoryNodeStatus::kLocked;
  }
}

std::vector<std::string> StoryEventGraph::GetCompletedNodes(
    uint8_t crystal_bitfield, uint8_t game_state) const {
  std::vector<std::string> completed;

  for (const auto& node : nodes_) {
    // Determine completion by checking flags
    bool is_completed = false;

    for (const auto& flag : node.flags) {
      // GameState checks
      if (flag.name == "IntroState" || flag.name == "GameState") {
        int required = 0;
        try {
          required = std::stoi(flag.value);
        } catch (...) {
          required = 1;  // Default: any progress means started
        }
        if (game_state >= required) {
          is_completed = true;
        }
      }

      // Crystal/dungeon completion checks
      if (flag.name.find("Crystal") != std::string::npos ||
          flag.name.find("Fortress") != std::string::npos) {
        // Any crystal means some dungeon progress
        if (crystal_bitfield != 0) {
          is_completed = true;
        }
      }
    }

    // Simple heuristic: EV-001 is always completed if game_state >= 1
    if (node.id == "EV-001" && game_state >= 1) {
      is_completed = true;
    }

    // EV-002/003 completed if game_state >= 1 (met Maku Tree = reached LoomBeach)
    if ((node.id == "EV-002" || node.id == "EV-003") && game_state >= 1) {
      is_completed = true;
    }

    // EV-005 completed when Kydrog encounter done (game_state >= 2)
    if (node.id == "EV-005" && game_state >= 2) {
      is_completed = true;
    }

    // EV-008 endgame
    if (node.id == "EV-008" && game_state >= 3) {
      is_completed = true;
    }

    if (is_completed) {
      completed.push_back(node.id);
    }
  }

  return completed;
}

const StoryEventNode* StoryEventGraph::GetNode(const std::string& id) const {
  auto it = node_index_.find(id);
  if (it != node_index_.end() && it->second < nodes_.size()) {
    return &nodes_[it->second];
  }
  return nullptr;
}

}  // namespace yaze::core
