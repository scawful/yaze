#include "core/story_event_graph.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <queue>
#include <sstream>
#include <string_view>

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

static int ParseIntFlexible(const json& obj, const char* key, int def) {
  if (!obj.is_object() || !obj.contains(key)) {
    return def;
  }
  const auto& v = obj.at(key);
  if (v.is_number_integer()) {
    return v.get<int>();
  }
  if (v.is_number_unsigned()) {
    const auto u = v.get<unsigned int>();
    return (u > static_cast<unsigned int>(std::numeric_limits<int>::max()))
               ? def
               : static_cast<int>(u);
  }
  if (v.is_string()) {
    try {
      size_t idx = 0;
      const std::string s = v.get<std::string>();
      const int parsed = std::stoi(s, &idx, 0);
      if (idx == s.size()) {
        return parsed;
      }
    } catch (...) {
    }
  }
  return def;
}

static std::vector<std::string> ParseScriptArray(const json& arr) {
  std::vector<std::string> result;
  if (!arr.is_array()) return result;

  for (const auto& item : arr) {
    if (item.is_string()) {
      result.push_back(item.get<std::string>());
      continue;
    }
    if (!item.is_object()) {
      continue;
    }

    // Allow the generator to embed a fully composed reference.
    if (item.contains("ref") && item["ref"].is_string()) {
      result.push_back(item["ref"].get<std::string>());
      continue;
    }

    const std::string file = item.value("file", "");
    const std::string symbol = item.value("symbol", "");
    if (!symbol.empty()) {
      if (!file.empty()) {
        result.push_back(file + ":" + symbol);
      } else {
        result.push_back(symbol);
      }
      continue;
    }

    // Legacy / fallback: file + line number (fragile, but supported).
    const int line = ParseIntFlexible(item, "line", -1);
    if (line > 0 && !file.empty()) {
      result.push_back(file + ":" + std::to_string(line));
    }
  }

  return result;
}

static uint32_t ParseUintFlexible(const json& obj, const char* key,
                                  uint32_t def) {
  if (!obj.is_object() || !obj.contains(key)) {
    return def;
  }
  const auto& v = obj.at(key);
  if (v.is_number_unsigned()) {
    return v.get<uint32_t>();
  }
  if (v.is_number_integer()) {
    const int64_t i = v.get<int64_t>();
    return (i < 0) ? def : static_cast<uint32_t>(i);
  }
  if (v.is_string()) {
    try {
      size_t idx = 0;
      const std::string s = v.get<std::string>();
      const unsigned long parsed = std::stoul(s, &idx, 0);
      if (idx == s.size()) {
        return static_cast<uint32_t>(parsed);
      }
    } catch (...) {
    }
  }
  return def;
}

