#include "app/editor/system/commands/command_palette.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "absl/strings/str_format.h"
#include "app/editor/events/core_events.h"
#include "app/editor/menu/right_drawer_manager.h"
#include "app/editor/registry/content_registry.h"
#include "app/editor/shell/coordinator/recent_projects_model.h"
#include "app/editor/system/workspace/editor_registry.h"
#include "app/editor/system/workspace/panel_host.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "core/project.h"
#include "util/json.h"
#include "util/log.h"
#include "zelda3/common.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace editor {

namespace {

constexpr int kScoreExact = 1000;
constexpr int kScorePrefix = 800;
constexpr int kScoreWordStartSubstring = 600;
constexpr int kScoreSubstring = 400;
constexpr int kScoreSubsequenceMax = kScoreSubstring - 1;

// Per-character weights for subsequence matches. The word-start bonus is
// larger than the consecutive bonus so "ow" prefers "Show: OverWorld"-style
// initials over two adjacent letters inside one word.
constexpr int kCharBase = 2;
constexpr int kCharWordStartBonus = 8;
constexpr int kCharConsecutiveBonus = 4;

// The shortcuts provider id; its entries lose representative ties so the
// palette-native name is shown while the live keybinding is inherited.
constexpr const char* kShortcutsProviderId = "shortcuts";

std::string ToLower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

bool IsAlnum(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

bool IsWordStartAt(const std::string& text, size_t index) {
  return index == 0 || !IsAlnum(text[index - 1]);
}

std::vector<std::string> SplitWhitespace(const std::string& text) {
  std::vector<std::string> tokens;
  std::string current;
  for (char c : text) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty())
    tokens.push_back(current);
  return tokens;
}

// Best in-order alignment of `query` inside `text` (both lowercase).
// Returns -1 when `query` is not a subsequence.
int BestSubsequenceScore(const std::string& text, const std::string& query) {
  const size_t n = text.size();
  const size_t m = query.size();
  if (m == 0 || m > n)
    return -1;
  constexpr int kNone = -1;
  // prev[i]: best score with query[0..j-1] matched and query[j-1] at text[i].
  std::vector<int> prev(n, kNone);
  std::vector<int> cur(n, kNone);
  for (size_t j = 0; j < m; ++j) {
    int best_before = kNone;  // max of prev[k] for k < i - 1
    for (size_t i = 0; i < n; ++i) {
      cur[i] = kNone;
      if (i >= 2 && prev[i - 2] > best_before)
        best_before = prev[i - 2];
      if (text[i] != query[j])
        continue;
      int char_score = kCharBase;
      if (IsWordStartAt(text, i))
        char_score += kCharWordStartBonus;
      if (j == 0) {
        cur[i] = char_score;
        continue;
      }
      int best = kNone;
      if (best_before != kNone)
        best = best_before + char_score;
      if (i >= 1 && prev[i - 1] != kNone) {
        best = std::max(best, prev[i - 1] + char_score + kCharConsecutiveBonus);
      }
      cur[i] = best;
    }
    std::swap(prev, cur);
  }
  int result = kNone;
  for (int v : prev)
    result = std::max(result, v);
  return result;
}

int ScoreSingle(const std::string& text, const std::string& query) {
  if (query.empty() || text.empty())
    return 0;
  if (text == query)
    return kScoreExact;
  size_t pos = text.find(query);
  if (pos == 0)
    return kScorePrefix;
  if (pos != std::string::npos) {
    while (pos != std::string::npos) {
      if (IsWordStartAt(text, pos))
        return kScoreWordStartSubstring;
      pos = text.find(query, pos + 1);
    }
    return kScoreSubstring;
  }
  const int subsequence = BestSubsequenceScore(text, query);
  if (subsequence < 0)
    return 0;
  return std::min(kScoreSubsequenceMax, 1 + subsequence);
}

// True when `name` contains the word "Panel"/"Panels" (legacy alias names).
bool HasPanelWord(std::string_view name) {
  const std::string lower = ToLower(name);
  for (size_t pos = lower.find("panel"); pos != std::string::npos;
       pos = lower.find("panel", pos + 1)) {
    const size_t end = pos + 5;
    const bool starts = pos == 0 || !IsAlnum(lower[pos - 1]);
    const bool ends = end == lower.size() || !IsAlnum(lower[end]) ||
                      (lower[end] == 's' &&
                       (end + 1 == lower.size() || !IsAlnum(lower[end + 1])));
    if (starts && ends)
      return true;
  }
  return false;
}

int FrecencyBoost(int usage_count, int64_t last_used_ms, int64_t now_ms) {
  int boost = std::min(usage_count, 20) * 3;  // <= 60
  if (last_used_ms > 0) {
    const int64_t age_ms = now_ms - last_used_ms;
    if (age_ms < 60LL * 1000) {
      boost += 50;
    } else if (age_ms < 60LL * 60 * 1000) {
      boost += 30;
    } else if (age_ms < 24LL * 60 * 60 * 1000) {
      boost += 15;
    }
  }
  return boost;  // <= 110, below the 200-point gap between match tiers
}

}  // namespace

int64_t CommandPalette::NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void CommandPalette::AddCommand(const std::string& name,
                                const std::string& category,
                                const std::string& description,
                                const std::string& shortcut,
                                std::function<void()> callback) {
  CommandEntry entry;
  entry.name = name;
  entry.category = category;
  entry.description = description;
  entry.shortcut = shortcut;
  entry.callback = std::move(callback);
  entry.provider_id = current_provider_id_;
  ++generation_;
  if (auto it = usage_.find(name); it != usage_.end()) {
    entry.usage_count = it->second.count;
    entry.last_used_ms = it->second.last_used_ms;
  }
  commands_[name] = std::move(entry);
}

void CommandPalette::Clear() {
  ++generation_;
  commands_.clear();
  providers_.clear();
  current_provider_id_.clear();
}

void CommandPalette::RegisterProvider(
    std::unique_ptr<CommandProvider> provider) {
  if (!provider)
    return;
  const std::string id = provider->ProviderId();
  if (id.empty()) {
    LOG_WARN("CommandPalette",
             "Provider with empty id refused; skipping registration");
    return;
  }
  // Replace any prior provider with the same id (warn so duplicate-id bugs
  // don't hide silently).
  for (auto it = providers_.begin(); it != providers_.end(); ++it) {
    if ((*it)->ProviderId() == id) {
      LOG_WARN("CommandPalette", "Replacing existing CommandProvider '%s'",
               id.c_str());
      RemoveProviderCommands(id);
      providers_.erase(it);
      break;
    }
  }
  CommandProvider* raw = provider.get();
  providers_.push_back(std::move(provider));
  current_provider_id_ = id;
  raw->Provide(this);
  current_provider_id_.clear();
}

void CommandPalette::UnregisterProvider(const std::string& provider_id) {
  RemoveProviderCommands(provider_id);
  providers_.erase(
      std::remove_if(providers_.begin(), providers_.end(),
                     [&](const std::unique_ptr<CommandProvider>& p) {
                       return p && p->ProviderId() == provider_id;
                     }),
      providers_.end());
}

void CommandPalette::RefreshProvider(const std::string& provider_id) {
  for (auto& provider : providers_) {
    if (provider && provider->ProviderId() == provider_id) {
      RemoveProviderCommands(provider_id);
      current_provider_id_ = provider_id;
      provider->Provide(this);
      current_provider_id_.clear();
      return;
    }
  }
}

void CommandPalette::RefreshProviders() {
  // Snapshot ids so we don't iterate while mutating.
  std::vector<std::string> ids;
  ids.reserve(providers_.size());
  for (const auto& provider : providers_) {
    if (provider)
      ids.push_back(provider->ProviderId());
  }
  for (const auto& id : ids) {
    RefreshProvider(id);
  }
}

void CommandPalette::RemoveProviderCommands(const std::string& provider_id) {
  if (provider_id.empty())
    return;
  ++generation_;
  for (auto it = commands_.begin(); it != commands_.end();) {
    if (it->second.provider_id == provider_id) {
      it = commands_.erase(it);
    } else {
      ++it;
    }
  }
}

bool CommandPalette::RecordUsage(const std::string& name) {
  const std::string canonical = CanonicalCommandName(name);
  if (canonical.empty())
    return false;
  const int64_t now = NowMs();
  auto& stats = usage_[canonical];
  // Fold in any stats recorded under another member of the dedupe group so
  // the count keeps growing from the group's best value.
  for (const auto& [other_name, entry] : commands_) {
    if (other_name != canonical &&
        NormalizeCommandName(other_name) == NormalizeCommandName(canonical)) {
      stats.count = std::max(stats.count, entry.usage_count);
    }
  }
  stats.count++;
  stats.last_used_ms = now;
  ++generation_;
  if (auto it = commands_.find(canonical); it != commands_.end()) {
    it->second.usage_count = stats.count;
    it->second.last_used_ms = stats.last_used_ms;
  }
  return true;
}