static std::vector<StoryPredicate> ParsePredicates(const json& arr) {
  std::vector<StoryPredicate> preds;
  if (!arr.is_array()) return preds;
  for (const auto& item : arr) {
    if (!item.is_object()) continue;
    StoryPredicate p;
    p.reg = item.value("register", "");
    if (p.reg.empty()) {
      p.reg = item.value("reg", "");
    }
    p.op = item.value("op", "");
    p.value = ParseIntFlexible(item, "value", 0);
    p.bit = ParseIntFlexible(item, "bit", -1);

    // For mask operations, accept either explicit "mask" or reuse "value".
    p.mask = ParseUintFlexible(item, "mask", 0);
    if (p.mask == 0 && item.contains("value")) {
      p.mask = static_cast<uint32_t>(ParseUintFlexible(item, "value", 0));
    }

    preds.push_back(std::move(p));
  }
  return preds;
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
      node.scripts = ParseScriptArray(item.value("scripts", json::array()));
      node.text_ids = ParseStringArray(item.value("text_ids", json::array()));
      node.completed_when =
          ParsePredicates(item.value("completed_when", json::array()));
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

namespace {

std::string NormalizeRegKey(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (unsigned char ch : input) {
    if (std::isalnum(ch)) {
      out.push_back(static_cast<char>(std::toupper(ch)));
    }
  }
  return out;
}

std::optional<uint32_t> GetRegisterValue(const OracleProgressionState& state,
                                        std::string_view reg) {
  const std::string key = NormalizeRegKey(reg);
  if (key == "CRYSTALS" || key == "CRYSTAL" || key == "CRYSTALBITFIELD") {
    return state.crystal_bitfield;
  }
  if (key == "GAMESTATE" || key == "GAME") {
    return state.game_state;
  }
  if (key == "OOSPROG") {
    return state.oosprog;
  }
  if (key == "OOSPROG2") {
    return state.oosprog2;
  }
  if (key == "SIDEQUEST" || key == "SIDEQUESTS") {
    return state.side_quest;
  }
  if (key == "PENDANTS" || key == "PENDANT") {
    return state.pendants;
  }
  return std::nullopt;
}

bool EvaluatePredicate(const StoryPredicate& p,
                       const OracleProgressionState& state) {
  const auto reg_val_opt = GetRegisterValue(state, p.reg);
  if (!reg_val_opt.has_value()) {
    return false;
  }
  const uint32_t reg_val = *reg_val_opt;

  const std::string op = NormalizeRegKey(p.op);
  if (op == "BITSET") {
    if (p.bit < 0 || p.bit >= 32) return false;
    return (reg_val & (1u << static_cast<uint32_t>(p.bit))) != 0;
  }
  if (op == "BITCLEAR") {
    if (p.bit < 0 || p.bit >= 32) return false;
    return (reg_val & (1u << static_cast<uint32_t>(p.bit))) == 0;
  }
  if (op == "MASKANY") {
    return p.mask != 0 && (reg_val & p.mask) != 0;
  }
  if (op == "MASKALL") {
    return p.mask != 0 && (reg_val & p.mask) == p.mask;
  }

  // Integer comparisons.
  const int lhs = static_cast<int>(reg_val);
  const int rhs = p.value;
  if (p.op == "==" || op == "EQ") return lhs == rhs;
  if (p.op == "!=" || op == "NE") return lhs != rhs;
  if (p.op == ">=" || op == "GE") return lhs >= rhs;
  if (p.op == "<=" || op == "LE") return lhs <= rhs;
  if (p.op == ">" || op == "GT") return lhs > rhs;
  if (p.op == "<" || op == "LT") return lhs < rhs;

  return false;
}

}  // namespace

void StoryEventGraph::UpdateStatus(uint8_t crystal_bitfield,
                                   uint8_t game_state) {
  OracleProgressionState state;
  state.crystal_bitfield = crystal_bitfield;
  state.game_state = game_state;
  UpdateStatus(state);
}

void StoryEventGraph::UpdateStatus(const OracleProgressionState& state) {
  auto completed = GetCompletedNodes(state);
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
  OracleProgressionState state;
  state.crystal_bitfield = crystal_bitfield;
  state.game_state = game_state;
  return GetCompletedNodes(state);
}

std::vector<std::string> StoryEventGraph::GetCompletedNodes(
    const OracleProgressionState& state) const {
  std::vector<std::string> completed;

  for (const auto& node : nodes_) {
    // Primary: explicit completion predicates (if present).
    if (!node.completed_when.empty()) {
      bool ok = true;
      for (const auto& p : node.completed_when) {
        if (!EvaluatePredicate(p, state)) {
          ok = false;
          break;
        }
      }
      if (ok) {
        completed.push_back(node.id);
      }
      continue;
    }

    // Fallback: heuristics (legacy behavior, keeps older story_events.json usable).
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
        if (state.game_state >= required) {
          is_completed = true;
        }
      }

      // Crystal/dungeon completion checks
      if (flag.name.find("Crystal") != std::string::npos ||
          flag.name.find("Fortress") != std::string::npos) {
        // Any crystal means some dungeon progress
        if (state.crystal_bitfield != 0) {
          is_completed = true;
        }
      }
    }

    // Heuristic anchors for early graph usefulness (until predicates are filled in).
    if (node.id == "EV-001" && state.game_state >= 1) {
      is_completed = true;
    }
    if ((node.id == "EV-002" || node.id == "EV-003") && state.game_state >= 1) {
      is_completed = true;
    }
    if (node.id == "EV-005" && state.game_state >= 2) {
      is_completed = true;
    }
    if (node.id == "EV-008" && state.game_state >= 3) {
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