/*static*/ int CommandPalette::FuzzyScore(const std::string& text,
                                          const std::string& query) {
  return ScoreText(text, query);
}

/*static*/ int CommandPalette::ScoreText(std::string_view text,
                                         std::string_view query) {
  const std::string query_lower = ToLower(query);
  const std::string text_lower = ToLower(text);
  const int whole = ScoreSingle(text_lower, query_lower);
  if (whole > 0)
    return whole;

  // Multi-word queries ("open room", "pin dungeon") match when every token
  // matches somewhere, regardless of order. Stays in the subsequence tier.
  const auto tokens = SplitWhitespace(query_lower);
  if (tokens.size() < 2)
    return 0;
  int total = 0;
  for (const auto& token : tokens) {
    const int token_score = ScoreSingle(text_lower, token);
    if (token_score <= 0)
      return 0;
    total += token_score;
  }
  const int average = total / static_cast<int>(tokens.size());
  return std::clamp(average / 2, 1, kScoreSubsequenceMax);
}

/*static*/ int CommandPalette::ScoreEntry(const CommandEntry& entry,
                                          std::string_view query,
                                          int64_t now_ms) {
  int score = ScoreText(entry.name, query);
  score = std::max(score, ScoreText(entry.category, query) / 2);
  score = std::max(score, ScoreText(entry.description, query) / 4);
  if (score <= 0)
    return 0;
  return score + FrecencyBoost(entry.usage_count, entry.last_used_ms, now_ms);
}

/*static*/ std::string CommandPalette::NormalizeCommandName(
    std::string_view name) {
  std::string lower = ToLower(name);
  // "(Alt)" marks alternate bindings of the same action.
  for (size_t pos = lower.find("(alt)"); pos != std::string::npos;
       pos = lower.find("(alt)")) {
    lower.erase(pos, 5);
  }
  std::vector<std::string> words;
  std::string current;
  for (char c : lower) {
    if (IsAlnum(c)) {
      current.push_back(c);
    } else if (!current.empty()) {
      words.push_back(std::move(current));
      current.clear();
    }
  }
  if (!current.empty())
    words.push_back(std::move(current));
  std::string out;
  for (auto& word : words) {
    // Legacy "Panel" names are aliases of the "Window" names.
    if (word == "panel")
      word = "window";
    else if (word == "panels")
      word = "windows";
    if (!out.empty())
      out.push_back(' ');
    out += word;
  }
  return out;
}

/*static*/ bool CommandPalette::IsInternalCommandId(std::string_view name) {
  if (name.empty())
    return true;
  for (char c : name) {
    if (std::isspace(static_cast<unsigned char>(c)))
      return false;
  }
  return name.find('.') != std::string_view::npos ||
         name.find('_') != std::string_view::npos ||
         std::islower(static_cast<unsigned char>(name.front())) != 0;
}

std::vector<CommandEntry> CommandPalette::GetVisibleCommands() const {
  std::vector<const CommandEntry*> sorted;
  sorted.reserve(commands_.size());
  for (const auto& [name, entry] : commands_) {
    if (IsInternalCommandId(entry.name))
      continue;
    sorted.push_back(&entry);
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const CommandEntry* a, const CommandEntry* b) {
              return a->name < b->name;
            });

  // Lower rank wins the representative slot: palette-native before
  // ShortcutManager mirrors, then "Window" names before legacy "Panel"
  // aliases (the Window names own the chords).
  auto rank_of = [](const CommandEntry& entry) {
    int rank = 0;
    if (entry.provider_id == kShortcutsProviderId)
      rank += 2;
    if (HasPanelWord(entry.name))
      rank += 1;
    return rank;
  };

  struct Group {
    const CommandEntry* rep = nullptr;
    int rep_rank = 0;
    std::string live_shortcut;  // from the shortcuts provider
    int live_rank = 0;
    std::string other_shortcut;  // first static hint
    std::function<void()> fallback_callback;
    int usage_count = 0;
    int64_t last_used_ms = 0;
  };
  std::vector<std::string> order;
  std::unordered_map<std::string, Group> groups;
  for (const CommandEntry* entry : sorted) {
    const std::string key = NormalizeCommandName(entry->name);
    auto [it, inserted] = groups.try_emplace(key);
    if (inserted)
      order.push_back(key);
    Group& group = it->second;
    const bool is_shortcut = entry->provider_id == kShortcutsProviderId;
    const int rank = rank_of(*entry);
    if (!group.rep || rank < group.rep_rank) {
      group.rep = entry;
      group.rep_rank = rank;
    }
    if (!entry->shortcut.empty()) {
      if (is_shortcut) {
        if (group.live_shortcut.empty() || rank < group.live_rank) {
          group.live_shortcut = entry->shortcut;
          group.live_rank = rank;
        }
      } else if (group.other_shortcut.empty()) {
        group.other_shortcut = entry->shortcut;
      }
    }
    if (!group.fallback_callback && entry->callback)
      group.fallback_callback = entry->callback;
    group.usage_count = std::max(group.usage_count, entry->usage_count);
    group.last_used_ms = std::max(group.last_used_ms, entry->last_used_ms);
  }

  std::vector<CommandEntry> result;
  result.reserve(order.size());
  for (const auto& key : order) {
    const Group& group = groups[key];
    CommandEntry entry = *group.rep;
    if (!group.live_shortcut.empty()) {
      entry.shortcut = group.live_shortcut;
    } else if (entry.shortcut.empty()) {
      entry.shortcut = group.other_shortcut;
    }
    if (!entry.callback)
      entry.callback = group.fallback_callback;
    entry.usage_count = group.usage_count;
    entry.last_used_ms = group.last_used_ms;
    result.push_back(std::move(entry));
  }
  return result;
}

std::string CommandPalette::CanonicalCommandName(
    const std::string& name) const {
  if (name.empty())
    return {};
  const std::string key = NormalizeCommandName(name);
  for (const auto& entry : GetVisibleCommands()) {
    if (NormalizeCommandName(entry.name) == key)
      return entry.name;
  }
  return {};
}

std::vector<CommandMatch> CommandPalette::Search(const std::string& query,
                                                 int64_t now_ms) const {
  std::vector<CommandMatch> matches;
  const bool empty_query = SplitWhitespace(query).empty();
  for (auto& entry : GetVisibleCommands()) {
    int score = 0;
    if (empty_query) {
      score = 1 + FrecencyBoost(entry.usage_count, entry.last_used_ms, now_ms);
    } else {
      score = ScoreEntry(entry, query, now_ms);
    }
    if (score > 0)
      matches.push_back({std::move(entry), score});
  }
  // Stable tiebreak: GetVisibleCommands() is name-sorted, stable_sort keeps
  // that order among equal scores.
  std::stable_sort(matches.begin(), matches.end(),
                   [](const CommandMatch& a, const CommandMatch& b) {
                     return a.score > b.score;
                   });
  return matches;
}

std::vector<CommandMatch> CommandPalette::Search(
    const std::string& query) const {
  return Search(query, NowMs());
}

std::vector<CommandEntry> CommandPalette::SearchCommands(
    const std::string& query) {
  std::vector<CommandEntry> results;
  for (auto& match : Search(query)) {
    results.push_back(std::move(match.entry));
  }
  return results;
}

std::vector<CommandEntry> CommandPalette::GetRecentCommands(int limit) {
  std::vector<CommandEntry> recent;
  for (auto& entry : GetVisibleCommands()) {
    if (entry.usage_count > 0)
      recent.push_back(std::move(entry));
  }
  std::stable_sort(recent.begin(), recent.end(),
                   [](const CommandEntry& a, const CommandEntry& b) {
                     return a.last_used_ms > b.last_used_ms;
                   });
  if (recent.size() > static_cast<size_t>(limit))
    recent.resize(limit);
  return recent;
}

std::vector<CommandEntry> CommandPalette::GetFrequentCommands(int limit) {
  std::vector<CommandEntry> frequent;
  for (auto& entry : GetVisibleCommands()) {
    if (entry.usage_count > 0)
      frequent.push_back(std::move(entry));
  }
  std::stable_sort(frequent.begin(), frequent.end(),
                   [](const CommandEntry& a, const CommandEntry& b) {
                     return a.usage_count > b.usage_count;
                   });
  if (frequent.size() > static_cast<size_t>(limit))
    frequent.resize(limit);
  return frequent;
}

void CommandPalette::SaveHistory(const std::string& filepath) {
  try {
    yaze::Json j;
    j["version"] = 1;
    j["commands"] = yaze::Json::object();

    for (const auto& [name, stats] : usage_) {
      if (stats.count > 0) {
        yaze::Json cmd;
        cmd["usage_count"] = stats.count;
        cmd["last_used_ms"] = stats.last_used_ms;
        j["commands"][name] = cmd;
      }
    }

    std::ofstream file(filepath);
    if (file.is_open()) {
      file << j.dump(2);
      LOG_DEBUG("CommandPalette", "Saved command history to %s",
                filepath.c_str());
    }
  } catch (const std::exception& e) {
    LOG_ERROR("CommandPalette", "Failed to save command history: %s", e.what());
  }
}

void CommandPalette::LoadHistory(const std::string& filepath) {
  if (!std::filesystem::exists(filepath)) {
    return;
  }

  try {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      return;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    yaze::Json j = yaze::Json::parse(content);

    if (!j.contains("commands") || !j["commands"].is_object()) {
      return;
    }

    int loaded = 0;
    for (auto& [name, cmd_json] : j["commands"].items()) {
      UsageStats stats;
      stats.count = cmd_json.value("usage_count", 0);
      stats.last_used_ms = cmd_json.value("last_used_ms", int64_t{0});
      if (stats.count <= 0)
        continue;
      usage_[name] = stats;
      ++generation_;
      if (auto it = commands_.find(name); it != commands_.end()) {
        it->second.usage_count = stats.count;
        it->second.last_used_ms = stats.last_used_ms;
      }
      loaded++;
    }

    LOG_INFO("CommandPalette", "Loaded %d command history entries from %s",
             loaded, filepath.c_str());
  } catch (const std::exception& e) {
    LOG_ERROR("CommandPalette", "Failed to load command history: %s", e.what());
  }
}

std::vector<CommandEntry> CommandPalette::GetAllCommands() const {
  std::vector<CommandEntry> result;
  result.reserve(commands_.size());
  for (const auto& [name, entry] : commands_) {
    result.push_back(entry);
  }
  return result;
}

void CommandPalette::RegisterPanelCommands(
    WorkspaceWindowManager* window_manager, size_t session_id) {
  if (!window_manager)
    return;

  for (const auto& base_id : window_manager->GetWindowsInSession(session_id)) {
    const auto* descriptor =
        window_manager->GetWindowDescriptor(session_id, base_id);
    if (!descriptor) {
      continue;
    }

    // Create show command
    std::string show_name =
        absl::StrFormat("Show: %s", descriptor->display_name);
    std::string show_desc =
        absl::StrFormat("Open the %s window", descriptor->display_name);

    AddCommand(show_name, CommandCategory::kPanel, show_desc,
               descriptor->shortcut_hint,
               [window_manager, base_id, session_id]() {
                 window_manager->OpenWindow(session_id, base_id);
               });

    // Create hide command
    std::string hide_name =
        absl::StrFormat("Hide: %s", descriptor->display_name);
    std::string hide_desc =
        absl::StrFormat("Close the %s window", descriptor->display_name);

    AddCommand(hide_name, CommandCategory::kPanel, hide_desc, "",
               [window_manager, base_id, session_id]() {
                 window_manager->CloseWindow(session_id, base_id);
               });

    // Create toggle command (legacy name kept for muscle memory / docs)
    std::string toggle_name =
        absl::StrFormat("Toggle: %s", descriptor->display_name);
    std::string toggle_desc = absl::StrFormat("Toggle the %s window visibility",
                                              descriptor->display_name);

    auto toggle_fn = [window_manager, base_id, session_id]() {
      window_manager->ToggleWindow(session_id, base_id);
    };

    AddCommand(toggle_name, CommandCategory::kPanel, toggle_desc, "",
               toggle_fn);

    // Window Finder selects a destination, rather than toggling its visibility.
    // Keep the explicit Toggle command above for open/close actions.
    std::string window_name =
        absl::StrFormat("window: %s", descriptor->display_name);
    AddCommand(
        window_name, CommandCategory::kPanel, show_desc,
        descriptor->shortcut_hint, [window_manager, base_id, session_id]() {
          WindowHost host(window_manager);
          const bool opened = ImGui::GetCurrentContext() != nullptr
                                  ? host.OpenAndFocusWindow(session_id, base_id)
                                  : host.OpenWindow(session_id, base_id);
          if (opened) {
            window_manager->MarkWindowRecentlyUsed(base_id);
          }
        });

    // Pin-to-global toggle. Mirrors the sidebar right-click pin + the panel
    // tab pin UI, so users who live in the command palette never need to
    // reach for the sidebar just to make a panel survive editor switches.
    std::string pin_toggle_name =
        absl::StrFormat("Toggle Pin: %s", descriptor->display_name);
    std::string pin_toggle_desc =
        absl::StrFormat("Keep the %s window visible across editor switches",
                        descriptor->display_name);

    AddCommand(pin_toggle_name, CommandCategory::kPanel, pin_toggle_desc, "",
               [window_manager, base_id, session_id]() {
                 const bool pinned =
                     window_manager->IsWindowPinned(session_id, base_id);
                 window_manager->SetWindowPinned(session_id, base_id, !pinned);
               });
  }
}

void CommandPalette::RegisterDrawerCommands(
    std::function<void(int drawer_type)> toggle_callback,
    std::function<void()> cycle_next, std::function<void()> cycle_prev) {
  if (!toggle_callback)
    return;

  for (const auto& entry : GetDrawerCatalog()) {
    if (!entry.name)
      continue;

    const int type_as_int = static_cast<int>(entry.type);
    std::string name = absl::StrFormat("drawer: %s", entry.name);
    std::string desc =
        absl::StrFormat("Toggle the %s right drawer", entry.name);

    AddCommand(
        name, CommandCategory::kDrawer, desc, /*shortcut=*/"",
        [toggle_callback, type_as_int]() { toggle_callback(type_as_int); });
  }

  if (cycle_next) {
    AddCommand("drawer: Next", CommandCategory::kDrawer,
               "Cycle to the next right drawer", "", std::move(cycle_next));
  }
  if (cycle_prev) {
    AddCommand("drawer: Previous", CommandCategory::kDrawer,
               "Cycle to the previous right drawer", "", std::move(cycle_prev));
  }
}

void CommandPalette::RegisterEditorCommands(
    std::function<void(const std::string&)> switch_callback) {
  // Get all editor categories
  auto categories = EditorRegistry::GetAllEditorCategories();

  for (const auto& category : categories) {
    std::string name = absl::StrFormat("Switch to: %s Editor", category);
    std::string desc =
        absl::StrFormat("Switch to the %s editor category", category);

    AddCommand(name, CommandCategory::kEditor, desc, "",
               [switch_callback, category]() { switch_callback(category); });
  }
}

void CommandPalette::RegisterLayoutCommands(
    std::function<void(const std::string&)> apply_callback) {
  struct ProfileInfo {
    const char* id;
    const char* name;
    const char* description;
  };

  static const ProfileInfo profiles[] = {
      {"code", "Code", "Focused editing workspace with minimal panel noise"},
      {"debug", "Debug",
       "Debugger-first workspace for tracing and memory tools"},
      {"mapping", "Mapping",
       "Map-centric workspace for overworld/dungeon flows"},
      {"chat", "Chat + Agent",
       "Agent collaboration workspace with chat-centric layout"},
  };

  for (const auto& profile : profiles) {
    std::string name = absl::StrFormat("Apply Profile: %s", profile.name);
    auto apply_fn = [apply_callback, profile_id = std::string(profile.id)]() {
      apply_callback("profile:" + profile_id);
    };
    AddCommand(name, CommandCategory::kLayout, profile.description, "",
               apply_fn);
    AddCommand(absl::StrFormat("layout: profile %s", profile.name),
               CommandCategory::kLayout, profile.description, "", apply_fn);
  }

  AddCommand("Capture Layout Snapshot", CommandCategory::kLayout,
             "Capture current layout as temporary session snapshot", "",
             [apply_callback]() { apply_callback("session:capture"); });
  AddCommand("layout: capture snapshot", CommandCategory::kLayout,
             "Capture current layout as temporary session snapshot", "",
             [apply_callback]() { apply_callback("session:capture"); });
  AddCommand("Restore Layout Snapshot", CommandCategory::kLayout,
             "Restore temporary session snapshot", "",
             [apply_callback]() { apply_callback("session:restore"); });
  AddCommand("layout: restore snapshot", CommandCategory::kLayout,
             "Restore temporary session snapshot", "",
             [apply_callback]() { apply_callback("session:restore"); });
  AddCommand("Clear Layout Snapshot", CommandCategory::kLayout,
             "Clear temporary session snapshot", "",
             [apply_callback]() { apply_callback("session:clear"); });
  AddCommand("layout: clear snapshot", CommandCategory::kLayout,
             "Clear temporary session snapshot", "",
             [apply_callback]() { apply_callback("session:clear"); });

  // Legacy named workspace presets
  struct PresetInfo {
    const char* name;
    const char* description;
  };

  static const PresetInfo presets[] = {
      {"Minimal", "Minimal workspace with essential panels only"},
      {"Developer", "Debug-focused layout with emulator and memory tools"},
      {"Designer", "Visual-focused layout for graphics and palette editing"},
      {"Modder", "Full-featured layout with all panels available"},
      {"Overworld Expert", "Optimized layout for overworld editing"},
      {"Dungeon Expert", "Optimized layout for dungeon editing"},
      {"Testing", "QA-focused layout with testing tools"},
      {"Audio", "Music and sound editing focused layout"},
      {"Logic Debugger", "Debug and development focused layout"},
      {"Overworld Artist", "Visual and overworld focused layout"},
      {"Dungeon Master", "Comprehensive dungeon editing layout"},
      {"Audio Engineer", "Music and sound editing layout"},
  };

  for (const auto& preset : presets) {
    std::string name = absl::StrFormat("Apply Layout: %s", preset.name);
    auto apply_fn = [apply_callback, preset_name = std::string(preset.name)]() {
      apply_callback(preset_name);
    };

    AddCommand(name, CommandCategory::kLayout, preset.description, "",
               apply_fn);
    AddCommand(absl::StrFormat("layout: %s", preset.name),
               CommandCategory::kLayout, preset.description, "", apply_fn);
  }

  // Reset to default layout command
  AddCommand("Reset Layout: Default", CommandCategory::kLayout,
             "Reset to the default layout for current editor", "",
             [apply_callback]() { apply_callback("Default"); });
  AddCommand("layout: Default", CommandCategory::kLayout,
             "Reset to the default layout for current editor", "",
             [apply_callback]() { apply_callback("Default"); });
}

void CommandPalette::RegisterRecentFilesCommands(
    std::function<void(const std::string&)> open_callback) {
  const auto& recent_files =
      project::RecentFilesManager::GetInstance().GetRecentFiles();

  for (const auto& filepath : recent_files) {
    // Skip files that no longer exist
    if (!std::filesystem::exists(filepath)) {
      continue;
    }

    // Extract just the filename for display
    std::filesystem::path path(filepath);
    std::string filename = path.filename().string();

    std::string name = absl::StrFormat("Open Recent: %s", filename);
    std::string desc = absl::StrFormat("Open file %s", filepath);

    AddCommand(name, CommandCategory::kFile, desc, "",
               [open_callback, filepath]() { open_callback(filepath); });
  }
}

void CommandPalette::RegisterOverworldMapCommands(size_t session_id) {
  for (int map_id = 0; map_id < zelda3::kNumOverworldMaps; ++map_id) {
    const std::string label = zelda3::GetOverworldMapLabel(map_id);
    const std::string map_name =
        label.empty() ? absl::StrFormat("Map %02X", map_id) : label;
    const std::string name =
        absl::StrFormat("Overworld: Open Map [%02X] %s", map_id, map_name);
    const std::string desc =
        absl::StrFormat("Jump to overworld map %02X", map_id);
    AddCommand(
        name, CommandCategory::kNavigation, desc, "", [map_id, session_id]() {
          if (auto* bus = ContentRegistry::Context::event_bus()) {
            bus->Publish(JumpToMapRequestEvent::Create(map_id, session_id));
          }
        });
  }
}

void CommandPalette::RegisterDungeonRoomCommands(size_t session_id) {
  constexpr int kTotalRooms = 0x128;
  for (int room_id = 0; room_id < kTotalRooms; ++room_id) {
    const std::string label = zelda3::GetRoomLabel(room_id);
    const std::string room_name =
        label.empty() ? absl::StrFormat("Room %03X", room_id) : label;

    const std::string name =
        absl::StrFormat("Dungeon: Open Room [%03X] %s", room_id, room_name);
    const std::string desc =
        absl::StrFormat("Jump to dungeon room %03X", room_id);

    AddCommand(
        name, CommandCategory::kNavigation, desc, "", [room_id, session_id]() {
          if (auto* bus = ContentRegistry::Context::event_bus()) {
            bus->Publish(JumpToRoomRequestEvent::Create(room_id, session_id));
          }
        });
  }
}

void CommandPalette::RegisterWorkflowCommands(
    WorkspaceWindowManager* window_manager, size_t session_id) {
  if (window_manager) {
    const auto categories = window_manager->GetAllCategories(session_id);
    for (const auto& category : categories) {
      for (const auto& descriptor :
           window_manager->GetWindowsInCategory(session_id, category)) {
        if (descriptor.workflow_group.empty()) {
          continue;
        }
        if (descriptor.enabled_condition && !descriptor.enabled_condition()) {
          continue;
        }
        const std::string label = descriptor.workflow_label.empty()
                                      ? descriptor.display_name
                                      : descriptor.workflow_label;
        const std::string group = descriptor.workflow_group.empty()
                                      ? std::string("General")
                                      : descriptor.workflow_group;
        const std::string description =
            descriptor.workflow_description.empty()
                ? absl::StrFormat("Open %s", descriptor.display_name)
                : descriptor.workflow_description;
        AddCommand(
            absl::StrFormat("%s: %s", group, label), CommandCategory::kWorkflow,
            description, descriptor.shortcut_hint,
            [window_manager, session_id, panel_id = descriptor.card_id]() {
              window_manager->OpenWindow(session_id, panel_id);
            });
      }
    }
  }

  for (const auto& action : ContentRegistry::WorkflowActions::GetAll()) {
    if (action.enabled && !action.enabled()) {
      continue;
    }
    const std::string group =
        action.group.empty() ? std::string("General") : action.group;
    AddCommand(absl::StrFormat("%s: %s", group, action.label),
               CommandCategory::kWorkflow, action.description, action.shortcut,
               action.callback);
  }
}

void CommandPalette::RegisterWelcomeCommands(
    const RecentProjectsModel* model,
    const std::vector<std::string>& template_names,
    std::function<void(const std::string&)> remove_callback,
    std::function<void(const std::string&)> toggle_pin_callback,
    std::function<void()> undo_remove_callback,
    std::function<void()> clear_recents_callback,
    std::function<void(const std::string&)> create_from_template_callback,
    std::function<void()> dismiss_welcome_callback,
    std::function<void()> show_welcome_callback) {
  // Per-entry remove/pin commands. Keeping the palette fully driven by the
  // same model that powers the welcome screen means this list stays in sync
  // with pins/renames automatically on the next RefreshCommands().
  if (model) {
    for (const auto& entry : model->entries()) {
      if (entry.unavailable)
        continue;  // Platform-gated; skip silently.
      const std::string label = entry.display_name_override.empty()
                                    ? entry.name
                                    : entry.display_name_override;
      const std::string path = entry.filepath;

      if (remove_callback) {
        const std::string name =
            absl::StrFormat("Welcome: Remove Recent \"%s\"", label);
        const std::string desc =
            absl::StrFormat("Remove %s from the welcome screen's recents list.",
                            entry.filepath);
        AddCommand(name, CommandCategory::kFile, desc, "",
                   [remove_callback, path]() { remove_callback(path); });
      }
      if (toggle_pin_callback) {
        const std::string name = absl::StrFormat(
            "Welcome: %s Recent \"%s\"", entry.pinned ? "Unpin" : "Pin", label);
        const std::string desc =
            absl::StrFormat("%s %s on the welcome screen.",
                            entry.pinned ? "Unpin" : "Pin", entry.filepath);
        AddCommand(
            name, CommandCategory::kFile, desc, "",
            [toggle_pin_callback, path]() { toggle_pin_callback(path); });
      }
    }
  }

  if (clear_recents_callback) {
    AddCommand("Welcome: Clear Recent Files", CommandCategory::kFile,
               "Forget every entry in the welcome screen's recent list.", "",
               clear_recents_callback);
  }

  if (undo_remove_callback) {
    AddCommand("Welcome: Undo Last Recent Removal", CommandCategory::kFile,
               "Restore the last recent-project entry removed via Forget.", "",
               undo_remove_callback);
  }

  if (create_from_template_callback) {
    for (const auto& template_name : template_names) {
      if (template_name.empty())
        continue;
      const std::string name = absl::StrFormat(
          "Welcome: Create Project from Template: %s", template_name);
      const std::string desc = absl::StrFormat(
          "Start a new project using the \"%s\" template.", template_name);
      AddCommand(name, CommandCategory::kFile, desc, "",
                 [create_from_template_callback, template_name]() {
                   create_from_template_callback(template_name);
                 });
    }
  }

  if (show_welcome_callback) {
    AddCommand("Welcome: Show Welcome Screen", CommandCategory::kView,
               "Bring back the welcome screen if it's been dismissed.", "",
               show_welcome_callback);
  }
  if (dismiss_welcome_callback) {
    AddCommand("Welcome: Dismiss Welcome Screen", CommandCategory::kView,
               "Hide the welcome screen for the rest of this session.", "",
               dismiss_welcome_callback);
  }
}

}  // namespace editor
}  // namespace yaze
